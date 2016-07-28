/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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
#include "raw_convert.h"
#include "log.h"
#include "ffmpeg_subtitle_stream.h"
#include "ffmpeg_audio_stream.h"
#include "digester.h"
#include "compose.hpp"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using std::cerr;
using std::vector;
using boost::shared_ptr;
using boost::optional;

boost::mutex FFmpeg::_mutex;
boost::weak_ptr<Log> FFmpeg::_ffmpeg_log;

FFmpeg::FFmpeg (boost::shared_ptr<const FFmpegContent> c)
	: _ffmpeg_content (c)
	, _avio_buffer (0)
	, _avio_buffer_size (4096)
	, _avio_context (0)
	, _format_context (0)
	, _frame (0)
{
	setup_general ();
	setup_decoders ();
}

FFmpeg::~FFmpeg ()
{
	boost::mutex::scoped_lock lm (_mutex);

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		avcodec_close (_format_context->streams[i]->codec);
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
	shared_ptr<Log> log = _ffmpeg_log.lock ();
	if (log) {
		string str (line);
		boost::algorithm::trim (str);
		log->log (String::compose ("FFmpeg: %1", str), LogEntry::TYPE_GENERAL);
	} else {
		cerr << line;
	}
}

void
FFmpeg::setup_general ()
{
	av_register_all ();

	/* This might not work too well in some cases of multiple FFmpeg decoders,
	   but it's probably good enough.
	*/
	_ffmpeg_log = _ffmpeg_content->film()->log ();
	av_log_set_callback (FFmpeg::ffmpeg_log_callback);

	_file_group.set_paths (_ffmpeg_content->paths ());
	_avio_buffer = static_cast<uint8_t*> (wrapped_av_malloc (_avio_buffer_size));
	_avio_context = avio_alloc_context (_avio_buffer, _avio_buffer_size, 0, this, avio_read_wrapper, 0, avio_seek_wrapper);
	_format_context = avformat_alloc_context ();
	_format_context->pb = _avio_context;

	AVDictionary* options = 0;
	/* These durations are in microseconds, and represent how far into the content file
	   we will look for streams.
	*/
	av_dict_set (&options, "analyzeduration", raw_convert<string> (5 * 60 * 1000000).c_str(), 0);
	av_dict_set (&options, "probesize", raw_convert<string> (5 * 60 * 1000000).c_str(), 0);

	if (avformat_open_input (&_format_context, 0, 0, &options) < 0) {
		throw OpenFileError (_ffmpeg_content->path(0).string ());
	}

	if (avformat_find_stream_info (_format_context, 0) < 0) {
		throw DecodeError (_("could not find stream information"));
	}

	/* Find video stream */

	optional<int> video_stream_undefined_frame_rate;

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		AVStream* s = _format_context->streams[i];
		if (s->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
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
		throw DecodeError (N_("could not allocate frame"));
	}
}

void
FFmpeg::setup_decoders ()
{
	boost::mutex::scoped_lock lm (_mutex);

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		AVCodecContext* context = _format_context->streams[i]->codec;

		AVCodec* codec = avcodec_find_decoder (context->codec_id);
		if (codec) {

			AVDictionary* options = 0;
			/* This option disables decoding of DCA frame footers in our patched version
			   of FFmpeg.  I believe these footers are of no use to us, and they can cause
			   problems when FFmpeg fails to decode them (mantis #352).
			*/
			av_dict_set (&options, "disable_footer", "1", 0);
			/* This allows decoding of some DNxHR 444 and HQX files; see
			   https://trac.ffmpeg.org/ticket/5681
			*/
			av_dict_set_int (&options, "strict", FF_COMPLIANCE_EXPERIMENTAL, 0);

			if (avcodec_open2 (context, codec, &options) < 0) {
				throw DecodeError (N_("could not open decoder"));
			}
		}

		/* We are silently ignoring any failures to find suitable decoders here */
	}
}

AVCodecContext *
FFmpeg::video_codec_context () const
{
	if (!_video_stream) {
		return 0;
	}

	return _format_context->streams[_video_stream.get()]->codec;
}

AVCodecContext *
FFmpeg::subtitle_codec_context () const
{
	if (!_ffmpeg_content->subtitle_stream ()) {
		return 0;
	}

	return _ffmpeg_content->subtitle_stream()->stream(_format_context)->codec;
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
FFmpeg::subtitle_period (AVSubtitle const & sub)
{
	ContentTime const packet_time = ContentTime::from_seconds (static_cast<double> (sub.pts) / AV_TIME_BASE);

	if (sub.end_display_time == static_cast<uint32_t> (-1)) {
		/* End time is not known */
		return FFmpegSubtitlePeriod (packet_time + ContentTime::from_seconds (sub.start_display_time / 1e3));
	}

	return FFmpegSubtitlePeriod (
		packet_time + ContentTime::from_seconds (sub.start_display_time / 1e3),
		packet_time + ContentTime::from_seconds (sub.end_display_time / 1e3)
		);
}

string
FFmpeg::subtitle_id (AVSubtitle const & sub)
{
	Digester digester;
	digester.add (sub.pts);
	for (unsigned int i = 0; i < sub.num_rects; ++i) {
		AVSubtitleRect* rect = sub.rects[i];
		digester.add (rect->x);
		digester.add (rect->y);
		digester.add (rect->w);
		digester.add (rect->h);
#ifdef DCPOMATIC_HAVE_AVSUBTITLERECT_PICT
		int const line = rect->pict.linesize[0];
		for (int j = 0; j < rect->h; ++j) {
			digester.add (rect->pict.data[0] + j * line, line);
		}
#else
		int const line = rect->linesize[0];
		for (int j = 0; j < rect->h; ++j) {
			digester.add (rect->data[0] + j * line, line);
		}
#endif
	}
	return digester.get ();
}

/** @return true if sub starts a new image subtitle */
bool
FFmpeg::subtitle_starts_image (AVSubtitle const & sub)
{
	bool image = false;
	bool text = false;

	for (unsigned int i = 0; i < sub.num_rects; ++i) {
		switch (sub.rects[i]->type) {
		case SUBTITLE_BITMAP:
			image = true;
			break;
		case SUBTITLE_TEXT:
		case SUBTITLE_ASS:
			text = true;
			break;
		default:
			break;
		}
	}

	/* We can't cope with mixed image/text in one AVSubtitle */
	DCPOMATIC_ASSERT (!image || !text);

	return image;
}

/** Compute the pts offset to use given a set of audio streams and some video details.
 *  Sometimes these parameters will have just been determined by an Examiner, sometimes
 *  they will have been retrieved from a piece of Content, hence the need for this method
 *  in FFmpeg.
 */
ContentTime
FFmpeg::pts_offset (vector<shared_ptr<FFmpegAudioStream> > audio_streams, optional<ContentTime> first_video, double video_frame_rate) const
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

	ContentTime po = ContentTime::min ();

	if (first_video) {
		po = - first_video.get ();
	}

	BOOST_FOREACH (shared_ptr<FFmpegAudioStream> i, audio_streams) {
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
		ContentTime const fvc = first_video.get() + po;
		po += fvc.round_up (video_frame_rate) - fvc;
	}

	return po;
}
