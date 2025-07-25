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


#include "audio_buffers.h"
#include "audio_content.h"
#include "audio_decoder.h"
#include "dcpomatic_log.h"
#include "exceptions.h"
#include "ffmpeg_audio_stream.h"
#include "ffmpeg_content.h"
#include "ffmpeg_decoder.h"
#include "ffmpeg_subtitle_stream.h"
#include "film.h"
#include "filter.h"
#include "frame_interval_checker.h"
#include "image.h"
#include "log.h"
#include "raw_image_proxy.h"
#include "text_content.h"
#include "text_decoder.h"
#include "util.h"
#include "video_decoder.h"
#include "video_filter_graph.h"
#include <dcp/text_string.h>
#include <sub/ssa_reader.h>
#include <sub/subtitle.h>
#include <sub/collect.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <boost/algorithm/string.hpp>
#include <iomanip>
#include <iostream>
#include <vector>
#include <stdint.h>

#include "i18n.h"


using std::cout;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::min;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using dcp::Size;
using namespace dcpomatic;


FFmpegDecoder::FFmpegDecoder (shared_ptr<const Film> film, shared_ptr<const FFmpegContent> c, bool fast)
	: FFmpeg (c)
	, Decoder (film)
	, _filter_graphs(c->filters(), dcp::Fraction(lrint(_ffmpeg_content->video_frame_rate().get_value_or(24) * 1000), 1000))
{
	if (c->video && c->video->use()) {
		video = make_shared<VideoDecoder>(this, c);
		_pts_offset = pts_offset (c->ffmpeg_audio_streams(), c->first_video(), c->active_video_frame_rate(film));
		/* It doesn't matter what size or pixel format this is, it just needs to be black */
		_black_image = make_shared<Image>(AV_PIX_FMT_RGB24, dcp::Size (128, 128), Image::Alignment::PADDED);
		_black_image->make_black ();
	} else {
		_pts_offset = {};
	}

	if (c->has_mapped_audio()) {
		audio = make_shared<AudioDecoder>(this, c->audio, fast);
	}

	if (c->only_text()) {
		text.push_back (make_shared<TextDecoder>(this, c->only_text()));
		/* XXX: we should be calling maybe_set_position() on this TextDecoder, but we can't easily find
		 * the time of the first subtitle at this point.
		 */
	}

	for (auto i: c->ffmpeg_audio_streams()) {
		_next_time[i] = boost::optional<dcpomatic::ContentTime>();
	}
}


FFmpegDecoder::FlushResult
FFmpegDecoder::flush ()
{
	LOG_DEBUG_PLAYER("Flush FFmpeg decoder: current state {}", static_cast<int>(_flush_state));

	switch (_flush_state) {
	case FlushState::CODECS:
		if (flush_codecs() == FlushResult::DONE) {
			LOG_DEBUG_PLAYER_NC("Finished flushing codecs");
			_flush_state = FlushState::AUDIO_DECODER;
		}
		break;
	case FlushState::AUDIO_DECODER:
		if (audio) {
			audio->flush();
		}
		LOG_DEBUG_PLAYER_NC("Finished flushing audio decoder");
		_flush_state = FlushState::FILL;
		break;
	case FlushState::FILL:
		if (flush_fill() == FlushResult::DONE) {
			LOG_DEBUG_PLAYER_NC("Finished flushing fills");
			return FlushResult::DONE;
		}
		break;
	}

	return FlushResult::AGAIN;
}


