/*
    Copyright (C) 2017-2018 Carl Hetherington <cth@carlh.net>

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

#include "ffmpeg_encoder.h"
#include "film.h"
#include "job.h"
#include "player.h"
#include "player_video.h"
#include "log.h"
#include "image.h"
#include "cross.h"
#include "compose.hpp"
#include <iostream>

#include "i18n.h"

using std::string;
using std::runtime_error;
using std::cout;
using std::pair;
using boost::shared_ptr;
using boost::bind;
using boost::weak_ptr;

int FFmpegFileEncoder::_video_stream_index = 0;
int FFmpegFileEncoder::_audio_stream_index = 1;

FFmpegFileEncoder::FFmpegFileEncoder (
	dcp::Size video_frame_size,
	int video_frame_rate,
	int audio_frame_rate,
	int channels,
	ExportFormat format,
	int x264_crf,
	boost::filesystem::path output
	)
	: _video_options (0)
	, _audio_channels (channels)
	, _output (output)
	, _video_frame_size (video_frame_size)
	, _video_frame_rate (video_frame_rate)
	, _audio_frame_rate (audio_frame_rate)
{
	_pixel_format = pixel_format (format);

	switch (format) {
	case EXPORT_FORMAT_PRORES:
		_sample_format = AV_SAMPLE_FMT_S16;
		_video_codec_name = "prores_ks";
		_audio_codec_name = "pcm_s16le";
		av_dict_set (&_video_options, "profile", "3", 0);
		av_dict_set (&_video_options, "threads", "auto", 0);
		break;
	case EXPORT_FORMAT_H264:
		_sample_format = AV_SAMPLE_FMT_FLTP;
		_video_codec_name = "libx264";
		_audio_codec_name = "aac";
		av_dict_set_int (&_video_options, "crf", x264_crf, 0);
		break;
	}

	setup_video ();
	setup_audio ();

	int r = avformat_alloc_output_context2 (&_format_context, 0, 0, _output.string().c_str());
	if (!_format_context) {
		throw runtime_error (String::compose("could not allocate FFmpeg format context (%1)", r));
	}

	_video_stream = avformat_new_stream (_format_context, _video_codec);
	if (!_video_stream) {
		throw runtime_error ("could not create FFmpeg output video stream");
	}

	_audio_stream = avformat_new_stream (_format_context, _audio_codec);
	if (!_audio_stream) {
		throw runtime_error ("could not create FFmpeg output audio stream");
	}

	_video_stream->id = _video_stream_index;
	_video_stream->codec = _video_codec_context;

	_audio_stream->id = _audio_stream_index;
	_audio_stream->codec = _audio_codec_context;

	if (avcodec_open2 (_video_codec_context, _video_codec, &_video_options) < 0) {
		throw runtime_error ("could not open FFmpeg video codec");
	}

	r = avcodec_open2 (_audio_codec_context, _audio_codec, 0);
	if (r < 0) {
		char buffer[256];
		av_strerror (r, buffer, sizeof(buffer));
		throw runtime_error (String::compose ("could not open FFmpeg audio codec (%1)", buffer));
	}

	if (avio_open_boost (&_format_context->pb, _output, AVIO_FLAG_WRITE) < 0) {
		throw runtime_error ("could not open FFmpeg output file");
	}

	if (avformat_write_header (_format_context, 0) < 0) {
		throw runtime_error ("could not write header to FFmpeg output file");
	}

	_pending_audio.reset (new AudioBuffers(channels, 0));
}

AVPixelFormat
FFmpegFileEncoder::pixel_format (ExportFormat format)
{
	switch (format) {
	case EXPORT_FORMAT_PRORES:
		return AV_PIX_FMT_YUV422P10;
	case EXPORT_FORMAT_H264:
		return AV_PIX_FMT_YUV420P;
	default:
		DCPOMATIC_ASSERT (false);
	}

	return AV_PIX_FMT_YUV422P10;
}

void
FFmpegFileEncoder::setup_video ()
{
	_video_codec = avcodec_find_encoder_by_name (_video_codec_name.c_str());
	if (!_video_codec) {
		throw runtime_error (String::compose ("could not find FFmpeg encoder %1", _video_codec_name));
	}

	_video_codec_context = avcodec_alloc_context3 (_video_codec);
	if (!_video_codec_context) {
		throw runtime_error ("could not allocate FFmpeg video context");
	}

	avcodec_get_context_defaults3 (_video_codec_context, _video_codec);

	/* Variable quantisation */
	_video_codec_context->global_quality = 0;
	_video_codec_context->width = _video_frame_size.width;
	_video_codec_context->height = _video_frame_size.height;
	_video_codec_context->time_base = (AVRational) { 1, _video_frame_rate };
	_video_codec_context->pix_fmt = _pixel_format;
	_video_codec_context->flags |= AV_CODEC_FLAG_QSCALE | AV_CODEC_FLAG_GLOBAL_HEADER;
}

