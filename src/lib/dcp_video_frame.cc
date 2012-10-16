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
#include "film.h"
#include "dcp_video_frame.h"
#include "lut.h"
#include "config.h"
#include "film_state.h"
#include "options.h"
#include "exceptions.h"
#include "server.h"
#include "util.h"
#include "scaler.h"
#include "image.h"
#include "log.h"
#include "subtitle.h"

using namespace std;
using namespace boost;

/** Construct a DCP video frame.
 *  @param input Input image.
 *  @param out Required size of output, in pixels (including any padding).
 *  @param s Scaler to use.
 *  @param p Number of pixels of padding either side of the image.
 *  @param f Index of the frame within the Film.
 *  @param fps Frames per second of the Film.
 *  @param pp FFmpeg post-processing string to use.
 *  @param clut Colour look-up table to use (see Config::colour_lut_index ())
 *  @param bw J2K bandwidth to use (see Config::j2k_bandwidth ())
 *  @param l Log to write to.
 */
DCPVideoFrame::DCPVideoFrame (
	shared_ptr<Image> yuv, shared_ptr<Subtitle> sub,
	Size out, int p, int subtitle_offset, float subtitle_scale,
	Scaler const * s, int f, float fps, string pp, int clut, int bw, Log* l
	)
	: _input (yuv)
	, _subtitle (sub)
	, _out_size (out)
	, _padding (p)
	, _subtitle_offset (subtitle_offset)
	, _subtitle_scale (subtitle_scale)
	, _scaler (s)
	, _frame (f)
	  /* we round here; not sure if this is right */
	, _frames_per_second (rint (fps))
	, _post_process (pp)
	, _colour_lut_index (clut)
	, _j2k_bandwidth (bw)
	, _log (l)
	, _image (0)
	, _parameters (0)
	, _cinfo (0)
	, _cio (0)
{
	
}

/** Create a libopenjpeg container suitable for our output image */
void
DCPVideoFrame::create_openjpeg_container ()
{
	for (int i = 0; i < 3; ++i) {
		_cmptparm[i].dx = 1;
		_cmptparm[i].dy = 1;
		_cmptparm[i].w = _out_size.width;
		_cmptparm[i].h = _out_size.height;
		_cmptparm[i].x0 = 0;
		_cmptparm[i].y0 = 0;
		_cmptparm[i].prec = 12;
		_cmptparm[i].bpp = 12;
		_cmptparm[i].sgnd = 0;
	}

	_image = opj_image_create (3, &_cmptparm[0], CLRSPC_SRGB);
	if (_image == 0) {
		throw EncodeError ("could not create libopenjpeg image");
	}

	_image->x0 = 0;
	_image->y0 = 0;
	_image->x1 = _out_size.width;
	_image->y1 = _out_size.height;
}

DCPVideoFrame::~DCPVideoFrame ()
{
	if (_image) {
		opj_image_destroy (_image);
	}

	if (_cio) {
		opj_cio_close (_cio);
	}

	if (_cinfo) {
		opj_destroy_compress (_cinfo);
	}

	if (_parameters) {
		free (_parameters->cp_comment);
		free (_parameters->cp_matrice);
	}
	
	delete _parameters;
}

/** J2K-encode this frame on the local host.
 *  @return Encoded data.
 */
shared_ptr<EncodedData>
DCPVideoFrame::encode_locally ()
{
	if (!_post_process.empty ()) {
		_input = _input->post_process (_post_process);
	}
	
	shared_ptr<Image> prepared = _input->scale_and_convert_to_rgb (_out_size, _padding, _scaler);

	if (_subtitle) {
		Rectangle tx = subtitle_transformed_area (
			float (_out_size.width) / _input->size().width,
			float (_out_size.height) / _input->size().height,
			_subtitle->area(), _subtitle_offset, _subtitle_scale
			);

		shared_ptr<Image> im = _subtitle->image()->scale (tx.size(), _scaler);
		prepared->alpha_blend (im, tx.position());
	}

	create_openjpeg_container ();

	struct {
		double r, g, b;
	} s;

	struct {
		double x, y, z;
	} d;

	/* Copy our RGB into the openjpeg container, converting to XYZ in the process */

	int jn = 0;
	for (int y = 0; y < _out_size.height; ++y) {
		uint8_t* p = prepared->data()[0] + y * prepared->stride()[0];
		for (int x = 0; x < _out_size.width; ++x) {

			/* In gamma LUT (converting 8-bit input to 12-bit) */
			s.r = lut_in[_colour_lut_index][*p++ << 4];
			s.g = lut_in[_colour_lut_index][*p++ << 4];
			s.b = lut_in[_colour_lut_index][*p++ << 4];
			
			/* RGB to XYZ Matrix */
			d.x = ((s.r * color_matrix[_colour_lut_index][0][0]) +
			       (s.g * color_matrix[_colour_lut_index][0][1]) +
			       (s.b * color_matrix[_colour_lut_index][0][2]));
			
			d.y = ((s.r * color_matrix[_colour_lut_index][1][0]) +
			       (s.g * color_matrix[_colour_lut_index][1][1]) +
			       (s.b * color_matrix[_colour_lut_index][1][2]));
			
			d.z = ((s.r * color_matrix[_colour_lut_index][2][0]) +
			       (s.g * color_matrix[_colour_lut_index][2][1]) +
			       (s.b * color_matrix[_colour_lut_index][2][2]));
			
			/* DCI companding */
			d.x = d.x * DCI_COEFFICENT * (DCI_LUT_SIZE - 1);
			d.y = d.y * DCI_COEFFICENT * (DCI_LUT_SIZE - 1);
			d.z = d.z * DCI_COEFFICENT * (DCI_LUT_SIZE - 1);
			
			/* Out gamma LUT */
			_image->comps[0].data[jn] = lut_out[LO_DCI][(int) d.x];
			_image->comps[1].data[jn] = lut_out[LO_DCI][(int) d.y];
			_image->comps[2].data[jn] = lut_out[LO_DCI][(int) d.z];

			++jn;
		}
	}

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
	_parameters->cp_comment = strdup ("DVD-o-matic");
	_parameters->cp_cinema = CINEMA2K_24;

	/* 3 components, so use MCT */
	_parameters->tcp_mct = 1;
	
	/* set max image */
	_parameters->max_comp_size = max_comp_size;
	_parameters->tcp_rates[0] = ((float) (3 * _image->comps[0].w * _image->comps[0].h * _image->comps[0].prec)) / (max_cs_len * 8);

	/* get a J2K compressor handle */
	_cinfo = opj_create_compress (CODEC_J2K);

	/* Set event manager to null (openjpeg 1.3 bug) */
	_cinfo->event_mgr = 0;

	/* Setup the encoder parameters using the current image and user parameters */
	opj_setup_encoder (_cinfo, _parameters, _image);

	_cio = opj_cio_open ((opj_common_ptr) _cinfo, 0, 0);

	int const r = opj_encode (_cinfo, _cio, _image, 0);
	if (r == 0) {
		throw EncodeError ("jpeg2000 encoding failed");
	}

	_log->log (String::compose ("Finished locally-encoded frame %1", _frame));
	
	return shared_ptr<EncodedData> (new LocallyEncodedData (_cio->buffer, cio_tell (_cio)));
}