/** @return true if we have finished flushing the codecs */
FFmpegDecoder::FlushResult
FFmpegDecoder::flush_codecs()
{
	bool did_something = false;
	if (video) {
		if (decode_and_process_video_packet(nullptr)) {
			did_something = true;
		}
	}

	for (auto i: ffmpeg_content()->ffmpeg_audio_streams()) {
		auto context = _codec_context[i->index(_format_context)];
		int r = avcodec_send_packet (context, nullptr);
		if (r < 0 && r != AVERROR_EOF) {
			/* EOF can happen if we've already sent a flush packet */
			throw DecodeError (N_("avcodec_send_packet"), N_("FFmpegDecoder::flush"), r);
		}
		r = avcodec_receive_frame (context, audio_frame(i));
		if (r >= 0) {
			process_audio_frame (i);
			did_something = true;
		}
	}

	return did_something ? FlushResult::AGAIN : FlushResult::DONE;
}


FFmpegDecoder::FlushResult
FFmpegDecoder::flush_fill()
{
	/* Make sure all streams are the same length and round up to the next video frame */

	bool did_something = false;

	auto const frc = film()->active_frame_rate_change(_ffmpeg_content->position());
	ContentTime full_length (_ffmpeg_content->full_length(film()), frc);
	full_length = full_length.ceil (frc.source);
	if (video && !video->ignore()) {
		double const vfr = _ffmpeg_content->video_frame_rate().get();
		auto const v = video->position(film()).get_value_or(ContentTime()) + ContentTime::from_frames(1, vfr);
		if (v < full_length) {
			video->emit(film(), make_shared<const RawImageProxy>(_black_image), v);
			did_something = true;
		}
	}

	if (audio && !audio->ignore()) {
		for (auto i: _ffmpeg_content->ffmpeg_audio_streams ()) {
			auto const a = audio->stream_position(film(), i);
			/* Unfortunately if a is 0 that really means that we don't know the stream position since
			   there has been no data on it since the last seek.  In this case we'll just do nothing
			   here.  I'm not sure if that's the right idea.
			*/
			if (a > ContentTime() && a < full_length) {
				LOG_DEBUG_PLAYER("Flush inserts silence at {}", to_string(a));
				auto to_do = min (full_length - a, ContentTime::from_seconds (0.1));
				auto silence = make_shared<AudioBuffers>(i->channels(), to_do.frames_ceil (i->frame_rate()));
				silence->make_silent ();
				audio->emit (film(), i, silence, a, true);
				did_something = true;
			}
		}
	}

	return did_something ? FlushResult::AGAIN : FlushResult::DONE;
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
		LOG_DEBUG_PLAYER("FFpmegDecoder::pass flushes because av_read_frame returned {}", r);
		if (r != AVERROR_EOF) {
			/* Maybe we should fail here, but for now we'll just finish off instead */
			char buf[256];
			av_strerror (r, buf, sizeof(buf));
			LOG_ERROR (N_("error on av_read_frame ({}) ({})"), &buf[0], r);
		}

		av_packet_free (&packet);
		return flush() == FlushResult::DONE;
	}

	int const si = packet->stream_index;
	auto fc = _ffmpeg_content;

	if (_video_stream && si == _video_stream.get() && video && !video->ignore()) {
		decode_and_process_video_packet (packet);
	} else if (fc->subtitle_stream() && fc->subtitle_stream()->uses_index(_format_context, si) && !only_text()->ignore()) {
		decode_and_process_subtitle_packet (packet);
	} else if (audio) {
		decode_and_process_audio_packet (packet);
	}

	if (_have_current_subtitle && _current_subtitle_to && position() > *_current_subtitle_to) {
		only_text()->emit_stop(*_current_subtitle_to);
		_have_current_subtitle = false;
	}

	av_packet_free (&packet);
	return false;
}


/** @param data pointer to array of pointers to buffers.
 *  Only the first buffer will be used for non-planar data, otherwise there will be one per channel.
 */
