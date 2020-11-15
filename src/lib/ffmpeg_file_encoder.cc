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
using boost::optional;
using namespace dcpomatic;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif

int FFmpegFileEncoder::_video_stream_index = 0;
int FFmpegFileEncoder::_audio_stream_index_base = 1;


class ExportAudioStream : public boost::noncopyable
{
public:
	ExportAudioStream (string codec_name, int channels, int frame_rate, AVSampleFormat sample_format, AVFormatContext* format_context, int stream_index)
		: _format_context (format_context)
		, _stream_index (stream_index)
	{
		_codec = avcodec_find_encoder_by_name (codec_name.c_str());
		if (!_codec) {
			throw runtime_error (String::compose("could not find FFmpeg encoder %1", codec_name));
		}

		_codec_context = avcodec_alloc_context3 (_codec);
		if (!_codec_context) {
			throw runtime_error ("could not allocate FFmpeg audio context");
		}

		avcodec_get_context_defaults3 (_codec_context, _codec);

		/* XXX: configurable */
		_codec_context->bit_rate = channels * 128 * 1024;
		_codec_context->sample_fmt = sample_format;
		_codec_context->sample_rate = frame_rate;
		_codec_context->channel_layout = av_get_default_channel_layout (channels);
		_codec_context->channels = channels;

		_stream = avformat_new_stream (format_context, _codec);
		if (!_stream) {
			throw runtime_error ("could not create FFmpeg output audio stream");
		}

		_stream->id = stream_index;
DCPOMATIC_DISABLE_WARNINGS
		_stream->codec = _codec_context;
DCPOMATIC_ENABLE_WARNINGS

		int r = avcodec_open2 (_codec_context, _codec, 0);
		if (r < 0) {
			char buffer[256];
			av_strerror (r, buffer, sizeof(buffer));
			throw runtime_error (String::compose("could not open FFmpeg audio codec (%1)", buffer));
		}
	}

	~ExportAudioStream ()
	{
		avcodec_close (_codec_context);
	}

	int frame_size () const {
		return _codec_context->frame_size;
	}

	bool flush ()
	{
		AVPacket packet;
		av_init_packet (&packet);
		packet.data = 0;
		packet.size = 0;
		bool flushed = false;

		int got_packet;
DCPOMATIC_DISABLE_WARNINGS
		avcodec_encode_audio2 (_codec_context, &packet, 0, &got_packet);
DCPOMATIC_ENABLE_WARNINGS
		if (got_packet) {
			packet.stream_index = 0;
			av_interleaved_write_frame (_format_context, &packet);
		} else {
			flushed = true;
		}
		av_packet_unref (&packet);
		return flushed;
	}

	void write (int size, int channel_offset, int channels, float** data, int64_t sample_offset)
	{
		DCPOMATIC_ASSERT (size);

		AVFrame* frame = av_frame_alloc ();
		DCPOMATIC_ASSERT (frame);

		int const buffer_size = av_samples_get_buffer_size (0, channels, size, _codec_context->sample_fmt, 0);
		DCPOMATIC_ASSERT (buffer_size >= 0);

		void* samples = av_malloc (buffer_size);
		DCPOMATIC_ASSERT (samples);

		frame->nb_samples = size;
		int r = avcodec_fill_audio_frame (frame, channels, _codec_context->sample_fmt, (const uint8_t *) samples, buffer_size, 0);
		DCPOMATIC_ASSERT (r >= 0);

		switch (_codec_context->sample_fmt) {
		case AV_SAMPLE_FMT_S16:
		{
			int16_t* q = reinterpret_cast<int16_t*> (samples);
			for (int i = 0; i < size; ++i) {
				for (int j = 0; j < channels; ++j) {
					*q++ = data[j + channel_offset][i] * 32767;
				}
			}
			break;
		}
		case AV_SAMPLE_FMT_S32:
		{
			int32_t* q = reinterpret_cast<int32_t*> (samples);
			for (int i = 0; i < size; ++i) {
				for (int j = 0; j < channels; ++j) {
					*q++ = data[j + channel_offset][i] * 2147483647;
				}
			}
			break;
		}
		case AV_SAMPLE_FMT_FLTP:
		{
			float* q = reinterpret_cast<float*> (samples);
			for (int i = 0; i < channels; ++i) {
				memcpy (q, data[i + channel_offset], sizeof(float) * size);
				q += size;
			}
			break;
		}
		default:
			DCPOMATIC_ASSERT (false);
		}

		DCPOMATIC_ASSERT (_codec_context->time_base.num == 1);
		frame->pts = sample_offset * _codec_context->time_base.den / _codec_context->sample_rate;

		AVPacket packet;
		av_init_packet (&packet);
		packet.data = 0;
		packet.size = 0;
		int got_packet;

		DCPOMATIC_DISABLE_WARNINGS
		if (avcodec_encode_audio2 (_codec_context, &packet, frame, &got_packet) < 0) {
			throw EncodeError ("FFmpeg audio encode failed");
		}
		DCPOMATIC_ENABLE_WARNINGS

		if (got_packet && packet.size) {
			packet.stream_index = _stream_index;
			av_interleaved_write_frame (_format_context, &packet);
			av_packet_unref (&packet);
		}

		av_free (samples);
		av_frame_free (&frame);
	}

private:
	AVFormatContext* _format_context;
	AVCodec* _codec;
	AVCodecContext* _codec_context;
	AVStream* _stream;
	int _stream_index;
};


