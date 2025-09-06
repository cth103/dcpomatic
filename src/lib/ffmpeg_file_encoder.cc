/*
    Copyright (C) 2017-2021 Carl Hetherington <cth@carlh.net>

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


#include "cross.h"
#include "ffmpeg_file_encoder.h"
#include "ffmpeg_wrapper.h"
#include "film.h"
#include "image.h"
#include "job.h"
#include "log.h"
#include "player.h"
#include "player_video.h"
extern "C" {
#include <libavutil/channel_layout.h>
}
#include <iostream>

#include "i18n.h"


using std::cout;
using std::make_shared;
using std::shared_ptr;
using std::string;
using boost::bind;
using namespace dcpomatic;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


int FFmpegFileEncoder::_video_stream_index = 0;
int FFmpegFileEncoder::_audio_stream_index_base = 1;


class ExportAudioStream
{
public:
	ExportAudioStream(string codec_name, int channels, int frame_rate, AVSampleFormat sample_format, AVFormatContext* format_context, int stream_index)
		: _format_context(format_context)
		, _stream_index(stream_index)
	{
		_codec = avcodec_find_encoder_by_name(codec_name.c_str());
		if (!_codec) {
			throw EncodeError(fmt::format("avcodec_find_encoder_by_name failed for {}", codec_name));
		}

		_codec_context = avcodec_alloc_context3(_codec);
		if (!_codec_context) {
			throw std::bad_alloc();
		}

		/* XXX: configurable */
		_codec_context->bit_rate = channels * 128 * 1024;
		_codec_context->sample_fmt = sample_format;
		_codec_context->sample_rate = frame_rate;
		av_channel_layout_default(&_codec_context->ch_layout, channels);

		int r = avcodec_open2(_codec_context, _codec, 0);
		if (r < 0) {
			throw EncodeError(N_("avcodec_open2"), N_("ExportAudioStream::ExportAudioStream"), r);
		}

		_stream = avformat_new_stream(format_context, _codec);
		if (!_stream) {
			throw EncodeError(N_("avformat_new_stream"), N_("ExportAudioStream::ExportAudioStream"));
		}

		_stream->id = stream_index;
		_stream->disposition |= AV_DISPOSITION_DEFAULT;
		r = avcodec_parameters_from_context(_stream->codecpar, _codec_context);
		if (r < 0) {
			throw EncodeError(N_("avcodec_parameters_from_context"), N_("ExportAudioStream::ExportAudioStream"), r);
		}
	}

	~ExportAudioStream()
	{
		avcodec_free_context(&_codec_context);
	}

	ExportAudioStream(ExportAudioStream const&) = delete;
	ExportAudioStream& operator=(ExportAudioStream const&) = delete;

	int frame_size() const {
		return _codec_context->frame_size;
	}

	bool flush()
	{
		int r = avcodec_send_frame(_codec_context, nullptr);
		if (r < 0 && r != AVERROR_EOF) {
			/* We get EOF if we've already flushed the stream once */
			throw EncodeError(N_("avcodec_send_frame"), N_("ExportAudioStream::flush"), r);
		}

		ffmpeg::Packet packet;
		r = avcodec_receive_packet(_codec_context, packet.get());
		if (r == AVERROR_EOF) {
			return true;
		} else if (r < 0) {
			throw EncodeError(N_("avcodec_receive_packet"), N_("ExportAudioStream::flush"), r);
		}

		packet->stream_index = _stream_index;
		av_interleaved_write_frame(_format_context, packet.get());
		return false;
	}

	void write(int size, int channel_offset, int channels, float* const* data, int64_t sample_offset)
	{
		DCPOMATIC_ASSERT(size);

		auto frame = av_frame_alloc();
		DCPOMATIC_ASSERT(frame);

		int line_size;
		int const buffer_size = av_samples_get_buffer_size(&line_size, channels, size, _codec_context->sample_fmt, 0);
		DCPOMATIC_ASSERT(buffer_size >= 0);

		auto samples = av_malloc(buffer_size);
		DCPOMATIC_ASSERT(samples);

		frame->nb_samples = size;
		frame->format = _codec_context->sample_fmt;
		frame->ch_layout.nb_channels = channels;
		int r = avcodec_fill_audio_frame(frame, channels, _codec_context->sample_fmt, (const uint8_t *) samples, buffer_size, 0);
		DCPOMATIC_ASSERT(r >= 0);

		switch (_codec_context->sample_fmt) {
		case AV_SAMPLE_FMT_S16:
		{
			int16_t* q = reinterpret_cast<int16_t*>(samples);
			for (int i = 0; i < size; ++i) {
				for (int j = 0; j < channels; ++j) {
					*q++ = data[j + channel_offset][i] * 32767;
				}
			}
			break;
		}
		case AV_SAMPLE_FMT_S32:
		{
			int32_t* q = reinterpret_cast<int32_t*>(samples);
			for (int i = 0; i < size; ++i) {
				for (int j = 0; j < channels; ++j) {
					*q++ = data[j + channel_offset][i] * 2147483647;
				}
			}
			break;
		}
		case AV_SAMPLE_FMT_FLTP:
		{
			for (int i = 0; i < channels; ++i) {
				memcpy(reinterpret_cast<float*>(static_cast<uint8_t*>(samples) + i * line_size), data[i + channel_offset], sizeof(float) * size);
			}
			break;
		}
		default:
			DCPOMATIC_ASSERT(false);
		}

		DCPOMATIC_ASSERT(_codec_context->time_base.num == 1);
		frame->pts = sample_offset * _codec_context->time_base.den / _codec_context->sample_rate;

		r = avcodec_send_frame(_codec_context, frame);
		av_free(samples);
		av_frame_free(&frame);
		if (r < 0) {
			throw EncodeError(N_("avcodec_send_frame"), N_("ExportAudioStream::write"), r);
		}

		ffmpeg::Packet packet;
		r = avcodec_receive_packet(_codec_context, packet.get());
		if (r < 0 && r != AVERROR(EAGAIN)) {
			throw EncodeError(N_("avcodec_receive_packet"), N_("ExportAudioStream::write"), r);
		} else if (r >= 0) {
			packet->stream_index = _stream_index;
			av_interleaved_write_frame(_format_context, packet.get());
		}
	}