static
shared_ptr<AudioBuffers>
deinterleave_audio(AVFrame* frame)
{
	auto format = static_cast<AVSampleFormat>(frame->format);

	/* XXX: can't we use swr_convert() to do the format conversion? */

	int const channels = frame->ch_layout.nb_channels;
	int const frames = frame->nb_samples;
	int const total_samples = frames * channels;
	auto audio = make_shared<AudioBuffers>(channels, frames);
	auto data = audio->data();

	if (frames == 0) {
		return audio;
	}

	switch (format) {
	case AV_SAMPLE_FMT_U8:
	{
		auto p = reinterpret_cast<uint8_t *> (frame->data[0]);
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
		auto p = reinterpret_cast<int16_t *> (frame->data[0]);
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
		auto p = reinterpret_cast<int16_t **> (frame->data);
		for (int i = 0; i < channels; ++i) {
			for (int j = 0; j < frames; ++j) {
				data[i][j] = static_cast<float>(p[i][j]) / (1 << 15);
			}
		}
	}
	break;

	case AV_SAMPLE_FMT_S32:
	{
		auto p = reinterpret_cast<int32_t *> (frame->data[0]);
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
		auto p = reinterpret_cast<int32_t **> (frame->data);
		for (int i = 0; i < channels; ++i) {
			for (int j = 0; j < frames; ++j) {
				data[i][j] = static_cast<float>(p[i][j]) / 2147483648;
			}
		}
	}
	break;

	case AV_SAMPLE_FMT_FLT:
	{
		auto p = reinterpret_cast<float*> (frame->data[0]);
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
		auto p = reinterpret_cast<float**> (frame->data);
		for (int i = 0; i < channels; ++i) {
			memcpy (data[i], p[i], frames * sizeof(float));
		}
	}
	break;

	default:
		throw DecodeError (fmt::format(_("Unrecognised audio sample format ({})"), static_cast<int>(format)));
	}

	return audio;
}


