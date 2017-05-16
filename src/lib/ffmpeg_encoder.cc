/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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
using boost::shared_ptr;
using boost::bind;
using boost::weak_ptr;

int FFmpegEncoder::_video_stream_index = 0;
int FFmpegEncoder::_audio_stream_index = 1;

static AVPixelFormat
force_pixel_format (AVPixelFormat, AVPixelFormat out)
{
	return out;
}

FFmpegEncoder::FFmpegEncoder (shared_ptr<const Film> film, weak_ptr<Job> job, boost::filesystem::path output, Format format)
	: Encoder (film, job)
	, _video_options (0)
	, _history (1000)
	, _output (output)
	, _pending_audio (new AudioBuffers (film->audio_channels(), 0))
{
	switch (format) {
	case FORMAT_PRORES:
		_pixel_format = AV_PIX_FMT_YUV422P10;
		_sample_format = AV_SAMPLE_FMT_S16;
		_video_codec_name = "prores_ks";
		_audio_codec_name = "pcm_s16le";
		av_dict_set (&_video_options, "profile", "3", 0);
		av_dict_set (&_video_options, "threads", "auto", 0);
		break;
	case FORMAT_H264:
		_pixel_format = AV_PIX_FMT_YUV420P;
		_sample_format = AV_SAMPLE_FMT_FLTP;
		_video_codec_name = "libx264";
		_audio_codec_name = "aac";
		break;
	}

	_player->set_always_burn_subtitles (true);
	_player->set_play_referenced ();
}

void
FFmpegEncoder::setup_video ()
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
	_video_codec_context->width = _film->frame_size().width;
	_video_codec_context->height = _film->frame_size().height;
	_video_codec_context->time_base = (AVRational) { 1, _film->video_frame_rate() };
	_video_codec_context->pix_fmt = _pixel_format;
	_video_codec_context->flags |= CODEC_FLAG_QSCALE | CODEC_FLAG_GLOBAL_HEADER;
}

void
FFmpegEncoder::setup_audio ()
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
	_audio_codec_context->sample_rate = _film->audio_frame_rate ();
	_audio_codec_context->channel_layout = av_get_default_channel_layout (_film->audio_channels ());
	_audio_codec_context->channels = _film->audio_channels ();
}

void
FFmpegEncoder::go ()
{
	setup_video ();
	setup_audio ();

	avformat_alloc_output_context2 (&_format_context, 0, 0, _output.string().c_str());
	if (!_format_context) {
		throw runtime_error ("could not allocate FFmpeg format context");
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

	int r = avcodec_open2 (_audio_codec_context, _audio_codec, 0);
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

	{
		shared_ptr<Job> job = _job.lock ();
		DCPOMATIC_ASSERT (job);
		job->sub (_("Encoding"));
	}

	while (!_player->pass ()) {}

	if (_pending_audio->frames() > 0) {
		audio_frame (_pending_audio->frames ());
	}

	/* Flush */

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
FFmpegEncoder::video (shared_ptr<PlayerVideo> video, DCPTime time)
{
	shared_ptr<Image> image = video->image (
		bind (&Log::dcp_log, _film->log().get(), _1, _2),
		bind (&force_pixel_format, _1, _pixel_format),
		true,
		false
		);

	AVFrame* frame = av_frame_alloc ();
	DCPOMATIC_ASSERT (frame);

	for (int i = 0; i < 3; ++i) {
		size_t const size = image->stride()[i] * image->size().height;
		AVBufferRef* buffer = av_buffer_alloc (size);
		DCPOMATIC_ASSERT (buffer);
		/* XXX: inefficient */
		memcpy (buffer->data, image->data()[i], size);
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

	_history.event ();

	{
		boost::mutex::scoped_lock lm (_mutex);
		_last_time = time;
	}

	shared_ptr<Job> job = _job.lock ();
	if (job) {
		job->set_progress (float(time.get()) / _film->length().get());
	}
}

/** Called when the player gives us some audio */
void
FFmpegEncoder::audio (shared_ptr<AudioBuffers> audio, DCPTime)
{
	_pending_audio->append (audio);

	int frame_size = _audio_codec_context->frame_size;
	if (frame_size == 0) {
		/* codec has AV_CODEC_CAP_VARIABLE_FRAME_SIZE */
		frame_size = 2000;
	}

	while (_pending_audio->frames() >= frame_size) {
		audio_frame (frame_size);
	}
}

void
FFmpegEncoder::audio_frame (int size)
{
	DCPOMATIC_ASSERT (size);

	AVFrame* frame = av_frame_alloc ();
	DCPOMATIC_ASSERT (frame);

	int const channels = _audio_codec_context->channels;
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
FFmpegEncoder::subtitle (PlayerSubtitles, DCPTimePeriod)
{

}

float
FFmpegEncoder::current_rate () const
{
	return _history.rate ();
}

Frame
FFmpegEncoder::frames_done () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _last_time.frames_round (_film->video_frame_rate ());
}
