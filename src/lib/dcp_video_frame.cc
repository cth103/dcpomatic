/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>
    Taken from code Copyright (C) 2010-2011 Terrence Meiczinger

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file  src/dcp_video_frame.cc
 *  @brief A single frame of video destined for a DCP.
 *
 *  Given an Image and some settings, this class knows how to encode
 *  the image to J2K either on the local host or on a remote server.
 *
 *  Objects of this class are used for the queue that we keep
 *  of images that require encoding.
 */

#include <stdint.h>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <errno.h>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <libdcp/rec709_linearised_gamma_lut.h>
#include <libdcp/srgb_linearised_gamma_lut.h>
#include <libdcp/gamma_lut.h>
#include <libdcp/xyz_frame.h>
#include <libdcp/rgb_xyz.h>
#include <libdcp/colour_matrix.h>
#include "film.h"
#include "dcp_video_frame.h"
#include "config.h"
#include "exceptions.h"
#include "server.h"
#include "util.h"
#include "scaler.h"
#include "image.h"
#include "log.h"

#include "i18n.h"

using std::string;
using std::stringstream;
using std::ofstream;
using std::cout;
using boost::shared_ptr;
using libdcp::Size;

#define DCI_COEFFICENT (48.0 / 52.37)

/** Construct a DCP video frame.
 *  @param input Input image.
 *  @param f Index of the frame within the DCP.
 *  @param bw J2K bandwidth to use (see Config::j2k_bandwidth ())
 *  @param l Log to write to.
 */
DCPVideoFrame::DCPVideoFrame (
	shared_ptr<const Image> image, int f, Eyes eyes, int dcp_fps, int bw, shared_ptr<Log> l
	)
	: _image (image)
	, _frame (f)
	, _eyes (eyes)
	, _frames_per_second (dcp_fps)
	, _j2k_bandwidth (bw)
	, _log (l)
{
	
}

/** J2K-encode this frame on the local host.
 *  @return Encoded data.
 */
shared_ptr<EncodedData>
DCPVideoFrame::encode_locally ()
{
	shared_ptr<libdcp::XYZFrame> xyz = libdcp::rgb_to_xyz (
		_image,
		libdcp::SRGBLinearisedGammaLUT::cache.get (12, 2.4),
		libdcp::GammaLUT::cache.get (16, 1 / 2.6),
		libdcp::colour_matrix::srgb_to_xyz
		);
		
	/* Set the max image and component sizes based on frame_rate */
	int max_cs_len = ((float) _j2k_bandwidth) / 8 / _frames_per_second;
	if (_eyes == EYES_LEFT || _eyes == EYES_RIGHT) {
		/* In 3D we have only half the normal bandwidth per eye */
		max_cs_len /= 2;
	}
	int const max_comp_size = max_cs_len / 1.25;

	/* get a J2K compressor handle */
	opj_cinfo_t* cinfo = opj_create_compress (CODEC_J2K);
	if (cinfo == 0) {
		throw EncodeError (N_("could not create JPEG2000 encoder"));
	}

	/* Set encoding parameters to default values */
	opj_cparameters_t parameters;
	opj_set_default_encoder_parameters (&parameters);

	/* Set default cinema parameters */
	parameters.tile_size_on = false;
	parameters.cp_tdx = 1;
	parameters.cp_tdy = 1;
	
	/* Tile part */
	parameters.tp_flag = 'C';
	parameters.tp_on = 1;
	
	/* Tile and Image shall be at (0,0) */
	parameters.cp_tx0 = 0;
	parameters.cp_ty0 = 0;
	parameters.image_offset_x0 = 0;
	parameters.image_offset_y0 = 0;

	/* Codeblock size = 32x32 */
	parameters.cblockw_init = 32;
	parameters.cblockh_init = 32;
	parameters.csty |= 0x01;
	
	/* The progression order shall be CPRL */
	parameters.prog_order = CPRL;
	
	/* No ROI */
	parameters.roi_compno = -1;
	
	parameters.subsampling_dx = 1;
	parameters.subsampling_dy = 1;
	
	/* 9-7 transform */
	parameters.irreversible = 1;
	
	parameters.tcp_rates[0] = 0;
	parameters.tcp_numlayers++;
	parameters.cp_disto_alloc = 1;
	parameters.cp_rsiz = CINEMA2K;
	parameters.cp_comment = strdup (N_("DCP-o-matic"));
	parameters.cp_cinema = CINEMA2K_24;

	/* 3 components, so use MCT */
	parameters.tcp_mct = 1;
	
	/* set max image */
	parameters.max_comp_size = max_comp_size;
	parameters.tcp_rates[0] = ((float) (3 * xyz->size().width * xyz->size().height * 12)) / (max_cs_len * 8);

	/* Set event manager to null (openjpeg 1.3 bug) */
	cinfo->event_mgr = 0;

	/* Setup the encoder parameters using the current image and user parameters */
	opj_setup_encoder (cinfo, &parameters, xyz->opj_image ());

	opj_cio_t* cio = opj_cio_open ((opj_common_ptr) cinfo, 0, 0);
	if (cio == 0) {
		opj_destroy_compress (cinfo);
		throw EncodeError (N_("could not open JPEG2000 stream"));
	}

	int const r = opj_encode (cinfo, cio, xyz->opj_image(), 0);
	if (r == 0) {
		opj_cio_close (cio);
		opj_destroy_compress (cinfo);
		throw EncodeError (N_("JPEG2000 encoding failed"));
	}

	_log->log (String::compose (N_("Finished locally-encoded frame %1"), _frame));

	shared_ptr<EncodedData> enc (new LocallyEncodedData (cio->buffer, cio_tell (cio)));

	opj_cio_close (cio);
	free (parameters.cp_comment);
	opj_destroy_compress (cinfo);

	return enc;
}