private:
	AVFormatContext* _format_context;
	AVCodec const * _codec;
	AVCodecContext* _codec_context;
	AVStream* _stream;
	int _stream_index;
};


FFmpegFileEncoder::FFmpegFileEncoder(
	dcp::Size video_frame_size,
	int video_frame_rate,
	int audio_frame_rate,
	int channels,
	ExportFormat format,
	bool audio_stream_per_channel,
	int x264_crf,
	boost::filesystem::path output
	)
	: _audio_stream_per_channel(audio_stream_per_channel)
	, _audio_channels(channels)
	, _output(output)
	, _video_frame_size(video_frame_size)
	, _video_frame_rate(video_frame_rate)
	, _audio_frame_rate(audio_frame_rate)
{
	_pixel_format = pixel_format(format);

	switch (format) {
	case ExportFormat::PRORES_4444:
		_sample_format = AV_SAMPLE_FMT_S32;
		_video_codec_name = "prores_ks";
		_audio_codec_name = "pcm_s24le";
		av_dict_set(&_video_options, "profile", "4", 0);
		av_dict_set(&_video_options, "threads", "auto", 0);
		break;
	case ExportFormat::PRORES_HQ:
		_sample_format = AV_SAMPLE_FMT_S32;
		_video_codec_name = "prores_ks";
		_audio_codec_name = "pcm_s24le";
		av_dict_set(&_video_options, "profile", "3", 0);
		av_dict_set(&_video_options, "threads", "auto", 0);
		break;
	case ExportFormat::PRORES_LT:
		_sample_format = AV_SAMPLE_FMT_S32;
		_video_codec_name = "prores_ks";
		_audio_codec_name = "pcm_s24le";
		av_dict_set(&_video_options, "profile", "1", 0);
		av_dict_set(&_video_options, "threads", "auto", 0);
		break;
	case ExportFormat::H264_AAC:
		_sample_format = AV_SAMPLE_FMT_FLTP;
		_video_codec_name = "libx264";
		_audio_codec_name = "aac";
		av_dict_set_int(&_video_options, "crf", x264_crf, 0);
		break;
	default:
		DCPOMATIC_ASSERT(false);
	}

	int r = avformat_alloc_output_context2(&_format_context, 0, 0, _output.string().c_str());
	if (!_format_context) {
		throw EncodeError(N_("avformat_alloc_output_context2"), "FFmpegFileEncoder::FFmpegFileEncoder", r);
	}

	setup_video();
	setup_audio();

	r = avio_open_boost(&_format_context->pb, _output, AVIO_FLAG_WRITE);
	if (r < 0) {
		throw EncodeError(fmt::format(_("Could not open output file {} ({})"), _output.string(), r));
	}

	AVDictionary* options = nullptr;

	r = avformat_write_header(_format_context, &options);
	if (r < 0) {
		throw EncodeError(N_("avformat_write_header"), N_("FFmpegFileEncoder::FFmpegFileEncoder"), r);
	}

	_pending_audio = make_shared<AudioBuffers>(channels, 0);
}