void
FFmpegFileEncoder::setup_audio ()
{
	_audio_codec = avcodec_find_encoder_by_name (_audio_codec_name.c_str());
	if (!_audio_codec) {
		throw runtime_error (String::compose ("could not find FFmpeg encoder %1", _audio_codec_name));
	}

	_audio_codec_context = avcodec_alloc_context3 (_audio_codec);
	if (!_audio_codec_context) {
		throw runtime_error ("could not allocate FFmpeg audio context");
	}

	avcodec_get_context_defaults3 (_audio_codec_context, _audio_codec);

	/* XXX: configurable */
	_audio_codec_context->bit_rate = 256 * 1024;
	_audio_codec_context->sample_fmt = _sample_format;
	_audio_codec_context->sample_rate = _audio_frame_rate;
	_audio_codec_context->channel_layout = av_get_default_channel_layout (_audio_channels);
	_audio_codec_context->channels = _audio_channels;
}

void
FFmpegFileEncoder::flush ()
{
	if (_pending_audio->frames() > 0) {
		audio_frame (_pending_audio->frames ());
	}

	bool flushed_video = false;
	bool flushed_audio = false;

	while (!flushed_video || !flushed_audio) {
		AVPacket packet;
		av_init_packet (&packet);
		packet.data = 0;
		packet.size = 0;

		int got_packet;
		avcodec_encode_video2 (_video_codec_context, &packet, 0, &got_packet);
		if (got_packet) {
			packet.stream_index = 0;
			av_interleaved_write_frame (_format_context, &packet);
		} else {
			flushed_video = true;
		}
		av_packet_unref (&packet);

		av_init_packet (&packet);
		packet.data = 0;
		packet.size = 0;

		avcodec_encode_audio2 (_audio_codec_context, &packet, 0, &got_packet);
		if (got_packet) {
			packet.stream_index = 0;
			av_interleaved_write_frame (_format_context, &packet);
		} else {
			flushed_audio = true;
		}
		av_packet_unref (&packet);
	}

	av_write_trailer (_format_context);

	avcodec_close (_video_codec_context);
	avcodec_close (_audio_codec_context);
	avio_close (_format_context->pb);
	avformat_free_context (_format_context);
}