FFmpegFileEncoder::FFmpegFileEncoder (
	dcp::Size video_frame_size,
	int video_frame_rate,
	int audio_frame_rate,
	int channels,
	ExportFormat format,
	bool audio_stream_per_channel,
	int x264_crf,
	boost::filesystem::path output
#ifdef DCPOMATIC_VARIANT_SWAROOP
	, optional<dcp::Key> key
	, optional<string> id
#endif
	)
	: _audio_stream_per_channel (audio_stream_per_channel)
	, _video_options (0)
	, _audio_channels (channels)
	, _output (output)
	, _video_frame_size (video_frame_size)
	, _video_frame_rate (video_frame_rate)
	, _audio_frame_rate (audio_frame_rate)
	, _audio_frames (0)
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
	case EXPORT_FORMAT_H264_AAC:
		_sample_format = AV_SAMPLE_FMT_FLTP;
		_video_codec_name = "libx264";
		_audio_codec_name = "aac";
		av_dict_set_int (&_video_options, "crf", x264_crf, 0);
		break;
	case EXPORT_FORMAT_H264_PCM:
		_sample_format = AV_SAMPLE_FMT_S32;
		_video_codec_name = "libx264";
		_audio_codec_name = "pcm_s24le";
		av_dict_set_int (&_video_options, "crf", x264_crf, 0);
		break;
	default:
		DCPOMATIC_ASSERT (false);
	}

#ifdef DCPOMATIC_VARIANT_SWAROOP
	int r = avformat_alloc_output_context2 (&_format_context, av_guess_format("mov", 0, 0), 0, 0);
#else
	int r = avformat_alloc_output_context2 (&_format_context, 0, 0, _output.string().c_str());
#endif
	if (!_format_context) {
		throw runtime_error (String::compose("could not allocate FFmpeg format context (%1)", r));
	}

	setup_video ();
	setup_audio ();

	r = avio_open_boost (&_format_context->pb, _output, AVIO_FLAG_WRITE);
	if (r < 0) {
		throw runtime_error (String::compose("could not open FFmpeg output file %1 (%2)", _output.string(), r));
	}

	AVDictionary* options = 0;

#ifdef DCPOMATIC_VARIANT_SWAROOP
	if (key) {
		av_dict_set (&options, "encryption_key", key->hex().c_str(), 0);
		/* XXX: is this OK? */
		av_dict_set (&options, "encryption_kid", "00000000000000000000000000000000", 0);
		av_dict_set (&options, "encryption_scheme", "cenc-aes-ctr", 0);
	}

	if (id) {
		if (av_dict_set(&_format_context->metadata, SWAROOP_ID_TAG, id->c_str(), 0) < 0) {
			throw runtime_error ("Could not write ID to output");
		}
	}
#endif

	if (avformat_write_header (_format_context, &options) < 0) {
		throw runtime_error ("could not write header to FFmpeg output file");
	}

	_pending_audio.reset (new AudioBuffers(channels, 0));
}


