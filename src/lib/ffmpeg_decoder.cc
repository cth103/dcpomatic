/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file  src/ffmpeg_decoder.cc
 *  @brief A decoder using FFmpeg to decode content.
 */

#include <stdexcept>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <stdint.h>
extern "C" {
#include <tiffio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libpostproc/postprocess.h>
}
#include <sndfile.h>
#include "film.h"
#include "format.h"
#include "transcoder.h"
#include "job.h"
#include "filter.h"
#include "film_state.h"
#include "options.h"
#include "exceptions.h"
#include "image.h"
#include "util.h"
#include "log.h"
#include "ffmpeg_decoder.h"

using namespace std;
using namespace boost;

FFmpegDecoder::FFmpegDecoder (boost::shared_ptr<const FilmState> s, boost::shared_ptr<const Options> o, Job* j, Log* l, bool minimal, bool ignore_length)
	: Decoder (s, o, j, l, minimal, ignore_length)
	, _format_context (0)
	, _video_stream (-1)
	, _audio_stream (-1)
	, _subtitle_stream (-1)
	, _frame (0)
	, _video_codec_context (0)
	, _video_codec (0)
	, _audio_codec_context (0)
	, _audio_codec (0)
	, _subtitle_codec_context (0)
	, _subtitle_codec (0)
	, _have_subtitle (false)
{
	setup_general ();
	setup_video ();
	setup_audio ();
	setup_subtitle ();
}

FFmpegDecoder::~FFmpegDecoder ()
{
	if (_audio_codec_context) {
		avcodec_close (_audio_codec_context);
	}
	
	if (_video_codec_context) {
		avcodec_close (_video_codec_context);
	}

	if (_have_subtitle) {
		avsubtitle_free (&_subtitle);
	}

	if (_subtitle_codec_context) {
		avcodec_close (_subtitle_codec_context);
	}
	
	av_free (_frame);
	avformat_close_input (&_format_context);
}	

void
FFmpegDecoder::setup_general ()
{
	int r;
	
	av_register_all ();

	if ((r = avformat_open_input (&_format_context, _fs->content_path().c_str(), 0, 0)) != 0) {
		throw OpenFileError (_fs->content_path ());
	}

	if (avformat_find_stream_info (_format_context, 0) < 0) {
		throw DecodeError ("could not find stream information");
	}

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		if (_format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			_video_stream = i;
		} else if (_format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			_audio_stream = i;
		} else if (_format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
			_subtitle_stream = i;
		}
	}

	if (_video_stream < 0) {
		throw DecodeError ("could not find video stream");
	}

	_frame = avcodec_alloc_frame ();
	if (_frame == 0) {
		throw DecodeError ("could not allocate frame");
	}
}

void
FFmpegDecoder::setup_video ()
{
	_video_codec_context = _format_context->streams[_video_stream]->codec;
	_video_codec = avcodec_find_decoder (_video_codec_context->codec_id);

	if (_video_codec == 0) {
		throw DecodeError ("could not find video decoder");
	}
	
	if (avcodec_open2 (_video_codec_context, _video_codec, 0) < 0) {
		throw DecodeError ("could not open video decoder");
	}
}

void
FFmpegDecoder::setup_audio ()
{
	if (_audio_stream < 0) {
		return;
	}
	
	_audio_codec_context = _format_context->streams[_audio_stream]->codec;
	_audio_codec = avcodec_find_decoder (_audio_codec_context->codec_id);

	if (_audio_codec == 0) {
		throw DecodeError ("could not find audio decoder");
	}

	if (avcodec_open2 (_audio_codec_context, _audio_codec, 0) < 0) {
		throw DecodeError ("could not open audio decoder");
	}

	/* This is a hack; sometimes it seems that _audio_codec_context->channel_layout isn't set up,
	   so bodge it here.  No idea why we should have to do this.
	*/

	if (_audio_codec_context->channel_layout == 0) {
		_audio_codec_context->channel_layout = av_get_default_channel_layout (audio_channels ());
	}
}

