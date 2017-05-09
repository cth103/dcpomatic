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

#include "ffmpeg_transcoder.h"
#include "film.h"
#include "job.h"
#include "player.h"
#include "player_video.h"
#include "log.h"
#include "image.h"
#include "compose.hpp"
#include <iostream>

#include "i18n.h"

using std::string;
using std::runtime_error;
using std::cout;
using boost::shared_ptr;
using boost::bind;
using boost::weak_ptr;

static AVPixelFormat
force_pixel_format (AVPixelFormat, AVPixelFormat out)
{
	return out;
}

FFmpegTranscoder::FFmpegTranscoder (shared_ptr<const Film> film, weak_ptr<Job> job)
	: Transcoder (film, job)
	, _pixel_format (AV_PIX_FMT_YUV422P10)
	, _history (1000)
{

}

void
FFmpegTranscoder::go ()
{
	string const codec_name = "prores_ks";
	AVCodec* codec = avcodec_find_encoder_by_name (codec_name.c_str());
	if (!codec) {
		throw runtime_error (String::compose ("could not find FFmpeg codec %1", codec_name));
	}

	_codec_context = avcodec_alloc_context3 (codec);
	if (!_codec_context) {
		throw runtime_error ("could not allocate FFmpeg context");
	}

	avcodec_get_context_defaults3 (_codec_context, codec);

	/* Variable quantisation */
	_codec_context->global_quality = 0;
	_codec_context->width = _film->frame_size().width;
	_codec_context->height = _film->frame_size().height;
	_codec_context->time_base = (AVRational) { 1, _film->video_frame_rate() };
	_codec_context->pix_fmt = _pixel_format;
	_codec_context->flags |= CODEC_FLAG_QSCALE | CODEC_FLAG_GLOBAL_HEADER;

	boost::filesystem::path filename = _film->file(_film->isdcf_name(true) + ".mov");
	avformat_alloc_output_context2 (&_format_context, 0, 0, _output.string().c_str());
	if (!_format_context) {
		throw runtime_error ("could not allocate FFmpeg format context");
	}

	_video_stream = avformat_new_stream (_format_context, codec);
	if (!_video_stream) {
		throw runtime_error ("could not create FFmpeg output video stream");
	}

	/* Note: needs to increment with each stream */
	_video_stream->id = 0;
	_video_stream->codec = _codec_context;

	AVDictionary* options = 0;
	av_dict_set (&options, "profile", "3", 0);
	av_dict_set (&options, "threads", "auto", 0);

	if (avcodec_open2 (_codec_context, codec, &options) < 0) {
		throw runtime_error ("could not open FFmpeg codec");
	}

	if (avio_open (&_format_context->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0) {
		throw runtime_error ("could not open FFmpeg output file");
	}

	if (avformat_write_header (_format_context, &options) < 0) {
		throw runtime_error ("could not write header to FFmpeg output file");
	}

	{
		shared_ptr<Job> job = _job.lock ();
		DCPOMATIC_ASSERT (job);
		job->sub (_("Encoding"));
	}

	while (!_player->pass ()) {}

	while (true) {
		AVPacket packet;
		av_init_packet (&packet);
		packet.data = 0;
		packet.size = 0;

		int got_packet;
		avcodec_encode_video2 (_codec_context, &packet, 0, &got_packet);
		if (!got_packet) {
			break;
		}

		packet.stream_index = 0;
		av_interleaved_write_frame (_format_context, &packet);
		av_packet_unref (&packet);
	}

	av_write_trailer (_format_context);

	avcodec_close (_codec_context);
	avio_close (_format_context->pb);
	avformat_free_context (_format_context);
}

void
FFmpegTranscoder::video (shared_ptr<PlayerVideo> video, DCPTime time)
{
	shared_ptr<Image> image = video->image (
		bind (&Log::dcp_log, _film->log().get(), _1, _2),
		bind (&force_pixel_format, _1, _pixel_format),
		true,
		false
		);

	AVFrame* frame = av_frame_alloc ();

	for (int i = 0; i < 3; ++i) {
		size_t const size = image->stride()[i] * image->size().height;
		AVBufferRef* buffer = av_buffer_alloc (size);
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
	{
		boost::mutex::scoped_lock lm (_mutex);
		_last_frame = time.frames_round(_film->video_frame_rate());
		frame->pts = _last_frame / (_film->video_frame_rate() * av_q2d (_video_stream->time_base));
	}

	AVPacket packet;
	av_init_packet (&packet);
	packet.data = 0;
	packet.size = 0;

	int got_packet;
	if (avcodec_encode_video2 (_codec_context, &packet, frame, &got_packet) < 0) {
		throw EncodeError ("FFmpeg video encode failed");
	}

	if (got_packet && packet.size) {
		/* XXX: this should not be hard-wired */
		packet.stream_index = 0;
		av_interleaved_write_frame (_format_context, &packet);
		av_packet_unref (&packet);
	}

	av_frame_free (&frame);

	_history.event ();

	shared_ptr<Job> job = _job.lock ();
	if (job) {
		job->set_progress (float(time.get()) / _film->length().get());
	}
}

void
FFmpegTranscoder::audio (shared_ptr<AudioBuffers> audio, DCPTime time)
{

}

void
FFmpegTranscoder::subtitle (PlayerSubtitles subs, DCPTimePeriod period)
{

}

float
FFmpegTranscoder::current_encoding_rate () const
{
	return _history.rate ();
}

int
FFmpegTranscoder::video_frames_enqueued () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _last_frame;
}
