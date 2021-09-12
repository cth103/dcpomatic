/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "ffmpeg.h"
#include "ffmpeg_content.h"
#include "film.h"
#include "exceptions.h"
#include "util.h"
#include "log.h"
#include "dcpomatic_log.h"
#include "ffmpeg_subtitle_stream.h"
#include "ffmpeg_audio_stream.h"
#include "digester.h"
#include "compose.hpp"
#include "config.h"
#include <dcp/raw_convert.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
#include <boost/algorithm/string.hpp>
#include <iostream>

#include "i18n.h"


using std::string;
using std::cout;
using std::cerr;
using std::vector;
using std::shared_ptr;
using boost::optional;
using dcp::raw_convert;
using namespace dcpomatic;


boost::mutex FFmpeg::_mutex;


FFmpeg::FFmpeg (std::shared_ptr<const FFmpegContent> c)
	: _ffmpeg_content (c)
{
	setup_general ();
	setup_decoders ();
}


FFmpeg::~FFmpeg ()
{
	boost::mutex::scoped_lock lm (_mutex);

	for (auto& i: _codec_context) {
		avcodec_free_context (&i);
	}

	av_frame_free (&_frame);
	avformat_close_input (&_format_context);
}


static int
avio_read_wrapper (void* data, uint8_t* buffer, int amount)
{
	return reinterpret_cast<FFmpeg*>(data)->avio_read (buffer, amount);
}


static int64_t
avio_seek_wrapper (void* data, int64_t offset, int whence)
{
	return reinterpret_cast<FFmpeg*>(data)->avio_seek (offset, whence);
}


void
FFmpeg::ffmpeg_log_callback (void* ptr, int level, const char* fmt, va_list vl)
{
	if (level > AV_LOG_WARNING) {
		return;
	}

	char line[1024];
	static int prefix = 0;
	av_log_format_line (ptr, level, fmt, vl, line, sizeof (line), &prefix);
	string str (line);
	boost::algorithm::trim (str);
	dcpomatic_log->log (String::compose ("FFmpeg: %1", str), LogEntry::TYPE_GENERAL);
}


void
FFmpeg::setup_general ()
{
	/* This might not work too well in some cases of multiple FFmpeg decoders,
	   but it's probably good enough.
	*/
	av_log_set_callback (FFmpeg::ffmpeg_log_callback);

	_file_group.set_paths (_ffmpeg_content->paths ());
	_avio_buffer = static_cast<uint8_t*> (wrapped_av_malloc(_avio_buffer_size));
	_avio_context = avio_alloc_context (_avio_buffer, _avio_buffer_size, 0, this, avio_read_wrapper, 0, avio_seek_wrapper);
	if (!_avio_context) {
		throw std::bad_alloc ();
	}
	_format_context = avformat_alloc_context ();
	if (!_format_context) {
		throw std::bad_alloc ();
	}
	_format_context->pb = _avio_context;

	AVDictionary* options = nullptr;
	int e = avformat_open_input (&_format_context, 0, 0, &options);
	if (e < 0) {
		throw OpenFileError (_ffmpeg_content->path(0).string(), e, OpenFileError::READ);
	}

	if (avformat_find_stream_info (_format_context, 0) < 0) {
		throw DecodeError (_("could not find stream information"));
	}

	/* Find video stream */

	optional<int> video_stream_undefined_frame_rate;

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		auto s = _format_context->streams[i];
		if (s->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && avcodec_find_decoder(s->codecpar->codec_id)) {
			if (s->avg_frame_rate.num > 0 && s->avg_frame_rate.den > 0) {
				/* This is definitely our video stream */
				_video_stream = i;
			} else {
				/* This is our video stream if we don't get a better offer */
				video_stream_undefined_frame_rate = i;
			}
		}
	}

	/* Files from iTunes sometimes have two video streams, one with the avg_frame_rate.num and .den set
	   to zero.  Only use such a stream if there is no alternative.
	*/
	if (!_video_stream && video_stream_undefined_frame_rate) {
		_video_stream = video_stream_undefined_frame_rate.get();
	}

	/* Ignore video streams with crazy frame rates.  These are usually things like album art on MP3s. */
	if (_video_stream && av_q2d(av_guess_frame_rate(_format_context, _format_context->streams[_video_stream.get()], 0)) > 1000) {
		_video_stream = optional<int>();
	}

	/* Hack: if the AVStreams have duplicate IDs, replace them with our
	   own.  We use the IDs so that we can cope with VOBs, in which streams
	   move about in index but remain with the same ID in different
	   VOBs.  However, some files have duplicate IDs, hence this hack.
	*/

	bool duplicates = false;
	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		for (uint32_t j = i + 1; j < _format_context->nb_streams; ++j) {
			if (_format_context->streams[i]->id == _format_context->streams[j]->id) {
				duplicates = true;
			}
		}
	}

	if (duplicates) {
		/* Put in our own IDs */
		for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
			_format_context->streams[i]->id = i;
		}
	}

	_frame = av_frame_alloc ();
	if (_frame == 0) {
		throw std::bad_alloc ();
	}
}


