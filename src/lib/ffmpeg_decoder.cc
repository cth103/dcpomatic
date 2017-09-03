/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

/** @file  src/ffmpeg_decoder.cc
 *  @brief A decoder using FFmpeg to decode content.
 */

#include "filter.h"
#include "exceptions.h"
#include "image.h"
#include "util.h"
#include "log.h"
#include "ffmpeg_decoder.h"
#include "subtitle_decoder.h"
#include "ffmpeg_audio_stream.h"
#include "ffmpeg_subtitle_stream.h"
#include "video_filter_graph.h"
#include "audio_buffers.h"
#include "ffmpeg_content.h"
#include "raw_image_proxy.h"
#include "video_decoder.h"
#include "film.h"
#include "audio_decoder.h"
#include "compose.hpp"
#include "subtitle_content.h"
#include "audio_content.h"
#include <dcp/subtitle_string.h>
#include <sub/ssa_reader.h>
#include <sub/subtitle.h>
#include <sub/collect.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <vector>
#include <iomanip>
#include <iostream>
#include <stdint.h>

#include "i18n.h"

#define LOG_GENERAL(...) _log->log (String::compose (__VA_ARGS__), LogEntry::TYPE_GENERAL);
#define LOG_ERROR(...) _log->log (String::compose (__VA_ARGS__), LogEntry::TYPE_ERROR);
#define LOG_WARNING_NC(...) _log->log (__VA_ARGS__, LogEntry::TYPE_WARNING);
#define LOG_WARNING(...) _log->log (String::compose (__VA_ARGS__), LogEntry::TYPE_WARNING);

using std::cout;
using std::string;
using std::vector;
using std::list;
using std::min;
using std::pair;
using std::max;
using std::map;
using boost::shared_ptr;
using boost::is_any_of;
using boost::split;
using boost::optional;
using boost::dynamic_pointer_cast;
using dcp::Size;

FFmpegDecoder::FFmpegDecoder (shared_ptr<const FFmpegContent> c, shared_ptr<Log> log, bool fast)
	: FFmpeg (c)
	, _log (log)
	, _have_current_subtitle (false)
{
	if (c->video) {
		video.reset (new VideoDecoder (this, c, log));
		_pts_offset = pts_offset (c->ffmpeg_audio_streams(), c->first_video(), c->active_video_frame_rate());
		/* It doesn't matter what size or pixel format this is, it just needs to be black */
		_black_image.reset (new Image (AV_PIX_FMT_RGB24, dcp::Size (128, 128), true));
		_black_image->make_black ();
	} else {
		_pts_offset = ContentTime ();
	}

	if (c->audio) {
		audio.reset (new AudioDecoder (this, c->audio, log, fast));
	}

	if (c->subtitle) {
		subtitle.reset (new SubtitleDecoder (this, c->subtitle, log));
	}

	_next_time.resize (_format_context->nb_streams);
}

void
FFmpegDecoder::flush ()
{
	/* Get any remaining frames */

	_packet.data = 0;
	_packet.size = 0;

	/* XXX: should we reset _packet.data and size after each *_decode_* call? */

	while (video && decode_video_packet ()) {}

	if (audio) {
		decode_audio_packet ();
	}

	/* Make sure all streams are the same length and round up to the next video frame */

	FrameRateChange const frc = _ffmpeg_content->film()->active_frame_rate_change(_ffmpeg_content->position());
	ContentTime full_length (_ffmpeg_content->full_length(), frc);
	full_length = full_length.ceil (frc.source);
	if (video) {
		double const vfr = _ffmpeg_content->video_frame_rate().get();
		Frame const f = full_length.frames_round (vfr);
		Frame v = video->position().frames_round (vfr);
		while (v < f) {
			video->emit (shared_ptr<const ImageProxy> (new RawImageProxy (_black_image)), v);
			++v;
		}
	}

	BOOST_FOREACH (shared_ptr<FFmpegAudioStream> i, _ffmpeg_content->ffmpeg_audio_streams ()) {
		ContentTime a = audio->stream_position(i);
		while (a < full_length) {
			ContentTime to_do = min (full_length - a, ContentTime::from_seconds (0.1));
			shared_ptr<AudioBuffers> silence (new AudioBuffers (i->channels(), to_do.frames_ceil (i->frame_rate())));
			silence->make_silent ();
			audio->emit (i, silence, a);
			a += to_do;
		}
	}

	if (audio) {
		audio->flush ();
	}
}