FFmpegFileEncoder::~FFmpegFileEncoder()
{
	_audio_streams.clear();
	avcodec_free_context(&_video_codec_context);
	avio_close(_format_context->pb);
	_format_context->pb = nullptr;
	avformat_free_context(_format_context);
}


AVPixelFormat
FFmpegFileEncoder::pixel_format(ExportFormat format)
{
	switch (format) {
	case ExportFormat::PRORES_4444:
		return AV_PIX_FMT_YUV444P10;
	case ExportFormat::PRORES_HQ:
	case ExportFormat::PRORES_LT:
		return AV_PIX_FMT_YUV422P10;
	case ExportFormat::H264_AAC:
		return AV_PIX_FMT_YUV420P;
	default:
		DCPOMATIC_ASSERT(false);
	}

	return AV_PIX_FMT_YUV422P10;
}


void
FFmpegFileEncoder::setup_video()
{
	_video_codec = avcodec_find_encoder_by_name(_video_codec_name.c_str());
	if (!_video_codec) {
		throw EncodeError(fmt::format("avcodec_find_encoder_by_name failed for {}", _video_codec_name));
	}

	_video_codec_context = avcodec_alloc_context3(_video_codec);
	if (!_video_codec_context) {
		throw std::bad_alloc();
	}

	/* Variable quantisation */
	_video_codec_context->global_quality = 0;
	_video_codec_context->width = _video_frame_size.width;
	_video_codec_context->height = _video_frame_size.height;
	_video_codec_context->time_base = (AVRational) { 1, _video_frame_rate };
	_video_codec_context->pix_fmt = _pixel_format;
	_video_codec_context->flags |= AV_CODEC_FLAG_QSCALE | AV_CODEC_FLAG_GLOBAL_HEADER;

	if (avcodec_open2(_video_codec_context, _video_codec, &_video_options) < 0) {
		throw EncodeError(N_("avcodec_open2"), N_("FFmpegFileEncoder::setup_video"));
	}

	_video_stream = avformat_new_stream(_format_context, _video_codec);
	if (!_video_stream) {
		throw EncodeError(N_("avformat_new_stream"), N_("FFmpegFileEncoder::setup_video"));
	}

	_video_stream->id = _video_stream_index;
	int r = avcodec_parameters_from_context(_video_stream->codecpar, _video_codec_context);
	if (r < 0) {
		throw EncodeError(N_("avcodec_parameters_from_context"), N_("FFmpegFileEncoder::setup_video"), r);
	}
}


void
FFmpegFileEncoder::setup_audio()
{
	int const streams = _audio_stream_per_channel ? _audio_channels : 1;
	int const channels_per_stream = _audio_stream_per_channel ? 1 : _audio_channels;

	for (int i = 0; i < streams; ++i) {
		_audio_streams.push_back(
			make_shared<ExportAudioStream>(
				_audio_codec_name, channels_per_stream, _audio_frame_rate, _sample_format, _format_context, _audio_stream_index_base + i
				)
			);
	}
}


