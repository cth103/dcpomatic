/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include "i18n.h"

using std::string;
using std::cout;
using std::max;
using std::stringstream;
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

	/* Run through until we find the first audio (for each stream) and video */

	while (1) {
		int r = av_read_frame (_format_context, &_packet);
		if (r < 0) {
			break;
		}

		int frame_finished;

		AVCodecContext* context = _format_context->streams[_packet.stream_index]->codec;

		if (_packet.stream_index == _video_stream && !_first_video) {
			if (avcodec_decode_video2 (context, _frame, &frame_finished, &_packet) >= 0 && frame_finished) {
				_first_video = frame_time (_video_stream);
			}
		} else {
			for (size_t i = 0; i < _audio_streams.size(); ++i) {
				if (_packet.stream_index == _audio_streams[i]->index (_format_context) && !_audio_streams[i]->first_audio) {
					if (avcodec_decode_audio4 (context, _frame, &frame_finished, &_packet) >= 0 && frame_finished) {
						_audio_streams[i]->first_audio = frame_time (_audio_streams[i]->index (_format_context));
					}
				}
			}
		}

		bool have_all_audio = true;
		size_t i = 0;
		while (i < _audio_streams.size() && have_all_audio) {
			have_all_audio = _audio_streams[i]->first_audio;
			++i;
		}

		av_free_packet (&_packet);
		
		if (_first_video && have_all_audio) {
			break;
		}
	}
}

optional<double>
FFmpegExaminer::frame_time (int stream) const
{
	optional<double> t;
	
	int64_t const bet = av_frame_get_best_effort_timestamp (_frame);
	if (bet != AV_NOPTS_VALUE) {
		t = bet * av_q2d (_format_context->streams[stream]->time_base);
	}

	return t;
}

float
FFmpegExaminer::video_frame_rate () const
{
	AVStream* s = _format_context->streams[_video_stream];

	if (s->avg_frame_rate.num && s->avg_frame_rate.den) {
		return av_q2d (s->avg_frame_rate);
	}

	return av_q2d (s->r_frame_rate);
}

libdcp::Size
FFmpegExaminer::video_size () const
{
	return libdcp::Size (video_codec_context()->width, video_codec_context()->height);
}

/** @return Length (in video frames) according to our content's header */
VideoFrame
FFmpegExaminer::video_length () const
{
	VideoFrame const length = (double (_format_context->duration) / AV_TIME_BASE) * video_frame_rate();
	return max (1, length);
}

string
FFmpegExaminer::audio_stream_name (AVStream* s) const
{
	stringstream n;

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
	stringstream n;

	n << stream_name (s);

	if (n.str().empty()) {
		n << _("unknown");
	}

	return n.str ();
}

string
FFmpegExaminer::stream_name (AVStream* s) const
{
	stringstream n;

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
