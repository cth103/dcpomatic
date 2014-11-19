/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include "ffmpeg_examiner.h"
#include "ffmpeg_content.h"
#include "ffmpeg_audio_stream.h"
#include "ffmpeg_subtitle_stream.h"
#include "util.h"
#include "safe_stringstream.h"

#include "i18n.h"

using std::string;
using std::cout;
using std::max;
using boost::shared_ptr;
using boost::optional;

FFmpegExaminer::FFmpegExaminer (shared_ptr<const FFmpegContent> c)
	: FFmpeg (c)
{
	/* Find audio and subtitle streams */

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		AVStream* s = _format_context->streams[i];
		if (s->codec->codec_type == AVMEDIA_TYPE_AUDIO) {

			/* This is a hack; sometimes it seems that _audio_codec_context->channel_layout isn't set up,
			   so bodge it here.  No idea why we should have to do this.
			*/

			if (s->codec->channel_layout == 0) {
				s->codec->channel_layout = av_get_default_channel_layout (s->codec->channels);
			}
			
			_audio_streams.push_back (
				shared_ptr<FFmpegAudioStream> (
					new FFmpegAudioStream (audio_stream_name (s), s->id, s->codec->sample_rate, s->codec->channels)
					)
				);

		} else if (s->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
			_subtitle_streams.push_back (shared_ptr<FFmpegSubtitleStream> (new FFmpegSubtitleStream (subtitle_stream_name (s), s->id)));
		}
	}

	/* Run through until we find:
	 *   - the first video.
	 *   - the first audio for each stream.
	 *   - the subtitle periods for each stream.
	 *
	 * We have to note subtitle periods as otherwise we have no way of knowing
	 * where we should look for subtitles (video and audio are always present,
	 * so they are ok).
	 */
	while (true) {
		int r = av_read_frame (_format_context, &_packet);
		if (r < 0) {
			break;
		}

		AVCodecContext* context = _format_context->streams[_packet.stream_index]->codec;

		if (_packet.stream_index == _video_stream) {
			video_packet (context);
		}
		
		for (size_t i = 0; i < _audio_streams.size(); ++i) {
			if (_audio_streams[i]->uses_index (_format_context, _packet.stream_index)) {
				audio_packet (context, _audio_streams[i]);
			}
		}

		for (size_t i = 0; i < _subtitle_streams.size(); ++i) {
			if (_subtitle_streams[i]->uses_index (_format_context, _packet.stream_index)) {
				subtitle_packet (context, _subtitle_streams[i]);
			}
		}

		av_free_packet (&_packet);
	}
}

void
FFmpegExaminer::video_packet (AVCodecContext* context)
{
	if (_first_video) {
		return;
	}

	int frame_finished;
	if (avcodec_decode_video2 (context, _frame, &frame_finished, &_packet) >= 0 && frame_finished) {
		_first_video = frame_time (_format_context->streams[_video_stream]);
	}
}

void
FFmpegExaminer::audio_packet (AVCodecContext* context, shared_ptr<FFmpegAudioStream> stream)
{
	if (stream->first_audio) {
		return;
	}

	int frame_finished;
	if (avcodec_decode_audio4 (context, _frame, &frame_finished, &_packet) >= 0 && frame_finished) {
		stream->first_audio = frame_time (stream->stream (_format_context));
	}
}

void
FFmpegExaminer::subtitle_packet (AVCodecContext* context, shared_ptr<FFmpegSubtitleStream> stream)
{
	int frame_finished;
	AVSubtitle sub;
	if (avcodec_decode_subtitle2 (context, &sub, &frame_finished, &_packet) >= 0 && frame_finished) {
		ContentTimePeriod const period = subtitle_period (sub);
		if (sub.num_rects == 0 && !stream->periods.empty () && stream->periods.back().to > period.from) {
			/* Finish the last subtitle */
			stream->periods.back().to = period.from;
		} else if (sub.num_rects == 1) {
			stream->periods.push_back (period);
		}
	}
}

optional<ContentTime>
FFmpegExaminer::frame_time (AVStream* s) const
{
	optional<ContentTime> t;
	
	int64_t const bet = av_frame_get_best_effort_timestamp (_frame);
	if (bet != AV_NOPTS_VALUE) {
		t = ContentTime::from_seconds (bet * av_q2d (s->time_base));
	}

	return t;
}

float
FFmpegExaminer::video_frame_rate () const
{
	/* This use of r_frame_rate is debateable; there's a few different
	 * frame rates in the format context, but this one seems to be the most
	 * reliable.
	 */
	return av_q2d (av_stream_get_r_frame_rate (_format_context->streams[_video_stream]));
}

dcp::Size
FFmpegExaminer::video_size () const
{
	return dcp::Size (video_codec_context()->width, video_codec_context()->height);
}

/** @return Length according to our content's header */
ContentTime
FFmpegExaminer::video_length () const
{
	ContentTime const length = ContentTime::from_seconds (double (_format_context->duration) / AV_TIME_BASE);
	return ContentTime (max (ContentTime::Type (1), length.get ()));
}

optional<float>
FFmpegExaminer::sample_aspect_ratio () const
{
	AVRational sar = av_guess_sample_aspect_ratio (_format_context, _format_context->streams[_video_stream], 0);
	if (sar.num == 0) {
		/* I assume this means that we don't know */
		return optional<float> ();
	}
	return float (sar.num) / sar.den;
}

string
FFmpegExaminer::audio_stream_name (AVStream* s) const
{
	SafeStringStream n;

	n << stream_name (s);

	if (!n.str().empty()) {
		n << "; ";
	}

	n << s->codec->channels << " channels";

	return n.str ();
}

string
FFmpegExaminer::subtitle_stream_name (AVStream* s) const
{
	SafeStringStream n;

	n << stream_name (s);

	if (n.str().empty()) {
		n << _("unknown");
	}

	return n.str ();
}

string
FFmpegExaminer::stream_name (AVStream* s) const
{
	SafeStringStream n;

	if (s->metadata) {
		AVDictionaryEntry const * lang = av_dict_get (s->metadata, "language", 0, 0);
		if (lang) {
			n << lang->value;
		}
		
		AVDictionaryEntry const * title = av_dict_get (s->metadata, "title", 0, 0);
		if (title) {
			if (!n.str().empty()) {
				n << " ";
			}
			n << title->value;
		}
	}

	return n.str ();
}