void
FFmpegDecoder::setup_subtitle ()
{
	if (_subtitle_stream < 0) {
		return;
	}

	_subtitle_codec_context = _format_context->streams[_subtitle_stream]->codec;
	_subtitle_codec = avcodec_find_decoder (_subtitle_codec_context->codec_id);

	if (_subtitle_codec == 0) {
		throw DecodeError ("could not find subtitle decoder");
	}
	
	if (avcodec_open2 (_subtitle_codec_context, _subtitle_codec, 0) < 0) {
		throw DecodeError ("could not open subtitle decoder");
	}
}


bool
FFmpegDecoder::do_pass ()
{
	int r = av_read_frame (_format_context, &_packet);
	if (r < 0) {
		if (r != AVERROR_EOF) {
			throw DecodeError ("error on av_read_frame");
		}
		
		/* Get any remaining frames */
		
		_packet.data = 0;
		_packet.size = 0;

		int frame_finished;

		if (_opt->decode_video) {
			while (avcodec_decode_video2 (_video_codec_context, _frame, &frame_finished, &_packet) >= 0 && frame_finished) {

				process_video (_frame);
			}
		}

		if (_audio_stream >= 0 && _opt->decode_audio) {
			while (avcodec_decode_audio4 (_audio_codec_context, _frame, &frame_finished, &_packet) >= 0 && frame_finished) {
				int const data_size = av_samples_get_buffer_size (
					0, _audio_codec_context->channels, _frame->nb_samples, audio_sample_format (), 1
					);
				
				assert (_audio_codec_context->channels == _fs->audio_channels);
				process_audio (_frame->data[0], data_size);
			}
		}

		return true;
	}

	if (_packet.stream_index == _video_stream && _opt->decode_video) {

		int frame_finished;
		if (avcodec_decode_video2 (_video_codec_context, _frame, &frame_finished, &_packet) >= 0 && frame_finished) {
			
			cout << "decoded some video.\n";
			if (_have_subtitle) {
				cout << "have a subtitle; " << _subtitle.num_rects << "\n";
				for (unsigned int i = 0; i < _subtitle.num_rects; ++i) {
					AVSubtitleRect* rect = _subtitle.rects[i];
					if (rect->type != SUBTITLE_BITMAP) {
						cout << "not a bitmap\n";
						throw DecodeError ("non-bitmap subtitles not yet supported");
					}

					/* XXX: all this assumes YUV420 in _frame */
					
					assert (rect->nb_colors == 4);
					assert (rect->pict.data[0]);

					/* Start of the first line in the target frame */
					uint8_t* frame_y_p = _frame->data[0] + rect->y * _frame->linesize[0];
					uint8_t* frame_u_p = _frame->data[1] + (rect->y / 2) * _frame->linesize[1];
					uint8_t* frame_v_p = _frame->data[2] + (rect->y / 2) * _frame->linesize[2];
					
					/* Start of the first line in the subtitle */
					uint8_t* sub_p = rect->pict.data[0];

					cout << "frame ls 0 is " << _frame->linesize[0] << "\n";
					cout << "frame ls 1 is " << _frame->linesize[1] << "\n";
					cout << "frame ls 2 is " << _frame->linesize[2] << "\n";

					uint32_t* palette = (uint32_t *) rect->pict.data[1];
					
					for (int sub_y = 0; sub_y < rect->h; ++sub_y) {
						uint8_t* sub_line_p = sub_p;
						uint8_t* frame_line_y_p = frame_y_p + rect->x;
						uint8_t* frame_line_u_p = frame_u_p + (rect->x / 2);
						uint8_t* frame_line_v_p = frame_v_p + (rect->x / 2);

						uint8_t current_u = 0;
						uint8_t current_v = 0;
						int subsample_step = 0;
						
						for (int sub_x = 0; sub_x < rect->w; ++sub_x) {

							uint32_t const val = palette[*sub_line_p++];
							int const   red = (val &       0xff);
							int const green = (val &     0xff00) >> 8;
							int const  blue = (val &   0xff0000) >> 16;
							int const alpha = (val & 0xff000000) >> 24;

							if (alpha) {
								*frame_line_y_p = RGB_TO_Y_CCIR (red, green, blue);
							}
							frame_line_y_p++;
							
							current_u |= ((RGB_TO_U_CCIR (red, green, blue, 0) & 0xf0) >> 4) << (4 * subsample_step);
							current_v |= ((RGB_TO_V_CCIR (red, green, blue, 0) & 0xf0) >> 4) << (4 * subsample_step);

							if (subsample_step == 1 && (sub_y % 2) == 0) {
								if (alpha) {
									*frame_line_u_p = current_u;
									*frame_line_v_p = current_v;
								}
								frame_line_u_p++;
								frame_line_v_p++;
								current_u = current_v = 0;
							}

							subsample_step = (subsample_step + 1) % 2;
						}

						sub_p += rect->pict.linesize[0];
						frame_y_p += _frame->linesize[0];
						if ((sub_y % 2) == 0) {
							frame_u_p += _frame->linesize[1];
							frame_v_p += _frame->linesize[2];
						}
					}
				}
			}
			
			process_video (_frame);
		}

	} else if (_audio_stream >= 0 && _packet.stream_index == _audio_stream && _opt->decode_audio) {
		
		avcodec_get_frame_defaults (_frame);
		
		int frame_finished;
		if (avcodec_decode_audio4 (_audio_codec_context, _frame, &frame_finished, &_packet) >= 0 && frame_finished) {
			int const data_size = av_samples_get_buffer_size (
				0, _audio_codec_context->channels, _frame->nb_samples, audio_sample_format (), 1
				);

			assert (_audio_codec_context->channels == _fs->audio_channels);
			process_audio (_frame->data[0], data_size);
		}

	} else if (_subtitle_stream >= 0 && _packet.stream_index == _subtitle_stream) {

		if (_have_subtitle) {
			avsubtitle_free (&_subtitle);
			_have_subtitle = false;
		}

		int got_subtitle;
		if (avcodec_decode_subtitle2 (_subtitle_codec_context, &_subtitle, &got_subtitle, &_packet) && got_subtitle) {
			cout << "got a subtitle.\n";
			_have_subtitle = true;
		}
	}
	
	av_free_packet (&_packet);
	return false;
}

