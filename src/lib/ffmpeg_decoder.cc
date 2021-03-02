/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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
#include "dcpomatic_log.h"
#include "ffmpeg_decoder.h"
#include "text_decoder.h"
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
#include "text_content.h"
#include "audio_content.h"
#include "frame_interval_checker.h"
#include <dcp/subtitle_string.h>
#include <sub/ssa_reader.h>
#include <sub/subtitle.h>
#include <sub/collect.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <boost/algorithm/string.hpp>
#include <vector>
#include <iomanip>
#include <iostream>
#include <stdint.h>

#include "i18n.h"

using std::cout;
using std::string;
using std::vector;
using std::list;
using std::min;
using std::pair;
using std::max;
using std::map;
using std::shared_ptr;
using std::make_shared;
using boost::is_any_of;
using boost::split;
using boost::optional;
using std::dynamic_pointer_cast;
using dcp::Size;
using namespace dcpomatic;


FFmpegDecoder::FFmpegDecoder (shared_ptr<const Film> film, shared_ptr<const FFmpegContent> c, bool fast)
	: FFmpeg (c)
	, Decoder (film)
	, _have_current_subtitle (false)
{
	if (c->video && c->video->use()) {
		video = make_shared<VideoDecoder>(this, c);
		_pts_offset = pts_offset (c->ffmpeg_audio_streams(), c->first_video(), c->active_video_frame_rate(film));
		/* It doesn't matter what size or pixel format this is, it just needs to be black */
		_black_image.reset (new Image (AV_PIX_FMT_RGB24, dcp::Size (128, 128), true));
		_black_image->make_black ();
	} else {
		_pts_offset = {};
	}

	if (c->audio) {
		audio = make_shared<AudioDecoder>(this, c->audio, fast);
	}

	if (c->only_text()) {
		/* XXX: this time here should be the time of the first subtitle, not 0 */
		text.push_back (make_shared<TextDecoder>(this, c->only_text(), ContentTime()));
	}

	for (auto i: c->ffmpeg_audio_streams()) {
		_next_time[i] = {};
	}
}


void
FFmpegDecoder::flush ()
{
	/* Get any remaining frames */

	AVPacket packet;
	packet.data = nullptr;
	packet.size = 0;
	while (video && decode_video_packet(&packet)) {}

	if (audio) {
		packet.data = nullptr;
		packet.size = 0;
		decode_audio_packet (&packet);
	}

	/* Make sure all streams are the same length and round up to the next video frame */

	auto const frc = film()->active_frame_rate_change(_ffmpeg_content->position());
	ContentTime full_length (_ffmpeg_content->full_length(film()), frc);
	full_length = full_length.ceil (frc.source);
	if (video) {
		double const vfr = _ffmpeg_content->video_frame_rate().get();
		auto const f = full_length.frames_round (vfr);
		auto v = video->position(film()).get_value_or(ContentTime()).frames_round(vfr) + 1;
		while (v < f) {
			video->emit (film(), shared_ptr<const ImageProxy> (new RawImageProxy (_black_image)), v);
			++v;
		}
	}

	for (auto i: _ffmpeg_content->ffmpeg_audio_streams ()) {
		auto a = audio->stream_position(film(), i);
		/* Unfortunately if a is 0 that really means that we don't know the stream position since
		   there has been no data on it since the last seek.  In this case we'll just do nothing
		   here.  I'm not sure if that's the right idea.
		*/
		if (a > ContentTime()) {
			while (a < full_length) {
				auto to_do = min (full_length - a, ContentTime::from_seconds (0.1));
				auto silence = make_shared<AudioBuffers>(i->channels(), to_do.frames_ceil (i->frame_rate()));
				silence->make_silent ();
				audio->emit (film(), i, silence, a, true);
				a += to_do;
			}
		}
	}

	if (audio) {
		audio->flush ();
	}
}


bool
FFmpegDecoder::pass ()
{
	auto packet = av_packet_alloc();
	DCPOMATIC_ASSERT (packet);

	int r = av_read_frame (_format_context, packet);

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

		av_packet_free (&packet);
		flush ();
		return true;
	}

	int const si = packet->stream_index;
	auto fc = _ffmpeg_content;

	if (_video_stream && si == _video_stream.get() && video && !video->ignore()) {
		decode_video_packet (packet);
	} else if (fc->subtitle_stream() && fc->subtitle_stream()->uses_index(_format_context, si) && !only_text()->ignore()) {
		decode_subtitle_packet (packet);
	} else {
		decode_audio_packet (packet);
	}

	av_packet_free (&packet);
	return false;
}