bool
FFmpegDecoder::pass ()
{
	int r = av_read_frame (_format_context, &_packet);

	/* AVERROR_INVALIDDATA can apparently be returned sometimes even when av_read_frame
	   has pretty-much succeeded (and hence generated data which should be processed).
	   Hence it makes sense to continue here in that case.
	*/
	if (r < 0 && r != AVERROR_INVALIDDATA) {
		if (r != AVERROR_EOF) {
			/* Maybe we should fail here, but for now we'll just finish off instead */
			char buf[256];
			av_strerror (r, buf, sizeof(buf));
			LOG_ERROR (N_("error on av_read_frame (%1) (%2)"), &buf[0], r);
		}

		flush ();
		return true;
	}

	int const si = _packet.stream_index;
	shared_ptr<const FFmpegContent> fc = _ffmpeg_content;

	if (_video_stream && si == _video_stream.get() && !video->ignore()) {
		decode_video_packet ();
	} else if (fc->subtitle_stream() && fc->subtitle_stream()->uses_index(_format_context, si) && !subtitle->ignore()) {
		decode_subtitle_packet ();
	} else {
		decode_audio_packet ();
	}

	av_packet_unref (&_packet);
	return false;
}

/** @param data pointer to array of pointers to buffers.
 *  Only the first buffer will be used for non-planar data, otherwise there will be one per channel.
 */
shared_ptr<AudioBuffers>
FFmpegDecoder::deinterleave_audio (shared_ptr<FFmpegAudioStream> stream) const
{
	DCPOMATIC_ASSERT (bytes_per_audio_sample (stream));

	int const size = av_samples_get_buffer_size (
		0, stream->stream(_format_context)->codec->channels, _frame->nb_samples, audio_sample_format (stream), 1
		);

	/* Deinterleave and convert to float */

	/* total_samples and frames will be rounded down here, so if there are stray samples at the end
	   of the block that do not form a complete sample or frame they will be dropped.
	*/
	int const total_samples = size / bytes_per_audio_sample (stream);
	int const channels = stream->channels();
	int const frames = total_samples / channels;
	shared_ptr<AudioBuffers> audio (new AudioBuffers (channels, frames));
	float** data = audio->data();

	switch (audio_sample_format (stream)) {
	case AV_SAMPLE_FMT_U8:
	{
		uint8_t* p = reinterpret_cast<uint8_t *> (_frame->data[0]);
		int sample = 0;
		int channel = 0;
		for (int i = 0; i < total_samples; ++i) {
			data[channel][sample] = float(*p++) / (1 << 23);

			++channel;
			if (channel == channels) {
				channel = 0;
				++sample;
			}
		}
	}
	break;

	case AV_SAMPLE_FMT_S16:
	{
		int16_t* p = reinterpret_cast<int16_t *> (_frame->data[0]);
		int sample = 0;
		int channel = 0;
		for (int i = 0; i < total_samples; ++i) {
			data[channel][sample] = float(*p++) / (1 << 15);

			++channel;
			if (channel == channels) {
				channel = 0;
				++sample;
			}
		}
	}
	break;

	case AV_SAMPLE_FMT_S16P:
	{
		int16_t** p = reinterpret_cast<int16_t **> (_frame->data);
		for (int i = 0; i < channels; ++i) {
			for (int j = 0; j < frames; ++j) {
				data[i][j] = static_cast<float>(p[i][j]) / (1 << 15);
			}
		}
	}
	break;

	case AV_SAMPLE_FMT_S32:
	{
		int32_t* p = reinterpret_cast<int32_t *> (_frame->data[0]);
		int sample = 0;
		int channel = 0;
		for (int i = 0; i < total_samples; ++i) {
			data[channel][sample] = static_cast<float>(*p++) / 2147483648;

			++channel;
			if (channel == channels) {
				channel = 0;
				++sample;
			}
		}
	}
	break;

	case AV_SAMPLE_FMT_S32P:
	{
		int32_t** p = reinterpret_cast<int32_t **> (_frame->data);
		for (int i = 0; i < channels; ++i) {
			for (int j = 0; j < frames; ++j) {
				data[i][j] = static_cast<float>(p[i][j]) / 2147483648;
			}
		}
	}
	break;

	case AV_SAMPLE_FMT_FLT:
	{
		float* p = reinterpret_cast<float*> (_frame->data[0]);
		int sample = 0;
		int channel = 0;
		for (int i = 0; i < total_samples; ++i) {
			data[channel][sample] = *p++;

			++channel;
			if (channel == channels) {
				channel = 0;
				++sample;
			}
		}
	}
	break;

	case AV_SAMPLE_FMT_FLTP:
	{
		float** p = reinterpret_cast<float**> (_frame->data);
		/* Sometimes there aren't as many channels in the _frame as in the stream */
		for (int i = 0; i < _frame->channels; ++i) {
			memcpy (data[i], p[i], frames * sizeof(float));
		}
		for (int i = _frame->channels; i < channels; ++i) {
			audio->make_silent (i);
		}
	}
	break;

	default:
		throw DecodeError (String::compose (_("Unrecognised audio sample format (%1)"), static_cast<int> (audio_sample_format (stream))));
	}

	return audio;
}

