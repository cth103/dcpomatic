/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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


#include "compose.hpp"
#include "cross.h"
#include "dcpomatic_socket.h"
#include "exceptions.h"
#include "ffmpeg_image_proxy.h"
#include "image.h"
#include "util.h"
#include "warnings.h"
#include <dcp/raw_convert.h>
DCPOMATIC_DISABLE_WARNINGS
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}
#include <libxml++/libxml++.h>
DCPOMATIC_ENABLE_WARNINGS
#include <iostream>

#include "i18n.h"


using std::cout;
using std::make_pair;
using std::make_shared;
using std::min;
using std::pair;
using std::shared_ptr;
using std::string;
using boost::optional;
using std::dynamic_pointer_cast;
using dcp::raw_convert;


FFmpegImageProxy::FFmpegImageProxy (boost::filesystem::path path)
	: _data (path)
	, _pos (0)
	, _path (path)
{

}

FFmpegImageProxy::FFmpegImageProxy (dcp::ArrayData data)
	: _data (data)
	, _pos (0)
{

}

FFmpegImageProxy::FFmpegImageProxy (shared_ptr<Socket> socket)
	: _pos (0)
{
	uint32_t const size = socket->read_uint32 ();
	_data = dcp::ArrayData (size);
	socket->read (_data.data(), size);
}

static int
avio_read_wrapper (void* data, uint8_t* buffer, int amount)
{
	return reinterpret_cast<FFmpegImageProxy*>(data)->avio_read (buffer, amount);
}

static int64_t
avio_seek_wrapper (void* data, int64_t offset, int whence)
{
	return reinterpret_cast<FFmpegImageProxy*>(data)->avio_seek (offset, whence);
}

int
FFmpegImageProxy::avio_read (uint8_t* buffer, int const amount)
{
	int const to_do = min(static_cast<int64_t>(amount), static_cast<int64_t>(_data.size()) - _pos);
	if (to_do == 0) {
		return AVERROR_EOF;
	}
	memcpy (buffer, _data.data() + _pos, to_do);
	_pos += to_do;
	return to_do;
}

int64_t
FFmpegImageProxy::avio_seek (int64_t const pos, int whence)
{
	switch (whence) {
	case AVSEEK_SIZE:
		return _data.size();
	case SEEK_CUR:
		_pos += pos;
		break;
	case SEEK_SET:
		_pos = pos;
		break;
	case SEEK_END:
		_pos = _data.size() - pos;
		break;
	}

	return _pos;
}


ImageProxy::Result
FFmpegImageProxy::image (Image::Alignment alignment, optional<dcp::Size>) const
{
	auto constexpr name_for_errors = "FFmpegImageProxy::image";

	boost::mutex::scoped_lock lm (_mutex);

	if (_image) {
		return Result (_image, 0);
	}

	uint8_t* avio_buffer = static_cast<uint8_t*> (wrapped_av_malloc(4096));
	auto avio_context = avio_alloc_context (avio_buffer, 4096, 0, const_cast<FFmpegImageProxy*>(this), avio_read_wrapper, 0, avio_seek_wrapper);
	AVFormatContext* format_context = avformat_alloc_context ();
	format_context->pb = avio_context;

	AVDictionary* options = nullptr;
	/* These durations are in microseconds, and represent how far into the content file
	   we will look for streams.
	*/
	av_dict_set (&options, "analyzeduration", raw_convert<string>(5 * 60 * 1000000).c_str(), 0);
	av_dict_set (&options, "probesize", raw_convert<string>(5 * 60 * 1000000).c_str(), 0);

	int e = avformat_open_input (&format_context, 0, 0, &options);
	if ((e < 0 && e == AVERROR_INVALIDDATA) || (e >= 0 && format_context->probe_score <= 25)) {
		/* Hack to fix loading of .tga files through AVIOContexts (rather then
		   directly from the file).  This code just does enough to allow the
		   probe code to take a hint from "foo.tga" and so try targa format.
		*/
		auto f = av_find_input_format ("image2");
		format_context = avformat_alloc_context ();
		format_context->pb = avio_context;
		format_context->iformat = f;
		e = avformat_open_input (&format_context, "foo.tga", f, &options);
	}
	if (e < 0) {
		if (_path) {
			throw OpenFileError (_path->string(), e, OpenFileError::READ);
		} else {
			boost::throw_exception(DecodeError(String::compose(_("Could not decode image (%1)"), e)));
		}
	}

	int r = avformat_find_stream_info(format_context, 0);
	if (r < 0) {
		throw DecodeError (N_("avcodec_find_stream_info"), name_for_errors, r, *_path);
	}

	DCPOMATIC_ASSERT (format_context->nb_streams == 1);

	auto frame = av_frame_alloc ();
	if (!frame) {
		std::bad_alloc ();
	}

	auto codec = avcodec_find_decoder (format_context->streams[0]->codecpar->codec_id);
	DCPOMATIC_ASSERT (codec);

	auto context = avcodec_alloc_context3 (codec);
	if (!context) {
		throw DecodeError (N_("avcodec_alloc_context3"), name_for_errors, *_path);
	}

	r = avcodec_open2 (context, codec, 0);
	if (r < 0) {
		throw DecodeError (N_("avcodec_open2"), name_for_errors, r, *_path);
	}

	AVPacket packet;
	r = av_read_frame (format_context, &packet);
	if (r < 0) {
		throw DecodeError (N_("av_read_frame"), name_for_errors, r, *_path);
	}

	r = avcodec_send_packet (context, &packet);
	if (r < 0) {
		throw DecodeError (N_("avcodec_send_packet"), name_for_errors, r, *_path);
	}

	r = avcodec_receive_frame (context, frame);
	if (r < 0) {
		throw DecodeError (N_("avcodec_receive_frame"), name_for_errors, r, *_path);
	}

	_image = make_shared<Image>(frame, alignment);

	av_packet_unref (&packet);
	av_frame_free (&frame);
	avcodec_free_context (&context);
	avformat_close_input (&format_context);
	av_free (avio_context->buffer);
	av_free (avio_context);

	return Result (_image, 0);
}


void
FFmpegImageProxy::add_metadata (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text (N_("FFmpeg"));
}

void
FFmpegImageProxy::write_to_socket (shared_ptr<Socket> socket) const
{
	socket->write (_data.size());
	socket->write (_data.data(), _data.size());
}

bool
FFmpegImageProxy::same (shared_ptr<const ImageProxy> other) const
{
	auto mp = dynamic_pointer_cast<const FFmpegImageProxy>(other);
	if (!mp) {
		return false;
	}

	return _data == mp->_data;
}

size_t
FFmpegImageProxy::memory_used () const
{
	size_t m = _data.size();
	if (_image) {
		m += _image->memory_used();
	}
	return m;
}