int
FFmpegDecoder::length_in_frames () const
{
	return (_format_context->duration / AV_TIME_BASE) * frames_per_second ();
}

float
FFmpegDecoder::frames_per_second () const
{
	return av_q2d (_format_context->streams[_video_stream]->avg_frame_rate);
}

int
FFmpegDecoder::audio_channels () const
{
	if (_audio_codec_context == 0) {
		return 0;
	}

	return _audio_codec_context->channels;
}

int
FFmpegDecoder::audio_sample_rate () const
{
	if (_audio_codec_context == 0) {
		return 0;
	}
	
	return _audio_codec_context->sample_rate;
}

AVSampleFormat
FFmpegDecoder::audio_sample_format () const
{
	if (_audio_codec_context == 0) {
		return (AVSampleFormat) 0;
	}
	
	return _audio_codec_context->sample_fmt;
}

int64_t
FFmpegDecoder::audio_channel_layout () const
{
	if (_audio_codec_context == 0) {
		return 0;
	}
	
	return _audio_codec_context->channel_layout;
}

Size
FFmpegDecoder::native_size () const
{
	return Size (_video_codec_context->width, _video_codec_context->height);
}

PixelFormat
FFmpegDecoder::pixel_format () const
{
	return _video_codec_context->pix_fmt;
}

int
FFmpegDecoder::time_base_numerator () const
{
	return _video_codec_context->time_base.num;
}

int
FFmpegDecoder::time_base_denominator () const
{
	return _video_codec_context->time_base.den;
}

int
FFmpegDecoder::sample_aspect_ratio_numerator () const
{
	return _video_codec_context->sample_aspect_ratio.num;
}

int
FFmpegDecoder::sample_aspect_ratio_denominator () const
{
	return _video_codec_context->sample_aspect_ratio.den;
}