AVSampleFormat
FFmpegDecoder::audio_sample_format (shared_ptr<FFmpegAudioStream> stream) const
{
	return stream->stream (_format_context)->codec->sample_fmt;
}

int
FFmpegDecoder::bytes_per_audio_sample (shared_ptr<FFmpegAudioStream> stream) const
{
	return av_get_bytes_per_sample (audio_sample_format (stream));
}

void
FFmpegDecoder::seek (ContentTime time, bool accurate)
{
	Decoder::seek (time, accurate);

	/* If we are doing an `accurate' seek, we need to use pre-roll, as
	   we don't really know what the seek will give us.
	*/

	ContentTime pre_roll = accurate ? ContentTime::from_seconds (2) : ContentTime (0);
	time -= pre_roll;

	/* XXX: it seems debatable whether PTS should be used here...
	   http://www.mjbshaw.com/2012/04/seeking-in-ffmpeg-know-your-timestamp.html
	*/

	optional<int> stream;

	if (_video_stream) {
		stream = _video_stream;
	} else {
		shared_ptr<FFmpegAudioStream> s = dynamic_pointer_cast<FFmpegAudioStream> (_ffmpeg_content->audio->stream ());
		if (s) {
			stream = s->index (_format_context);
		}
	}

	DCPOMATIC_ASSERT (stream);

	ContentTime u = time - _pts_offset;
	if (u < ContentTime ()) {
		u = ContentTime ();
	}
	av_seek_frame (
		_format_context,
		stream.get(),
		u.seconds() / av_q2d (_format_context->streams[stream.get()]->time_base),
		AVSEEK_FLAG_BACKWARD
		);

	if (video_codec_context ()) {
		avcodec_flush_buffers (video_codec_context());
	}

	BOOST_FOREACH (shared_ptr<FFmpegAudioStream> i, ffmpeg_content()->ffmpeg_audio_streams()) {
		avcodec_flush_buffers (i->stream(_format_context)->codec);
	}

	if (subtitle_codec_context ()) {
		avcodec_flush_buffers (subtitle_codec_context ());
	}

	_have_current_subtitle = false;
}

void
FFmpegDecoder::decode_audio_packet ()
{
	/* Audio packets can contain multiple frames, so we may have to call avcodec_decode_audio4
	   several times.
	*/

	AVPacket copy_packet = _packet;
	int const stream_index = copy_packet.stream_index;

	/* XXX: inefficient */
	vector<shared_ptr<FFmpegAudioStream> > streams = ffmpeg_content()->ffmpeg_audio_streams ();
	vector<shared_ptr<FFmpegAudioStream> >::const_iterator stream = streams.begin ();
	while (stream != streams.end () && !(*stream)->uses_index (_format_context, stream_index)) {
		++stream;
	}

	if (stream == streams.end ()) {
		/* The packet's stream may not be an audio one; just ignore it in this method if so */
		return;
	}

	while (copy_packet.size > 0) {

		int frame_finished;
		int decode_result = avcodec_decode_audio4 ((*stream)->stream (_format_context)->codec, _frame, &frame_finished, &copy_packet);
		if (decode_result < 0) {
			/* avcodec_decode_audio4 can sometimes return an error even though it has decoded
			   some valid data; for example dca_subframe_footer can return AVERROR_INVALIDDATA
			   if it overreads the auxiliary data.	ffplay carries on if frame_finished is true,
			   even in the face of such an error, so I think we should too.

			   Returning from the method here caused mantis #352.
			*/
			LOG_WARNING ("avcodec_decode_audio4 failed (%1)", decode_result);

			/* Fudge decode_result so that we come out of the while loop when
			   we've processed this data.
			*/
			decode_result = copy_packet.size;
		}

		if (frame_finished) {
			shared_ptr<AudioBuffers> data = deinterleave_audio (*stream);

			ContentTime ct;
			if (_frame->pts == AV_NOPTS_VALUE && _next_time[stream_index]) {
				/* In some streams we see not every frame coming through with a timestamp; for those
				   that have AV_NOPTS_VALUE we need to work out the timestamp ourselves.  This is
				   particularly noticeable with TrueHD streams (see #1111).
				*/
				ct = *_next_time[stream_index];
			} else {
				ct = ContentTime::from_seconds (
					av_frame_get_best_effort_timestamp (_frame) *
					av_q2d ((*stream)->stream (_format_context)->time_base))
					+ _pts_offset;
			}

			_next_time[stream_index] = ct + ContentTime::from_frames(data->frames(), (*stream)->frame_rate());

			if (ct < ContentTime ()) {
				/* Discard audio data that comes before time 0 */
				Frame const remove = min (int64_t (data->frames()), (-ct).frames_ceil(double((*stream)->frame_rate ())));
				data->move (data->frames() - remove, remove, 0);
				data->set_frames (data->frames() - remove);
				ct += ContentTime::from_frames (remove, (*stream)->frame_rate ());
			}

			if (ct < ContentTime()) {
				LOG_WARNING (
					"Crazy timestamp %1 for %2 samples in stream %3 packet pts %4 (ts=%5 tb=%6, off=%7)",
					to_string(ct),
					data->frames(),
					copy_packet.stream_index,
					copy_packet.pts,
					av_frame_get_best_effort_timestamp(_frame),
					av_q2d((*stream)->stream(_format_context)->time_base),
					to_string(_pts_offset)
					);
			}

			/* Give this data provided there is some, and its time is sane */
			if (ct >= ContentTime() && data->frames() > 0) {
				audio->emit (*stream, data, ct);
			}
		}

		copy_packet.data += decode_result;
		copy_packet.size -= decode_result;
	}
}