/** Send this frame to a remote server for J2K encoding, then read the result.
 *  @param serv Server to send to.
 *  @return Encoded data.
 */
shared_ptr<EncodedData>
DCPVideoFrame::encode_remotely (ServerDescription const * serv)
{
	boost::asio::io_service io_service;
	boost::asio::ip::tcp::resolver resolver (io_service);
	boost::asio::ip::tcp::resolver::query query (serv->host_name(), boost::lexical_cast<string> (Config::instance()->server_port ()));
	boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve (query);

	shared_ptr<Socket> socket (new Socket);

	socket->connect (*endpoint_iterator);

	stringstream s;
	s << "encode please\n"
	  << "width " << _image->size().width << "\n"
	  << "height " << _image->size().height << "\n"
	  << "eyes " << static_cast<int> (_eyes) << "\n"
	  << "frame " << _frame << "\n"
	  << "frames_per_second " << _frames_per_second << "\n"
	  << "j2k_bandwidth " << _j2k_bandwidth << "\n";

	_log->log (String::compose (
			   N_("Sending to remote; pixel format %1, components %2, lines (%3,%4,%5), line sizes (%6,%7,%8)"),
			   _image->pixel_format(), _image->components(),
			   _image->lines(0), _image->lines(1), _image->lines(2),
			   _image->line_size()[0], _image->line_size()[1], _image->line_size()[2]
			   ));

	socket->write (s.str().length() + 1);
	socket->write ((uint8_t *) s.str().c_str(), s.str().length() + 1);

	_image->write_to_socket (socket);

	shared_ptr<EncodedData> e (new RemotelyEncodedData (socket->read_uint32 ()));
	socket->read (e->data(), e->size());

	_log->log (String::compose (N_("Finished remotely-encoded frame %1"), _frame));
	
	return e;
}

EncodedData::EncodedData (int s)
	: _data (new uint8_t[s])
	, _size (s)
{

}

EncodedData::EncodedData (string file)
{
	_size = boost::filesystem::file_size (file);
	_data = new uint8_t[_size];

	FILE* f = fopen (file.c_str(), N_("rb"));
	if (!f) {
		throw FileError (_("could not open file for reading"), file);
	}
	
	size_t const r = fread (_data, 1, _size, f);
	if (r != size_t (_size)) {
		fclose (f);
		throw FileError (_("could not read encoded data"), file);
	}
		
	fclose (f);
}


EncodedData::~EncodedData ()
{
	delete[] _data;
}

/** Write this data to a J2K file.
 *  @param Film Film.
 *  @param frame DCP frame index.
 */
void
EncodedData::write (shared_ptr<const Film> film, int frame, Eyes eyes) const
{
	string const tmp_j2c = film->j2c_path (frame, eyes, true);

	FILE* f = fopen (tmp_j2c.c_str (), N_("wb"));
	
	if (!f) {
		throw WriteFileError (tmp_j2c, errno);
	}

	fwrite (_data, 1, _size, f);
	fclose (f);

	string const real_j2c = film->j2c_path (frame, eyes, false);

	/* Rename the file from foo.j2c.tmp to foo.j2c now that it is complete */
	boost::filesystem::rename (tmp_j2c, real_j2c);
}

void
EncodedData::write_info (shared_ptr<const Film> film, int frame, Eyes eyes, libdcp::FrameInfo fin) const
{
	string const info = film->info_path (frame, eyes);
	ofstream h (info.c_str());
	fin.write (h);
}

/** Send this data to a socket.
 *  @param socket Socket
 */
void
EncodedData::send (shared_ptr<Socket> socket)
{
	socket->write (_size);
	socket->write (_data, _size);
}

LocallyEncodedData::LocallyEncodedData (uint8_t* d, int s)
	: EncodedData (s)
{
	memcpy (_data, d, s);
}

/** @param s Size of data in bytes */
RemotelyEncodedData::RemotelyEncodedData (int s)
	: EncodedData (s)
{

}
