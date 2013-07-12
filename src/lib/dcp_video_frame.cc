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
 *  @param clut Colour look-up table to use (see Config::colour_lut_index ())
 *  @param bw J2K bandwidth to use (see Config::j2k_bandwidth ())
 *  @param l Log to write to.
 */
DCPVideoFrame::DCPVideoFrame (
	shared_ptr<const Image> image, int f, int dcp_fps, int clut, int bw, shared_ptr<Log> l
	)
	: _image (image)
	, _frame (f)
	, _frames_per_second (dcp_fps)
	, _colour_lut (clut)
	, _j2k_bandwidth (bw)
	, _log (l)
	, _parameters (0)
	, _cinfo (0)
	, _cio (0)
{
	
}

DCPVideoFrame::~DCPVideoFrame ()
{
	if (_cio) {
		opj_cio_close (_cio);
	}

	if (_cinfo) {
		opj_destroy_compress (_cinfo);
	}

	if (_parameters) {
		free (_parameters->cp_comment);
	}
	
	delete _parameters;
}

/** J2K-encode this frame on the local host.
 *  @return Encoded data.
 */
shared_ptr<EncodedData>
DCPVideoFrame::encode_locally ()
{
	/* In sRGB / Rec709 gamma LUT */
	shared_ptr<libdcp::LUT> lut_in;
	if (_colour_lut == 0) {
		lut_in = libdcp::SRGBLinearisedGammaLUT::cache.get (12, 2.4);
	} else {
		lut_in = libdcp::Rec709LinearisedGammaLUT::cache.get (12, 1 / 0.45);
	}

	/* Out DCI gamma LUT */
	shared_ptr<libdcp::LUT> lut_out = libdcp::GammaLUT::cache.get (16, 1 / 2.6);

	shared_ptr<libdcp::XYZFrame> xyz = libdcp::rgb_to_xyz (
		_image, lut_in, lut_out, _colour_lut == 0 ? libdcp::colour_matrix::srgb_to_xyz : libdcp::colour_matrix::rec709_to_xyz
		);
		
	/* Set the max image and component sizes based on frame_rate */
	int const max_cs_len = ((float) _j2k_bandwidth) / 8 / _frames_per_second;
	int const max_comp_size = max_cs_len / 1.25;

	/* Set encoding parameters to default values */
	_parameters = new opj_cparameters_t;
	opj_set_default_encoder_parameters (_parameters);

	/* Set default cinema parameters */
	_parameters->tile_size_on = false;
	_parameters->cp_tdx = 1;
	_parameters->cp_tdy = 1;
	
	/* Tile part */
	_parameters->tp_flag = 'C';
	_parameters->tp_on = 1;
	
	/* Tile and Image shall be at (0,0) */
	_parameters->cp_tx0 = 0;
	_parameters->cp_ty0 = 0;
	_parameters->image_offset_x0 = 0;
	_parameters->image_offset_y0 = 0;

	/* Codeblock size = 32x32 */
	_parameters->cblockw_init = 32;
	_parameters->cblockh_init = 32;
	_parameters->csty |= 0x01;
	
	/* The progression order shall be CPRL */
	_parameters->prog_order = CPRL;
	
	/* No ROI */
	_parameters->roi_compno = -1;
	
	_parameters->subsampling_dx = 1;
	_parameters->subsampling_dy = 1;
	
	/* 9-7 transform */
	_parameters->irreversible = 1;
	
	_parameters->tcp_rates[0] = 0;
	_parameters->tcp_numlayers++;
	_parameters->cp_disto_alloc = 1;
	_parameters->cp_rsiz = CINEMA2K;
	_parameters->cp_comment = strdup (N_("DCP-o-matic"));
	_parameters->cp_cinema = CINEMA2K_24;

	/* 3 components, so use MCT */
	_parameters->tcp_mct = 1;
	
	/* set max image */
	_parameters->max_comp_size = max_comp_size;
	_parameters->tcp_rates[0] = ((float) (3 * xyz->size().width * xyz->size().height * 12)) / (max_cs_len * 8);

	/* get a J2K compressor handle */
	_cinfo = opj_create_compress (CODEC_J2K);
	if (_cinfo == 0) {
		throw EncodeError (N_("could not create JPEG2000 encoder"));
	}

	/* Set event manager to null (openjpeg 1.3 bug) */
	_cinfo->event_mgr = 0;

	/* Setup the encoder parameters using the current image and user parameters */
	opj_setup_encoder (_cinfo, _parameters, xyz->opj_image ());

	_cio = opj_cio_open ((opj_common_ptr) _cinfo, 0, 0);
	if (_cio == 0) {
		throw EncodeError (N_("could not open JPEG2000 stream"));
	}

	int const r = opj_encode (_cinfo, _cio, xyz->opj_image(), 0);
	if (r == 0) {
		throw EncodeError (N_("JPEG2000 encoding failed"));
	}

	_log->log (String::compose (N_("Finished locally-encoded frame %1"), _frame));
	
	return shared_ptr<EncodedData> (new LocallyEncodedData (_cio->buffer, cio_tell (_cio)));
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
	s << N_("encode please\n")
	  << N_("width ") << _image->size().width << N_("\n")
	  << N_("height ") << _image->size().height << N_("\n")
	  << N_("frame ") << _frame << N_("\n")
	  << N_("frames_per_second ") << _frames_per_second << N_("\n")
	  << N_("colour_lut ") << _colour_lut << N_("\n")
	  << N_("j2k_bandwidth ") << _j2k_bandwidth << N_("\n");

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
	
	fread (_data, 1, _size, f);
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
EncodedData::write (shared_ptr<const Film> film, int frame) const
{
	string const tmp_j2c = film->j2c_path (frame, true);

	FILE* f = fopen (tmp_j2c.c_str (), N_("wb"));
	
	if (!f) {
		throw WriteFileError (tmp_j2c, errno);
	}

	fwrite (_data, 1, _size, f);
	fclose (f);

	string const real_j2c = film->j2c_path (frame, false);

	/* Rename the file from foo.j2c.tmp to foo.j2c now that it is complete */
	boost::filesystem::rename (tmp_j2c, real_j2c);
}

void
EncodedData::write_info (shared_ptr<const Film> film, int frame, libdcp::FrameInfo fin) const
{
	string const info = film->info_path (frame);
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