void
FFmpegFileEncoder::flush()
{
	if (_pending_audio->frames() > 0) {
		audio_frame(_pending_audio->frames());
	}

	bool flushed_video = false;
	bool flushed_audio = false;

	while (!flushed_video || !flushed_audio) {
		int r = avcodec_send_frame(_video_codec_context, nullptr);
		if (r < 0 && r != AVERROR_EOF) {
			/* We get EOF if we've already flushed the stream once */
			throw EncodeError(N_("avcodec_send_frame"), N_("FFmpegFileEncoder::flush"), r);
		}

		ffmpeg::Packet packet;
		r = avcodec_receive_packet(_video_codec_context, packet.get());
		if (r == AVERROR_EOF) {
			flushed_video = true;
		} else if (r < 0) {
			throw EncodeError(N_("avcodec_receive_packet"), N_("FFmpegFileEncoder::flush"), r);
		} else {
			packet->stream_index = _video_stream_index;
			packet->duration = _video_stream->time_base.den / _video_frame_rate;
			av_interleaved_write_frame(_format_context, packet.get());
		}

		flushed_audio = true;
		for (auto const& i: _audio_streams) {
			if (!i->flush()) {
				flushed_audio = false;
			}
		}
	}

	auto const r = av_write_trailer(_format_context);
	if (r) {
		if (r == -ENOSPC) {
			throw DiskFullError(_output);
		} else {
			throw EncodeError(N_("av_write_trailer"), N_("FFmpegFileEncoder::flush"), r);
		}
	}
}


void
FFmpegFileEncoder::video(shared_ptr<PlayerVideo> video, DCPTime time)
{
	/* All our output formats are video range at the moment */
	auto image = video->image(force(_pixel_format), VideoRange::VIDEO, false);

	auto frame = av_frame_alloc();
	DCPOMATIC_ASSERT(frame);

	for (int i = 0; i < 3; ++i) {
		auto buffer = _pending_images.create_buffer(image, i);
		frame->buf[i] = av_buffer_ref(buffer);
		frame->data[i] = buffer->data;
		frame->linesize[i] = image->stride()[i];
		av_buffer_unref(&buffer);
	}

	frame->width = image->size().width;
	frame->height = image->size().height;
	frame->format = _pixel_format;
	DCPOMATIC_ASSERT(_video_stream->time_base.num == 1);
	frame->pts = time.get() * _video_stream->time_base.den / DCPTime::HZ;

	int r = avcodec_send_frame(_video_codec_context, frame);
	av_frame_free(&frame);
	if (r < 0) {
		throw EncodeError(N_("avcodec_send_frame"), N_("FFmpegFileEncoder::video"), r);
	}

	ffmpeg::Packet packet;
	r = avcodec_receive_packet(_video_codec_context, packet.get());
	if (r < 0 && r != AVERROR(EAGAIN)) {
		throw EncodeError(N_("avcodec_receive_packet"), N_("FFmpegFileEncoder::video"), r);
	} else if (r >= 0) {
		packet->stream_index = _video_stream_index;
		packet->duration = _video_stream->time_base.den / _video_frame_rate;
		av_interleaved_write_frame(_format_context, packet.get());
	}
}


/** Called when the player gives us some audio */
void
FFmpegFileEncoder::audio(shared_ptr<AudioBuffers> audio)
{
	_pending_audio->append(audio);

	DCPOMATIC_ASSERT(!_audio_streams.empty());
	int frame_size = _audio_streams[0]->frame_size();
	if (frame_size == 0) {
		/* codec has AV_CODEC_CAP_VARIABLE_FRAME_SIZE */
		frame_size = _audio_frame_rate / _video_frame_rate;
	}

	while (_pending_audio->frames() >= frame_size) {
		audio_frame(frame_size);
	}
}


void
FFmpegFileEncoder::audio_frame(int size)
{
	if (_audio_stream_per_channel) {
		int offset = 0;
		for (auto i: _audio_streams) {
			i->write(size, offset, 1, _pending_audio->data(), _audio_frames);
			++offset;
		}
	} else {
		DCPOMATIC_ASSERT(!_audio_streams.empty());
		DCPOMATIC_ASSERT(_pending_audio->channels());
		_audio_streams[0]->write(size, 0, _pending_audio->channels(), _pending_audio->data(), _audio_frames);
	}

	_pending_audio->trim_start(size);
	_audio_frames += size;
}


void
FFmpegFileEncoder::subtitle(PlayerText, DCPTimePeriod)
{

}
