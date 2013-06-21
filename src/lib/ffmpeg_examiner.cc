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

using std::string;
using std::stringstream;
using boost::shared_ptr;

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
					new FFmpegAudioStream (stream_name (s), i, s->codec->sample_rate, s->codec->channels)
					)
				);

		} else if (s->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
			_subtitle_streams.push_back (shared_ptr<FFmpegSubtitleStream> (new FFmpegSubtitleStream (stream_name (s), i)));
		}
	}

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
	return libdcp::Size (_video_codec_context->width, _video_codec_context->height);
}

/** @return Length (in video frames) according to our content's header */
ContentVideoFrame
FFmpegExaminer::video_length () const
{
	return (double (_format_context->duration) / AV_TIME_BASE) * video_frame_rate();
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

	if (n.str().empty()) {
		n << "unknown";
	}

	return n.str ();
}