void
FFmpeg::setup_decoders ()
{
	boost::mutex::scoped_lock lm (_mutex);

	_codec_context.resize (_format_context->nb_streams);
	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		auto codec = avcodec_find_decoder (_format_context->streams[i]->codecpar->codec_id);
		if (codec) {
			auto context = avcodec_alloc_context3 (codec);
			if (!context) {
				throw std::bad_alloc ();
			}
			_codec_context[i] = context;

			int r = avcodec_parameters_to_context (context, _format_context->streams[i]->codecpar);
			if (r < 0) {
				throw DecodeError ("avcodec_parameters_to_context", "FFmpeg::setup_decoders", r);
			}

			context->thread_count = 8;
			context->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;

			AVDictionary* options = nullptr;
			/* This option disables decoding of DCA frame footers in our patched version
			   of FFmpeg.  I believe these footers are of no use to us, and they can cause
			   problems when FFmpeg fails to decode them (mantis #352).
			*/
			av_dict_set (&options, "disable_footer", "1", 0);
			/* This allows decoding of some DNxHR 444 and HQX files; see
			   https://trac.ffmpeg.org/ticket/5681
			*/
			av_dict_set_int (&options, "strict", FF_COMPLIANCE_EXPERIMENTAL, 0);
			/* Enable following of links in files */
			av_dict_set_int (&options, "enable_drefs", 1, 0);

			r = avcodec_open2 (context, codec, &options);
			if (r < 0) {
				throw DecodeError (N_("avcodec_open2"), N_("FFmpeg::setup_decoders"), r);
			}
		} else {
			dcpomatic_log->log (String::compose ("No codec found for stream %1", i), LogEntry::TYPE_WARNING);
		}
	}
}


AVCodecContext *
FFmpeg::video_codec_context () const
{
	if (!_video_stream) {
		return nullptr;
	}

	return _codec_context[_video_stream.get()];
}


AVCodecContext *
FFmpeg::subtitle_codec_context () const
{
	auto str = _ffmpeg_content->subtitle_stream();
	if (!str) {
		return nullptr;
	}

	return _codec_context[str->index(_format_context)];
}


int
FFmpeg::avio_read (uint8_t* buffer, int const amount)
{
	return _file_group.read (buffer, amount);
}


int64_t
FFmpeg::avio_seek (int64_t const pos, int whence)
{
	if (whence == AVSEEK_SIZE) {
		return _file_group.length ();
	}

	return _file_group.seek (pos, whence);
}


FFmpegSubtitlePeriod
FFmpeg::subtitle_period (AVPacket const* packet, AVStream const* stream, AVSubtitle const & sub)
{
	auto const packet_time = ContentTime::from_seconds (packet->pts * av_q2d(stream->time_base));

	if (sub.end_display_time == 0 || sub.end_display_time == static_cast<uint32_t>(-1)) {
		/* End time is not known */
		return FFmpegSubtitlePeriod (packet_time + ContentTime::from_seconds (sub.start_display_time / 1e3));
	}

	return FFmpegSubtitlePeriod (
		packet_time + ContentTime::from_seconds (sub.start_display_time / 1e3),
		packet_time + ContentTime::from_seconds (sub.end_display_time / 1e3)
		);
}


/** Compute the pts offset to use given a set of audio streams and some video details.
 *  Sometimes these parameters will have just been determined by an Examiner, sometimes
 *  they will have been retrieved from a piece of Content, hence the need for this method
 *  in FFmpeg.
 */
ContentTime
FFmpeg::pts_offset (vector<shared_ptr<FFmpegAudioStream>> audio_streams, optional<ContentTime> first_video, double video_frame_rate) const
{
	/* Audio and video frame PTS values may not start with 0.  We want
	   to fiddle them so that:

	   1.  One of them starts at time 0.
	   2.  The first video PTS value ends up on a frame boundary.

	   Then we remove big initial gaps in PTS and we allow our
	   insertion of black frames to work.

	   We will do:
	     audio_pts_to_use = audio_pts_from_ffmpeg + pts_offset;
	     video_pts_to_use = video_pts_from_ffmpeg + pts_offset;
	*/

	/* First, make one of them start at 0 */

	auto po = ContentTime::min ();

	if (first_video) {
		po = - first_video.get ();
	}

	for (auto i: audio_streams) {
		if (i->first_audio) {
			po = max (po, - i->first_audio.get ());
		}
	}

	/* If the offset is positive we would be pushing things from a -ve PTS to be played.
	   I don't think we ever want to do that, as it seems things at -ve PTS are not meant
	   to be seen (use for alignment bars etc.); see mantis #418.
	*/
	if (po > ContentTime ()) {
		po = ContentTime ();
	}

	/* Now adjust so that the video pts starts on a frame */
	if (first_video) {
		auto const fvc = first_video.get() + po;
		po += fvc.ceil (video_frame_rate) - fvc;
	}

	return po;
}