/** @param data pointer to array of pointers to buffers.
 *  Only the first buffer will be used for non-planar data, otherwise there will be one per channel.
 */
shared_ptr<AudioBuffers>
FFmpegDecoder::deinterleave_audio (shared_ptr<FFmpegAudioStream> stream) const
{
	DCPOMATIC_ASSERT (bytes_per_audio_sample (stream));

DCPOMATIC_DISABLE_WARNINGS
	int const size = av_samples_get_buffer_size (
		0, stream->stream(_format_context)->codec->channels, _frame->nb_samples, audio_sample_format (stream), 1
		);
DCPOMATIC_ENABLE_WARNINGS

	/* XXX: can't we just use _frame->nb_samples directly here? */
	/* XXX: can't we use swr_convert() to do the format conversion? */

	/* Deinterleave and convert to float */

	/* total_samples and frames will be rounded down here, so if there are stray samples at the end
	   of the block that do not form a complete sample or frame they will be dropped.
	*/
	int const total_samples = size / bytes_per_audio_sample (stream);
	int const channels = stream->channels();
	int const frames = total_samples / channels;
	auto audio = make_shared<AudioBuffers>(channels, frames);
	auto data = audio->data();

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
		DCPOMATIC_ASSERT (_frame->channels <= channels);
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
DCPOMATIC_DISABLE_WARNINGS
	return stream->stream (_format_context)->codec->sample_fmt;
DCPOMATIC_ENABLE_WARNINGS
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

	auto pre_roll = accurate ? ContentTime::from_seconds (2) : ContentTime (0);
	time -= pre_roll;

	/* XXX: it seems debatable whether PTS should be used here...
	   http://www.mjbshaw.com/2012/04/seeking-in-ffmpeg-know-your-timestamp.html
	*/

	optional<int> stream;

	if (_video_stream) {
		stream = _video_stream;
	} else {
		DCPOMATIC_ASSERT (_ffmpeg_content->audio);
		auto s = dynamic_pointer_cast<FFmpegAudioStream>(_ffmpeg_content->audio->stream());
		if (s) {
			stream = s->index (_format_context);
		}
	}

	DCPOMATIC_ASSERT (stream);

	auto u = time - _pts_offset;
	if (u < ContentTime ()) {
		u = ContentTime ();
	}
	av_seek_frame (
		_format_context,
		stream.get(),
		u.seconds() / av_q2d (_format_context->streams[stream.get()]->time_base),
		AVSEEK_FLAG_BACKWARD
		);

	{
		/* Force re-creation of filter graphs to reset them and hence to make sure
		   they don't have any pre-seek frames knocking about.
		*/
		boost::mutex::scoped_lock lm (_filter_graphs_mutex);
		_filter_graphs.clear ();
	}

	if (video_codec_context ()) {
		avcodec_flush_buffers (video_codec_context());
	}

DCPOMATIC_DISABLE_WARNINGS
	for (auto i: ffmpeg_content()->ffmpeg_audio_streams()) {
		avcodec_flush_buffers (i->stream(_format_context)->codec);
	}
DCPOMATIC_ENABLE_WARNINGS

	if (subtitle_codec_context ()) {
		avcodec_flush_buffers (subtitle_codec_context ());
	}

	_have_current_subtitle = false;

	for (auto& i: _next_time) {
		i.second = {};
	}
}


shared_ptr<FFmpegAudioStream>
FFmpegDecoder::audio_stream_from_index (int index) const
{
	/* XXX: inefficient */
	auto streams = ffmpeg_content()->ffmpeg_audio_streams();
	auto stream = streams.begin();
	while (stream != streams.end() && !(*stream)->uses_index(_format_context, index)) {
		++stream;
	}

	if (stream == streams.end ()) {
		return {};
	}

	return *stream;
}