void
FFmpegFileEncoder::video (shared_ptr<PlayerVideo> video, DCPTime time)
{
	shared_ptr<Image> image = video->image (
		bind (&PlayerVideo::force, _1, _pixel_format),
		true,
		false
		);

	AVFrame* frame = av_frame_alloc ();
	DCPOMATIC_ASSERT (frame);

	_pending_images[image->data()[0]] = image;
	for (int i = 0; i < 3; ++i) {
		AVBufferRef* buffer = av_buffer_create(image->data()[i], image->stride()[i] * image->size().height, &buffer_free, this, 0);
		frame->buf[i] = av_buffer_ref (buffer);
		frame->data[i] = buffer->data;
		frame->linesize[i] = image->stride()[i];
		av_buffer_unref (&buffer);
	}

	frame->width = image->size().width;
	frame->height = image->size().height;
	frame->format = _pixel_format;
	frame->pts = time.seconds() / av_q2d (_video_stream->time_base);

	AVPacket packet;
	av_init_packet (&packet);
	packet.data = 0;
	packet.size = 0;

	int got_packet;
	if (avcodec_encode_video2 (_video_codec_context, &packet, frame, &got_packet) < 0) {
		throw EncodeError ("FFmpeg video encode failed");
	}

	if (got_packet && packet.size) {
		packet.stream_index = _video_stream_index;
		av_interleaved_write_frame (_format_context, &packet);
		av_packet_unref (&packet);
	}

	av_frame_free (&frame);

}

/** Called when the player gives us some audio */
void
FFmpegFileEncoder::audio (shared_ptr<AudioBuffers> audio)
{
	_pending_audio->append (audio);

	int frame_size = _audio_codec_context->frame_size;
	if (frame_size == 0) {
		/* codec has AV_CODEC_CAP_VARIABLE_FRAME_SIZE */
		frame_size = _audio_frame_rate / _video_frame_rate;
	}

	while (_pending_audio->frames() >= frame_size) {
		audio_frame (frame_size);
	}
}

void
FFmpegFileEncoder::audio_frame (int size)
{
	DCPOMATIC_ASSERT (size);

	AVFrame* frame = av_frame_alloc ();
	DCPOMATIC_ASSERT (frame);

	int const channels = _pending_audio->channels();
	DCPOMATIC_ASSERT (channels);

	int const buffer_size = av_samples_get_buffer_size (0, channels, size, _audio_codec_context->sample_fmt, 0);
	DCPOMATIC_ASSERT (buffer_size >= 0);

	void* samples = av_malloc (buffer_size);
	DCPOMATIC_ASSERT (samples);

	frame->nb_samples = size;
	int r = avcodec_fill_audio_frame (frame, channels, _audio_codec_context->sample_fmt, (const uint8_t *) samples, buffer_size, 0);
	DCPOMATIC_ASSERT (r >= 0);

	float** p = _pending_audio->data ();
	switch (_audio_codec_context->sample_fmt) {
	case AV_SAMPLE_FMT_S16:
	{
		int16_t* q = reinterpret_cast<int16_t*> (samples);
		for (int i = 0; i < size; ++i) {
			for (int j = 0; j < channels; ++j) {
				*q++ = p[j][i] * 32767;
			}
		}
		break;
	}
	case AV_SAMPLE_FMT_FLTP:
	{
		float* q = reinterpret_cast<float*> (samples);
		for (int i = 0; i < channels; ++i) {
			memcpy (q, p[i], sizeof(float) * size);
			q += size;
		}
		break;
	}
	default:
		DCPOMATIC_ASSERT (false);
	}

	AVPacket packet;
	av_init_packet (&packet);
	packet.data = 0;
	packet.size = 0;

	int got_packet;
	if (avcodec_encode_audio2 (_audio_codec_context, &packet, frame, &got_packet) < 0) {
		throw EncodeError ("FFmpeg audio encode failed");
	}

	if (got_packet && packet.size) {
		packet.stream_index = _audio_stream_index;
		av_interleaved_write_frame (_format_context, &packet);
		av_packet_unref (&packet);
	}

	av_free (samples);
	av_frame_free (&frame);

	_pending_audio->trim_start (size);
}

void
FFmpegFileEncoder::subtitle (PlayerText, DCPTimePeriod)
{

}

void
FFmpegFileEncoder::buffer_free (void* opaque, uint8_t* data)
{
	reinterpret_cast<FFmpegFileEncoder*>(opaque)->buffer_free2(data);
}

void
FFmpegFileEncoder::buffer_free2 (uint8_t* data)
{
	_pending_images.erase (data);
}