FFmpegFileEncoder::~FFmpegFileEncoder ()
{
	_audio_streams.clear ();
	avcodec_close (_video_codec_context);
	avformat_free_context (_format_context);
}


AVPixelFormat
FFmpegFileEncoder::pixel_format (ExportFormat format)
{
	switch (format) {
	case EXPORT_FORMAT_PRORES:
		return AV_PIX_FMT_YUV422P10;
	case EXPORT_FORMAT_H264_AAC:
	case EXPORT_FORMAT_H264_PCM:
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

	_video_stream = avformat_new_stream (_format_context, _video_codec);
	if (!_video_stream) {
		throw runtime_error ("could not create FFmpeg output video stream");
	}

DCPOMATIC_DISABLE_WARNINGS
	_video_stream->id = _video_stream_index;
	_video_stream->codec = _video_codec_context;
DCPOMATIC_ENABLE_WARNINGS

	if (avcodec_open2 (_video_codec_context, _video_codec, &_video_options) < 0) {
		throw runtime_error ("could not open FFmpeg video codec");
	}
}


void
FFmpegFileEncoder::setup_audio ()
{
	int const streams = _audio_stream_per_channel ? _audio_channels : 1;
	int const channels_per_stream = _audio_stream_per_channel ? 1 : _audio_channels;

	for (int i = 0; i < streams; ++i) {
		_audio_streams.push_back(
			shared_ptr<ExportAudioStream>(
				new ExportAudioStream(_audio_codec_name, channels_per_stream, _audio_frame_rate, _sample_format, _format_context, _audio_stream_index_base + i)
				)
			);
	}
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
DCPOMATIC_DISABLE_WARNINGS
		avcodec_encode_video2 (_video_codec_context, &packet, 0, &got_packet);
DCPOMATIC_ENABLE_WARNINGS
		if (got_packet) {
			packet.stream_index = 0;
			av_interleaved_write_frame (_format_context, &packet);
		} else {
			flushed_video = true;
		}
		av_packet_unref (&packet);

		flushed_audio = true;
		BOOST_FOREACH (shared_ptr<ExportAudioStream> i, _audio_streams) {
			if (!i->flush()) {
				flushed_audio = false;
			}
		}
	}

	av_write_trailer (_format_context);
}

void
FFmpegFileEncoder::video (shared_ptr<PlayerVideo> video, DCPTime time)
{
	/* All our output formats are video range at the moment */
	shared_ptr<Image> image = video->image (
		bind (&PlayerVideo::force, _1, _pixel_format),
		VIDEO_RANGE_VIDEO,
		true,
		false
		);

	AVFrame* frame = av_frame_alloc ();
	DCPOMATIC_ASSERT (frame);

	{
		boost::mutex::scoped_lock lm (_pending_images_mutex);
		_pending_images[image->data()[0]] = image;
	}

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
	DCPOMATIC_ASSERT (_video_stream->time_base.num == 1);
	frame->pts = time.get() * _video_stream->time_base.den / DCPTime::HZ;

	AVPacket packet;
	av_init_packet (&packet);
	packet.data = 0;
	packet.size = 0;

	int got_packet;
DCPOMATIC_DISABLE_WARNINGS
	if (avcodec_encode_video2 (_video_codec_context, &packet, frame, &got_packet) < 0) {
		throw EncodeError ("FFmpeg video encode failed");
	}
DCPOMATIC_ENABLE_WARNINGS

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

	DCPOMATIC_ASSERT (!_audio_streams.empty());
	int frame_size = _audio_streams[0]->frame_size();
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
	if (_audio_stream_per_channel) {
		int offset = 0;
		BOOST_FOREACH (shared_ptr<ExportAudioStream> i, _audio_streams) {
			i->write (size, offset, 1, _pending_audio->data(), _audio_frames);
			++offset;
		}
	} else {
		DCPOMATIC_ASSERT (!_audio_streams.empty());
		DCPOMATIC_ASSERT (_pending_audio->channels());
		_audio_streams[0]->write (size, 0, _pending_audio->channels(), _pending_audio->data(), _audio_frames);
	}

	_pending_audio->trim_start (size);
	_audio_frames += size;
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
	boost::mutex::scoped_lock lm (_pending_images_mutex);
	if (_pending_images.find(data) != _pending_images.end()) {
		_pending_images.erase (data);
	}
}