void
FFmpegDecoder::process_audio_frame (shared_ptr<FFmpegAudioStream> stream, int64_t packet_pts)
{
	auto data = deinterleave_audio (stream);

	ContentTime ct;
	if (_frame->pts == AV_NOPTS_VALUE) {
		/* In some streams we see not every frame coming through with a timestamp; for those
		   that have AV_NOPTS_VALUE we need to work out the timestamp ourselves.  This is
		   particularly noticeable with TrueHD streams (see #1111).
		   */
		if (_next_time[stream]) {
			ct = *_next_time[stream];
		}
	} else {
		ct = ContentTime::from_seconds (
			_frame->best_effort_timestamp *
			av_q2d (stream->stream(_format_context)->time_base))
			+ _pts_offset;
	}

	_next_time[stream] = ct + ContentTime::from_frames(data->frames(), stream->frame_rate());

	if (ct < ContentTime()) {
		/* Discard audio data that comes before time 0 */
		auto const remove = min (int64_t(data->frames()), (-ct).frames_ceil(double(stream->frame_rate())));
		data->move (data->frames() - remove, remove, 0);
		data->set_frames (data->frames() - remove);
		ct += ContentTime::from_frames (remove, stream->frame_rate());
	}

	if (ct < ContentTime()) {
		LOG_WARNING (
			"Crazy timestamp %1 for %2 samples in stream %3 packet pts %4 (ts=%5 tb=%6, off=%7)",
			to_string(ct),
			data->frames(),
			stream->id(),
			packet_pts,
			_frame->best_effort_timestamp,
			av_q2d(stream->stream(_format_context)->time_base),
			to_string(_pts_offset)
			);
	}

	/* Give this data provided there is some, and its time is sane */
	if (ct >= ContentTime() && data->frames() > 0) {
		audio->emit (film(), stream, data, ct);
	}
}


void
FFmpegDecoder::decode_audio_packet (AVPacket* packet)
{
	auto stream = audio_stream_from_index (packet->stream_index);
	if (!stream) {
		return;
	}

	/* Audio packets can contain multiple frames, so we may have to call avcodec_decode_audio4
	   several times.  Make a simple copy so we can alter data and size.
	*/
	AVPacket copy_packet = *packet;

	while (copy_packet.size > 0) {
		int frame_finished;
		DCPOMATIC_DISABLE_WARNINGS
		int decode_result = avcodec_decode_audio4 (stream->stream(_format_context)->codec, _frame, &frame_finished, &copy_packet);
		DCPOMATIC_ENABLE_WARNINGS
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
			process_audio_frame (stream, copy_packet.pts);
		}

		copy_packet.data += decode_result;
		copy_packet.size -= decode_result;
	}
}


bool
FFmpegDecoder::decode_video_packet (AVPacket* packet)
{
	DCPOMATIC_ASSERT (_video_stream);

	int frame_finished;
DCPOMATIC_DISABLE_WARNINGS
	if (avcodec_decode_video2 (video_codec_context(), _frame, &frame_finished, packet) < 0 || !frame_finished) {
		return false;
	}
DCPOMATIC_ENABLE_WARNINGS

	boost::mutex::scoped_lock lm (_filter_graphs_mutex);

	shared_ptr<VideoFilterGraph> graph;

	auto i = _filter_graphs.begin();
	while (i != _filter_graphs.end() && !(*i)->can_process (dcp::Size (_frame->width, _frame->height), (AVPixelFormat) _frame->format)) {
		++i;
	}

	if (i == _filter_graphs.end ()) {
		dcp::Fraction vfr (lrint(_ffmpeg_content->video_frame_rate().get() * 1000), 1000);
		graph = make_shared<VideoFilterGraph>(dcp::Size (_frame->width, _frame->height), (AVPixelFormat) _frame->format, vfr);
		graph->setup (_ffmpeg_content->filters ());
		_filter_graphs.push_back (graph);
		LOG_GENERAL (N_("New graph for %1x%2, pixel format %3"), _frame->width, _frame->height, _frame->format);
	} else {
		graph = *i;
	}

	auto images = graph->process (_frame);

	for (auto const& i: images) {

		auto image = i.first;

		if (i.second != AV_NOPTS_VALUE) {
			double const pts = i.second * av_q2d(_format_context->streams[_video_stream.get()]->time_base) + _pts_offset.seconds();

			video->emit (
				film(),
				make_shared<RawImageProxy>(image),
				llrint(pts * _ffmpeg_content->active_video_frame_rate(film()))
				);
		} else {
			LOG_WARNING_NC ("Dropping frame without PTS");
		}
	}

	return true;
}