bool
FFmpegDecoder::decode_video_packet ()
{
	DCPOMATIC_ASSERT (_video_stream);

	int frame_finished;
	if (avcodec_decode_video2 (video_codec_context(), _frame, &frame_finished, &_packet) < 0 || !frame_finished) {
		return false;
	}

	boost::mutex::scoped_lock lm (_filter_graphs_mutex);

	shared_ptr<VideoFilterGraph> graph;

	list<shared_ptr<VideoFilterGraph> >::iterator i = _filter_graphs.begin();
	while (i != _filter_graphs.end() && !(*i)->can_process (dcp::Size (_frame->width, _frame->height), (AVPixelFormat) _frame->format)) {
		++i;
	}

	if (i == _filter_graphs.end ()) {
		graph.reset (new VideoFilterGraph (dcp::Size (_frame->width, _frame->height), (AVPixelFormat) _frame->format));
		graph->setup (_ffmpeg_content->filters ());
		_filter_graphs.push_back (graph);
		LOG_GENERAL (N_("New graph for %1x%2, pixel format %3"), _frame->width, _frame->height, _frame->format);
	} else {
		graph = *i;
	}

	list<pair<shared_ptr<Image>, int64_t> > images = graph->process (_frame);

	for (list<pair<shared_ptr<Image>, int64_t> >::iterator i = images.begin(); i != images.end(); ++i) {

		shared_ptr<Image> image = i->first;

		if (i->second != AV_NOPTS_VALUE) {
			double const pts = i->second * av_q2d (_format_context->streams[_video_stream.get()]->time_base) + _pts_offset.seconds ();
			video->emit (
				shared_ptr<ImageProxy> (new RawImageProxy (image)),
				llrint (pts * _ffmpeg_content->active_video_frame_rate ())
				);
		} else {
			LOG_WARNING_NC ("Dropping frame without PTS");
		}
	}

	return true;
}

void
FFmpegDecoder::decode_subtitle_packet ()
{
	int got_subtitle;
	AVSubtitle sub;
	if (avcodec_decode_subtitle2 (subtitle_codec_context(), &sub, &got_subtitle, &_packet) < 0 || !got_subtitle) {
		return;
	}

	/* Stop any current subtitle, either at the time it was supposed to stop, or now if now is sooner */
	if (_have_current_subtitle) {
		if (_current_subtitle_to) {
			subtitle->emit_stop (min(*_current_subtitle_to, subtitle_period(sub).from + _pts_offset));
		} else {
			subtitle->emit_stop (subtitle_period(sub).from + _pts_offset);
		}
		_have_current_subtitle = false;
	}

	if (sub.num_rects <= 0) {
		/* Nothing new in this subtitle */
		return;
	}

	/* Subtitle PTS (within the source, not taking into account any of the
	   source that we may have chopped off for the DCP).
	*/
	FFmpegSubtitlePeriod sub_period = subtitle_period (sub);
	ContentTime from;
	from = sub_period.from + _pts_offset;
	_have_current_subtitle = true;
	if (sub_period.to) {
		_current_subtitle_to = *sub_period.to + _pts_offset;
	}

	for (unsigned int i = 0; i < sub.num_rects; ++i) {
		AVSubtitleRect const * rect = sub.rects[i];

		switch (rect->type) {
		case SUBTITLE_NONE:
			break;
		case SUBTITLE_BITMAP:
			decode_bitmap_subtitle (rect, from);
			break;
		case SUBTITLE_TEXT:
			cout << "XXX: SUBTITLE_TEXT " << rect->text << "\n";
			break;
		case SUBTITLE_ASS:
			decode_ass_subtitle (rect->ass, from);
			break;
		}
	}

	avsubtitle_free (&sub);
}