/** Send this frame to a remote server for J2K encoding, then read the result.
 *  @param serv Server to send to.
 *  @return Encoded data.
 */
shared_ptr<EncodedData>
DCPVideoFrame::encode_remotely (ServerDescription const * serv)
{
	asio::io_service io_service;
	asio::ip::tcp::resolver resolver (io_service);
	asio::ip::tcp::resolver::query query (serv->host_name(), boost::lexical_cast<string> (Config::instance()->server_port ()));
	asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve (query);

	shared_ptr<Socket> socket (new Socket);

	socket->connect (*endpoint_iterator, 30);

	stringstream s;
	s << "encode please\n"
	  << "input_width " << _input->size().width << "\n"
	  << "input_height " << _input->size().height << "\n"
	  << "input_pixel_format " << _input->pixel_format() << "\n"
	  << "output_width " << _out_size.width << "\n"
	  << "output_height " << _out_size.height << "\n"
	  << "padding " <<  _padding << "\n"
	  << "subtitle_offset " << _subtitle_offset << "\n"
	  << "subtitle_scale " << _subtitle_scale << "\n"
	  << "scaler " << _scaler->id () << "\n"
	  << "frame " << _frame << "\n"
	  << "frames_per_second " << _frames_per_second << "\n";

	if (!_post_process.empty()) {
		s << "post_process " << _post_process << "\n";
	}
	
	s << "colour_lut " << Config::instance()->colour_lut_index () << "\n"
	  << "j2k_bandwidth " << Config::instance()->j2k_bandwidth () << "\n";

	if (_subtitle) {
		s << "subtitle_x " << _subtitle->position().x << "\n"
		  << "subtitle_y " << _subtitle->position().y << "\n"
		  << "subtitle_width " << _subtitle->image()->size().width << "\n"
		  << "subtitle_height " << _subtitle->image()->size().height << "\n";
	}

	socket->write ((uint8_t *) s.str().c_str(), s.str().length() + 1, 30);

	_input->write_to_socket (socket);
	if (_subtitle) {
		_subtitle->image()->write_to_socket (socket);
	}

	char buffer[32];
	socket->read_indefinite ((uint8_t *) buffer, sizeof (buffer), 30);
	socket->consume (strlen (buffer) + 1);
	shared_ptr<EncodedData> e (new RemotelyEncodedData (atoi (buffer)));

	/* now read the rest */
	socket->read_definite_and_consume (e->data(), e->size(), 30);

	_log->log (String::compose ("Finished remotely-encoded frame %1", _frame));
	
	return e;
}

/** Write this data to a J2K file.
 *  @param opt Options.
 *  @param frame Frame index.
 */
void
EncodedData::write (shared_ptr<const Options> opt, int frame)
{
	string const tmp_j2k = opt->frame_out_path (frame, true);

	FILE* f = fopen (tmp_j2k.c_str (), "wb");
	
	if (!f) {
		throw WriteFileError (tmp_j2k, errno);
	}

	fwrite (_data, 1, _size, f);
	fclose (f);

	string const real_j2k = opt->frame_out_path (frame, false);

	/* Rename the file from foo.j2c.tmp to foo.j2c now that it is complete */
	filesystem::rename (tmp_j2k, real_j2k);

	/* Write a file containing the hash */
	string const hash = real_j2k + ".md5";
	ofstream h (hash.c_str());
	h << md5_digest (_data, _size) << "\n";
	h.close ();
}

/** Send this data to a socket.
 *  @param socket Socket
 */
void
EncodedData::send (shared_ptr<Socket> socket)
{
	stringstream s;
	s << _size;
	socket->write ((uint8_t *) s.str().c_str(), s.str().length() + 1, 30);
	socket->write (_data, _size, 30);
}

/** @param s Size of data in bytes */
RemotelyEncodedData::RemotelyEncodedData (int s)
	: EncodedData (new uint8_t[s], s)
{

}

RemotelyEncodedData::~RemotelyEncodedData ()
{
	delete[] _data;
}