void
FFmpegDecoder::decode_subtitle_packet (AVPacket* packet)
{
	int got_subtitle;
	AVSubtitle sub;
	if (avcodec_decode_subtitle2 (subtitle_codec_context(), &sub, &got_subtitle, packet) < 0 || !got_subtitle) {
		return;
	}

	/* Stop any current subtitle, either at the time it was supposed to stop, or now if now is sooner */
	if (_have_current_subtitle) {
		if (_current_subtitle_to) {
			only_text()->emit_stop (min(*_current_subtitle_to, subtitle_period(sub).from + _pts_offset));
		} else {
			only_text()->emit_stop (subtitle_period(sub).from + _pts_offset);
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
	auto sub_period = subtitle_period (sub);
	ContentTime from;
	from = sub_period.from + _pts_offset;
	if (sub_period.to) {
		_current_subtitle_to = *sub_period.to + _pts_offset;
	} else {
		_current_subtitle_to = optional<ContentTime>();
		_have_current_subtitle = true;
	}

	for (unsigned int i = 0; i < sub.num_rects; ++i) {
		auto const rect = sub.rects[i];

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

	if (_current_subtitle_to) {
		only_text()->emit_stop (*_current_subtitle_to);
	}

	avsubtitle_free (&sub);
}


void
FFmpegDecoder::decode_bitmap_subtitle (AVSubtitleRect const * rect, ContentTime from)
{
	/* Note BGRA is expressed little-endian, so the first byte in the word is B, second
	   G, third R, fourth A.
	*/
	auto image = make_shared<Image>(AV_PIX_FMT_BGRA, dcp::Size (rect->w, rect->h), true);

#ifdef DCPOMATIC_HAVE_AVSUBTITLERECT_PICT
	/* Start of the first line in the subtitle */
	auto sub_p = rect->pict.data[0];
	/* sub_p looks up into a BGRA palette which is at rect->pict.data[1];
	   (i.e. first byte B, second G, third R, fourth A)
	*/
	auto const palette = rect->pict.data[1];
#else
	/* Start of the first line in the subtitle */
	auto sub_p = rect->data[0];
	/* sub_p looks up into a BGRA palette which is at rect->data[1].
	   (first byte B, second G, third R, fourth A)
	*/
	auto const* palette = rect->data[1];
#endif
	/* And the stream has a map of those palette colours to colours
	   chosen by the user; created a `mapped' palette from those settings.
	*/
	auto colour_map = ffmpeg_content()->subtitle_stream()->colours();
	vector<RGBA> mapped_palette (rect->nb_colors);
	for (int i = 0; i < rect->nb_colors; ++i) {
		RGBA c (palette[2], palette[1], palette[0], palette[3]);
		auto j = colour_map.find (c);
		if (j != colour_map.end ()) {
			mapped_palette[i] = j->second;
		} else {
			/* This colour was not found in the FFmpegSubtitleStream's colour map; probably because
			   it is from a project that was created before this stuff was added.  Just use the
			   colour straight from the original palette.
			*/
			mapped_palette[i] = c;
		}
		palette += 4;
	}

	/* Start of the output data */
	auto out_p = image->data()[0];

	for (int y = 0; y < rect->h; ++y) {
		auto sub_line_p = sub_p;
		auto out_line_p = out_p;
		for (int x = 0; x < rect->w; ++x) {
			auto const p = mapped_palette[*sub_line_p++];
			*out_line_p++ = p.b;
			*out_line_p++ = p.g;
			*out_line_p++ = p.r;
			*out_line_p++ = p.a;
		}
#ifdef DCPOMATIC_HAVE_AVSUBTITLERECT_PICT
		sub_p += rect->pict.linesize[0];
#else
		sub_p += rect->linesize[0];
#endif
		out_p += image->stride()[0];
	}

	int target_width = subtitle_codec_context()->width;
	if (target_width == 0 && video_codec_context()) {
		/* subtitle_codec_context()->width == 0 has been seen in the wild but I don't
		   know if it's supposed to mean something from FFmpeg's point of view.
		*/
		target_width = video_codec_context()->width;
	}
	int target_height = subtitle_codec_context()->height;
	if (target_height == 0 && video_codec_context()) {
		target_height = video_codec_context()->height;
	}
	DCPOMATIC_ASSERT (target_width);
	DCPOMATIC_ASSERT (target_height);
	dcpomatic::Rect<double> const scaled_rect (
		static_cast<double>(rect->x) / target_width,
		static_cast<double>(rect->y) / target_height,
		static_cast<double>(rect->w) / target_width,
		static_cast<double>(rect->h) / target_height
		);

	only_text()->emit_bitmap_start (from, image, scaled_rect);
}


void
FFmpegDecoder::decode_ass_subtitle (string ass, ContentTime from)
{
	/* We have no styles and no Format: line, so I'm assuming that FFmpeg
	   produces a single format of Dialogue: lines...
	*/

	int commas = 0;
	string text;
	for (size_t i = 0; i < ass.length(); ++i) {
		if (commas < 9 && ass[i] == ',') {
			++commas;
		} else if (commas == 9) {
			text += ass[i];
		}
	}

	if (text.empty ()) {
		return;
	}

	sub::RawSubtitle base;
	auto raw = sub::SSAReader::parse_line (
		base,
		text,
		_ffmpeg_content->video->size().width,
		_ffmpeg_content->video->size().height
		);

	for (auto const& i: sub::collect<vector<sub::Subtitle>>(raw)) {
		only_text()->emit_plain_start (from, i);
	}
}