void
FFmpegDecoder::decode_bitmap_subtitle (AVSubtitleRect const * rect, ContentTime from)
{
	/* Note RGBA is expressed little-endian, so the first byte in the word is R, second
	   G, third B, fourth A.
	*/
	shared_ptr<Image> image (new Image (AV_PIX_FMT_RGBA, dcp::Size (rect->w, rect->h), true));

#ifdef DCPOMATIC_HAVE_AVSUBTITLERECT_PICT
	/* Start of the first line in the subtitle */
	uint8_t* sub_p = rect->pict.data[0];
	/* sub_p looks up into a BGRA palette which is here
	   (i.e. first byte B, second G, third R, fourth A)
	*/
	uint32_t const * palette = (uint32_t *) rect->pict.data[1];
#else
	/* Start of the first line in the subtitle */
	uint8_t* sub_p = rect->data[0];
	/* sub_p looks up into a BGRA palette which is here
	   (i.e. first byte B, second G, third R, fourth A)
	*/
	uint32_t const * palette = (uint32_t *) rect->data[1];
#endif
	/* And the stream has a map of those palette colours to colours
	   chosen by the user; created a `mapped' palette from those settings.
	*/
	map<RGBA, RGBA> colour_map = ffmpeg_content()->subtitle_stream()->colours ();
	vector<RGBA> mapped_palette (rect->nb_colors);
	for (int i = 0; i < rect->nb_colors; ++i) {
		RGBA c ((palette[i] & 0xff0000) >> 16, (palette[i] & 0xff00) >> 8, palette[i] & 0xff, (palette[i] & 0xff000000) >> 24);
		map<RGBA, RGBA>::const_iterator j = colour_map.find (c);
		if (j != colour_map.end ()) {
			mapped_palette[i] = j->second;
		} else {
			/* This colour was not found in the FFmpegSubtitleStream's colour map; probably because
			   it is from a project that was created before this stuff was added.  Just use the
			   colour straight from the original palette.
			*/
			mapped_palette[i] = c;
		}
	}

	/* Start of the output data */
	uint32_t* out_p = (uint32_t *) image->data()[0];

	for (int y = 0; y < rect->h; ++y) {
		uint8_t* sub_line_p = sub_p;
		uint32_t* out_line_p = out_p;
		for (int x = 0; x < rect->w; ++x) {
			RGBA const p = mapped_palette[*sub_line_p++];
			/* XXX: this seems to be wrong to me (isn't the output image RGBA?) but it looks right on screen */
			*out_line_p++ = (p.a << 24) | (p.r << 16) | (p.g << 8) | p.b;
		}
#ifdef DCPOMATIC_HAVE_AVSUBTITLERECT_PICT
		sub_p += rect->pict.linesize[0];
#else
		sub_p += rect->linesize[0];
#endif
		out_p += image->stride()[0] / sizeof (uint32_t);
	}

	int const target_width = subtitle_codec_context()->width;
	int const target_height = subtitle_codec_context()->height;
	dcpomatic::Rect<double> const scaled_rect (
		static_cast<double> (rect->x) / target_width,
		static_cast<double> (rect->y) / target_height,
		static_cast<double> (rect->w) / target_width,
		static_cast<double> (rect->h) / target_height
		);

	subtitle->emit_image_start (from, image, scaled_rect);
}

void
FFmpegDecoder::decode_ass_subtitle (string ass, ContentTime from)
{
	/* We have no styles and no Format: line, so I'm assuming that FFmpeg
	   produces a single format of Dialogue: lines...
	*/

	vector<string> bits;
	split (bits, ass, is_any_of (","));
	if (bits.size() < 10) {
		return;
	}

	sub::RawSubtitle base;
	list<sub::RawSubtitle> raw = sub::SSAReader::parse_line (
		base,
		bits[9],
		_ffmpeg_content->video->size().width,
		_ffmpeg_content->video->size().height
		);

	BOOST_FOREACH (sub::Subtitle const & i, sub::collect<list<sub::Subtitle> > (raw)) {
		subtitle->emit_text_start (from, i);
	}
}
