/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


#include "dcdm_j2k_writer.h"
#include "exceptions.h"
#include <dcp/file.h>
#include <dcp/openjpeg_image.h>
#include <openjpeg.h>
#include <cstring>
#include <string>
#include <vector>


using std::shared_ptr;
using std::string;


static char const comment[] = "SMPTE DCDM X'Y'Z' image as defined in SMPTE ST 428-1";


/** An in-memory OpenJPEG output stream, so that the file can then be written with dcp::File
 *  (which copes with non-ASCII and long paths on Windows).  OpenJPEG seeks back to fill in
 *  TLM markers, so writes go at a position rather than always at the end.
 */
struct MemoryStream
{
	std::vector<uint8_t> data;
	size_t position = 0;
};


static
OPJ_SIZE_T
memory_write(void* buffer, OPJ_SIZE_T bytes, void* user)
{
	auto stream = reinterpret_cast<MemoryStream*>(user);
	auto const end = stream->position + bytes;
	if (end > stream->data.size()) {
		stream->data.resize(end);
	}
	memcpy(stream->data.data() + stream->position, buffer, bytes);
	stream->position = end;
	return bytes;
}


static
OPJ_OFF_T
memory_skip(OPJ_OFF_T bytes, void* user)
{
	auto stream = reinterpret_cast<MemoryStream*>(user);
	auto const target = static_cast<OPJ_OFF_T>(stream->position) + bytes;
	if (target < 0) {
		return -1;
	}
	stream->position = static_cast<size_t>(target);
	if (stream->position > stream->data.size()) {
		stream->data.resize(stream->position);
	}
	return bytes;
}


static
OPJ_BOOL
memory_seek(OPJ_OFF_T position, void* user)
{
	auto stream = reinterpret_cast<MemoryStream*>(user);
	if (position < 0) {
		return OPJ_FALSE;
	}
	stream->position = static_cast<size_t>(position);
	if (stream->position > stream->data.size()) {
		stream->data.resize(stream->position);
	}
	return OPJ_TRUE;
}


void
write_lossless_j2k(boost::filesystem::path const& path, shared_ptr<const dcp::OpenJPEGImage> xyz)
{
	auto const size = xyz->size();

	opj_image_cmptparm_t components[3];
	memset(components, 0, sizeof(components));
	for (auto& c: components) {
		c.dx = 1;
		c.dy = 1;
		c.w = size.width;
		c.h = size.height;
		c.prec = 12;
		c.sgnd = 0;
	}

	auto image = opj_image_create(3, components, OPJ_CLRSPC_UNSPECIFIED);
	if (!image) {
		throw EncodeError("could not create OpenJPEG image");
	}
	image->x0 = 0;
	image->y0 = 0;
	image->x1 = size.width;
	image->y1 = size.height;
	for (int c = 0; c < 3; ++c) {
		memcpy(image->comps[c].data, xyz->data(c), sizeof(OPJ_INT32) * size.width * size.height);
	}

	opj_cparameters_t parameters;
	opj_set_default_encoder_parameters(&parameters);
	/* Lossless: one layer, no rate allocation, reversible 5/3 wavelet (irreversible = 0) */
	parameters.tcp_numlayers = 1;
	parameters.tcp_rates[0] = 0;
	parameters.cp_disto_alloc = 1;
	parameters.irreversible = 0;
	parameters.numresolution = 6;
	parameters.prog_order = OPJ_RPCL;
	parameters.cblockw_init = 128;
	parameters.cblockh_init = 32;
	/* Precinct sizes from the highest resolution down */
	parameters.csty |= 0x01;
	parameters.res_spec = 6;
	for (int i = 0; i < 6; ++i) {
		int const s = i == 5 ? 128 : 256;
		parameters.prcw_init[i] = s;
		parameters.prch_init[i] = s;
	}
	parameters.tp_on = 1;
	parameters.tp_flag = 'R';
	string comment_copy(comment);
	parameters.cp_comment = &comment_copy[0];

	auto codec = opj_create_compress(OPJ_CODEC_J2K);
	if (!codec) {
		opj_image_destroy(image);
		throw EncodeError("could not create OpenJPEG encoder");
	}

	auto cleanup = [&]() {
		opj_destroy_codec(codec);
		opj_image_destroy(image);
	};

	if (!opj_setup_encoder(codec, &parameters, image)) {
		cleanup();
		throw EncodeError("could not set up OpenJPEG encoder");
	}

	char const* options[] = { "TLM=YES", nullptr };
	if (!opj_encoder_set_extra_options(codec, options)) {
		cleanup();
		throw EncodeError("could not enable TLM markers in OpenJPEG");
	}

	MemoryStream memory;
	auto stream = opj_stream_create(OPJ_J2K_STREAM_CHUNK_SIZE, OPJ_FALSE);
	if (!stream) {
		cleanup();
		throw EncodeError("could not create OpenJPEG stream");
	}
	opj_stream_set_user_data(stream, &memory, nullptr);
	opj_stream_set_write_function(stream, memory_write);
	opj_stream_set_skip_function(stream, memory_skip);
	opj_stream_set_seek_function(stream, memory_seek);

	bool const ok = opj_start_compress(codec, image, stream) && opj_encode(codec, stream) && opj_end_compress(codec, stream);
	opj_stream_destroy(stream);
	cleanup();

	if (!ok) {
		throw EncodeError("could not encode lossless JPEG 2000 image: " + path.string());
	}

	dcp::File file(path, "wb");
	if (!file) {
		throw FileError("could not open JPEG 2000 file for writing", path);
	}
	file.checked_write(memory.data.data(), memory.data.size());
	file.close();
}