AVSampleFormat
FFmpegDecoder::audio_sample_format (shared_ptr<FFmpegAudioStream> stream) const
{
	return static_cast<AVSampleFormat>(stream->stream(_format_context)->codecpar->format);
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

	_flush_state = FlushState::CODECS;

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

	/* Force re-creation of filter graphs to reset them, to make sure
	   they don't have any pre-seek frames.
	*/
	_filter_graphs.clear();

	if (video_codec_context ()) {
		avcodec_flush_buffers (video_codec_context());
	}

	for (auto i: ffmpeg_content()->ffmpeg_audio_streams()) {
		avcodec_flush_buffers (_codec_context[i->index(_format_context)]);
	}

	if (subtitle_codec_context ()) {
		avcodec_flush_buffers (subtitle_codec_context ());
	}

	_have_current_subtitle = false;

	for (auto& i: _next_time) {
		i.second = boost::optional<dcpomatic::ContentTime>();
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
FFmpegDecoder::process_audio_frame (shared_ptr<FFmpegAudioStream> stream)
{
	auto frame = audio_frame (stream);
	auto data = deinterleave_audio(frame);

	auto const time_base = stream->stream(_format_context)->time_base;

	ContentTime ct;
	if (frame->pts == AV_NOPTS_VALUE) {
		/* In some streams we see not every frame coming through with a timestamp; for those
		   that have AV_NOPTS_VALUE we need to work out the timestamp ourselves.  This is
		   particularly noticeable with TrueHD streams (see #1111).
		   */
		if (_next_time[stream]) {
			ct = *_next_time[stream];
		}
	} else {
		ct = ContentTime::from_seconds (
			frame->best_effort_timestamp *
			av_q2d(time_base))
			+ _pts_offset;
		LOG_DEBUG_PLAYER(
			"Process audio with timestamp {} (BET {}, timebase {}/{}, (PTS offset {})",
			to_string(ct),
			frame->best_effort_timestamp,
			time_base.num,
			time_base.den,
			to_string(_pts_offset)
			);
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
			"Crazy timestamp {} for {} samples in stream {} (ts={} tb={}, off={})",
			to_string(ct),
			data->frames(),
			stream->id(),
			frame->best_effort_timestamp,
			av_q2d(time_base),
			to_string(_pts_offset)
			);
	}

	/* Give this data provided there is some, and its time is sane */
	if (ct >= ContentTime() && data->frames() > 0) {
		audio->emit (film(), stream, data, ct);
	}
}


void
FFmpegDecoder::decode_and_process_audio_packet (AVPacket* packet)
{
	auto stream = audio_stream_from_index (packet->stream_index);
	if (!stream) {
		return;
	}

	auto context = _codec_context[stream->index(_format_context)];
	auto frame = audio_frame (stream);

	LOG_DEBUG_PLAYER("Send audio packet on stream {}", stream->index(_format_context));
	int r = avcodec_send_packet (context, packet);
	if (r < 0) {
		LOG_WARNING("avcodec_send_packet returned {} for an audio packet", r);
	}
	while (r >= 0) {
		r = avcodec_receive_frame (context, frame);
		if (r == AVERROR(EAGAIN)) {
			/* More input is required */
			LOG_DEBUG_PLAYER_NC("EAGAIN after trying to receive audio frame");
			return;
		}

		/* We choose to be relaxed here about other errors; it seems that there may be valid
		 * data to decode even if an error occurred.  #352 may be related (though this was
		 * when we were using an old version of the FFmpeg API).
		 */
		process_audio_frame (stream);
	}
}


bool
FFmpegDecoder::decode_and_process_video_packet (AVPacket* packet)
{
	DCPOMATIC_ASSERT (_video_stream);

	auto context = video_codec_context();

	bool pending = false;
	do {
		int r = avcodec_send_packet (context, packet);
		if (r < 0) {
			LOG_WARNING("avcodec_send_packet returned {} for a video packet", r);
		}

		/* EAGAIN means we should call avcodec_receive_frame and then re-send the same packet */
		pending = r == AVERROR(EAGAIN);

		while (true) {
			r = avcodec_receive_frame (context, _video_frame);
			if (r == AVERROR(EAGAIN) || r == AVERROR_EOF || (r < 0 && !packet)) {
				/* More input is required, no more frames are coming, or we are flushing and there was
				 * some error which we just want to ignore.
				 */
				return false;
			} else if (r < 0) {
				throw DecodeError (N_("avcodec_receive_frame"), N_("FFmpeg::decode_and_process_video_packet"), r);
			}

			process_video_frame ();
		}
	} while (pending);

	return true;
}


void
FFmpegDecoder::process_video_frame ()
{
	auto graph = _filter_graphs.get(dcp::Size(_video_frame->width, _video_frame->height), static_cast<AVPixelFormat>(_video_frame->format));
	auto images = graph->process (_video_frame);

	for (auto const& i: images) {

		auto image = i.first;

		if (i.second != AV_NOPTS_VALUE) {
			double const pts = i.second * av_q2d(_format_context->streams[_video_stream.get()]->time_base) + _pts_offset.seconds();

			video->emit (
				film(),
				make_shared<RawImageProxy>(image),
				ContentTime::from_seconds(pts)
				);
		} else {
			LOG_WARNING_NC ("Dropping frame without PTS");
		}
	}
}


void
FFmpegDecoder::decode_and_process_subtitle_packet (AVPacket* packet)
{
	auto context = subtitle_codec_context();
	if (!context) {
		return;
	}

	int got_subtitle;
	AVSubtitle sub;
	if (avcodec_decode_subtitle2(context, &sub, &got_subtitle, packet) < 0 || !got_subtitle) {
		return;
	}

	auto sub_period = subtitle_period (packet, ffmpeg_content()->subtitle_stream()->stream(_format_context), sub);

	/* Stop any current subtitle, either at the time it was supposed to stop, or now if now is sooner */
	if (_have_current_subtitle) {
		if (_current_subtitle_to) {
			only_text()->emit_stop (min(*_current_subtitle_to, sub_period.from + _pts_offset));
		} else {
			only_text()->emit_stop (sub_period.from + _pts_offset);
		}
		_have_current_subtitle = false;
	}

	if (sub.num_rects <= 0) {
		/* Nothing new in this subtitle */
		avsubtitle_free (&sub);
		return;
	}

	/* Subtitle PTS (within the source, not taking into account any of the
	   source that we may have chopped off for the DCP).
	*/
	ContentTime from;
	from = sub_period.from + _pts_offset;
	_have_current_subtitle = true;
	if (sub_period.to) {
		_current_subtitle_to = *sub_period.to + _pts_offset;
	} else {
		_current_subtitle_to = {};
	}

	ContentBitmapText bitmap_text(from);
	for (unsigned int i = 0; i < sub.num_rects; ++i) {
		auto const rect = sub.rects[i];

		switch (rect->type) {
		case SUBTITLE_NONE:
			break;
		case SUBTITLE_BITMAP:
			bitmap_text.subs.push_back(process_bitmap_subtitle(rect));
			break;
		case SUBTITLE_TEXT:
			cout << "XXX: SUBTITLE_TEXT " << rect->text << "\n";
			break;
		case SUBTITLE_ASS:
			process_ass_subtitle (rect->ass, from);
			break;
		}
	}

	if (!bitmap_text.subs.empty()) {
		only_text()->emit_bitmap_start(bitmap_text);
	}

	avsubtitle_free (&sub);
}


BitmapText
FFmpegDecoder::process_bitmap_subtitle (AVSubtitleRect const * rect)
{
	/* Note BGRA is expressed little-endian, so the first byte in the word is B, second
	   G, third R, fourth A.
	*/
	auto image = make_shared<Image>(AV_PIX_FMT_BGRA, dcp::Size (rect->w, rect->h), Image::Alignment::PADDED);

#ifdef DCPOMATIC_HAVE_AVSUBTITLERECT_PICT
	/* Start of the first line in the subtitle */
	auto sub_p = rect->pict.data[0];
	/* sub_p looks up into a BGRA palette which is at rect->pict.data[1];
	   (i.e. first byte B, second G, third R, fourth A)
	*/
	auto const* palette = rect->pict.data[1];
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

	int x_offset = 0;
	int y_offset = 0;
	if (_ffmpeg_content->video && _ffmpeg_content->video->use()) {
		auto const crop = _ffmpeg_content->video->actual_crop();
		target_width -= crop.left + crop.right;
		target_height -= crop.top + crop.bottom;
		x_offset = -crop.left;
		y_offset = -crop.top;
	}

	DCPOMATIC_ASSERT(target_width > 0);
	DCPOMATIC_ASSERT(target_height > 0);

	dcpomatic::Rect<double> const scaled_rect (
		static_cast<double>(rect->x + x_offset) / target_width,
		static_cast<double>(rect->y + y_offset) / target_height,
		static_cast<double>(rect->w) / target_width,
		static_cast<double>(rect->h) / target_height
		);

	return { image, scaled_rect };
}


void
FFmpegDecoder::process_ass_subtitle (string ass, ContentTime from)
{
	/* We have no styles and no Format: line, so I'm assuming that FFmpeg
	   produces a single format of Dialogue: lines...
	*/

	int commas = 0;
	string text;
	for (size_t i = 0; i < ass.length(); ++i) {
		if (commas < 8 && ass[i] == ',') {
			++commas;
		} else if (commas == 8) {
			text += ass[i];
		}
	}

	if (text.empty ()) {
		return;
	}

	auto video_size = _ffmpeg_content->video->size();
	DCPOMATIC_ASSERT(video_size);

	sub::SSAReader::Context context(video_size->width, video_size->height, sub::Colour(1, 1, 1));
	auto const raw = sub::SSAReader::parse_line({}, text, context);

	for (auto const& i: sub::collect<vector<sub::Subtitle>>(raw)) {
		only_text()->emit_plain_start (from, i);
	}
}
