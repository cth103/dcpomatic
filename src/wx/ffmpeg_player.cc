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

#include <wx/wx.h>
#include <wx/tglbtn.h>
#include <iostream>
#include <stdint.h>
extern "C" {
#include <libavcodec/avcodec.h>	
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
#include "wx/ffmpeg_player.h"

using namespace std;

FFmpegPlayer::FFmpegPlayer (wxWindow* parent)
	: _format_context (0)
	, _video_stream (-1)
	, _frame (0)
	, _video_codec_context (0)
	, _video_codec (0)
	, _scale_context (0)
	, _frame_valid (false)
	, _panel (new wxPanel (parent))
	, _slider (new wxSlider (parent, wxID_ANY, 0, 0, 4096))
	, _play_button (new wxToggleButton (parent, wxID_ANY, wxT ("Play")))
	, _panel_width (0)
	, _panel_height (0)
	, _full_width (0)
	, _full_height (0)
	, _top_crop_in_source (0)
	, _bottom_crop_in_source (0)
	, _left_crop_in_source (0)
	, _right_crop_in_source (0)
	, _ratio (1.85)
{
	_rgb[0] = 0;

	avcodec_register_all ();
	av_register_all ();
	
	_frame = avcodec_alloc_frame ();
	if (!_frame) {
		cerr << "could not allocate frame\n";
	}
	
	_panel->Bind (wxEVT_PAINT, &FFmpegPlayer::paint_panel, this);
	_panel->Bind (wxEVT_SIZE, &FFmpegPlayer::panel_sized, this);
	_slider->Bind (wxEVT_SCROLL_THUMBTRACK, &FFmpegPlayer::slider_moved, this);
	_slider->Bind (wxEVT_SCROLL_PAGEUP, &FFmpegPlayer::slider_moved, this);
	_slider->Bind (wxEVT_SCROLL_PAGEDOWN, &FFmpegPlayer::slider_moved, this);
	_play_button->Bind (wxEVT_COMMAND_TOGGLEBUTTON_CLICKED, &FFmpegPlayer::play_clicked, this);
	_timer.Bind (wxEVT_TIMER, &FFmpegPlayer::timer, this);
}

FFmpegPlayer::~FFmpegPlayer ()
{
	delete[] _rgb[0];
	
	if (_scale_context) {
		sws_freeContext (_scale_context);
	}
	
	if (_video_codec_context) {
		avcodec_close (_video_codec_context);
	}
	
	av_free (_frame);

	if (_format_context) {
		avformat_close_input (&_format_context);
	}
}

void
FFmpegPlayer::timer (wxTimerEvent& ev)
{
	if (!can_display ()) {
		return;
	}
	
	_panel->Refresh ();
	_panel->Update ();
	decode_frame ();
	convert_frame ();
}

void
FFmpegPlayer::update_panel ()
{
	_panel->Refresh ();
	_panel->Update ();
}

void
FFmpegPlayer::decode_frame ()
{
	assert (_format_context);
	
	while (1) {
		int r = av_read_frame (_format_context, &_packet);
		if (r < 0) {
			return;
		}
		
		avcodec_get_frame_defaults (_frame);
		if (_packet.stream_index == _video_stream) {
			int frame_finished;
			int const r = avcodec_decode_video2 (_video_codec_context, _frame, &frame_finished, &_packet);
			if (r >= 0 && frame_finished) {
				_frame_valid = true;
				av_free_packet (&_packet);
				return;
			}
		}
		
		av_free_packet (&_packet);
	}
}

void
FFmpegPlayer::convert_frame ()
{
	if (!_scale_context || !_rgb[0] || !_frame_valid) {
		return;
	}
	
	sws_scale (
		_scale_context,
		_frame->data, _frame->linesize,
		0, _video_codec_context->height,
		_rgb, _rgb_stride
		);
	
	uint8_t* in = _rgb[0];
	uint8_t* out = _rgb[0];
	
	in += top_crop_in_view() * _full_width * 3;
	for (int y = 0; y < cropped_height_in_view(); ++y) {
		/* in is the start of the appropriate full-width line */
		memmove (out, in + left_crop_in_view() * 3, cropped_width_in_view() * 3);
		in += _full_width * 3;
		out += cropped_width_in_view() * 3;
	}
}

void
FFmpegPlayer::paint_panel (wxPaintEvent& ev)
{
	wxPaintDC dc (_panel);
	
	wxImage i (cropped_width_in_view(), cropped_height_in_view(), _rgb[0], true);
	wxBitmap b (i);
	dc.DrawBitmap (b, 0, 0);
}

void
FFmpegPlayer::slider_moved (wxCommandEvent& ev)
{
	if (!can_display ()) {
		return;
	}
	
	int const video_length_in_frames = (double(_format_context->duration) / AV_TIME_BASE) * frames_per_second();
	int const new_frame = video_length_in_frames * _slider->GetValue() / 4096;
	int64_t const t = static_cast<int64_t>(new_frame) / (av_q2d (_format_context->streams[_video_stream]->time_base) * frames_per_second());
	av_seek_frame (_format_context, _video_stream, t, 0);
	avcodec_flush_buffers (_video_codec_context);

	decode_frame ();
	convert_frame ();
	update_panel ();
}

float
FFmpegPlayer::frames_per_second () const
{
	assert (_format_context);
	
	AVStream* s = _format_context->streams[_video_stream];
	
	if (s->avg_frame_rate.num && s->avg_frame_rate.den) {
		return av_q2d (s->avg_frame_rate);
	}
	
	return av_q2d (s->r_frame_rate);
}

void
FFmpegPlayer::allocate_buffer_and_scaler ()
{
	if (!_format_context || !_panel_width || !_panel_height) {
		return;
	}

	float const panel_ratio = static_cast<float> (_panel_width) / _panel_height;
	
	int new_width;
	int new_height;
	if (panel_ratio < _ratio) {
		/* panel is less widescreen than the target ratio; clamp width */
		new_width = _panel_width;
		new_height = new_width / _ratio;
	} else {
		/* panel is more widescreen than the target ratio; clamp height */
		new_height = _panel_height;
		new_width = new_height * _ratio;
	}

        if (new_width == 0 || new_height == 0) {
	     return;
	}
	
	_full_height = new_height + ((_top_crop_in_source + _bottom_crop_in_source) * height_source_to_view_scaling());
	_full_width = new_width + ((_left_crop_in_source + _right_crop_in_source) * width_source_to_view_scaling());
	
	delete[] _rgb[0];
	
	_rgb[0] = new uint8_t[_full_width * _full_height * 3];
        memset (_rgb[0], 0, _full_width * _full_height * 3);
	_rgb_stride[0] = _full_width * 3;
	
	if (_scale_context) {
		sws_freeContext (_scale_context);
	}
	
	_scale_context = sws_getContext (
		_video_codec_context->width, _video_codec_context->height, _video_codec_context->pix_fmt,
		_full_width, _full_height, PIX_FMT_RGB24,
		SWS_BICUBIC, 0, 0, 0
		);

	if (!_scale_context) {
		cout << "could not allocate sc\n";
	}
}

float
FFmpegPlayer::width_source_to_view_scaling () const
{
	return static_cast<float> (_full_width) / _video_codec_context->width;
}

float
FFmpegPlayer::height_source_to_view_scaling () const
{
	return static_cast<float> (_full_height) / _video_codec_context->height;
}

int
FFmpegPlayer::cropped_width_in_view () const
{
	return _full_width - ((_left_crop_in_source + _right_crop_in_source) * width_source_to_view_scaling());
}

int
FFmpegPlayer::cropped_height_in_view () const
{
	return _full_height - ((_top_crop_in_source + _bottom_crop_in_source) * height_source_to_view_scaling());
}

int
FFmpegPlayer::left_crop_in_view () const
{
	return _left_crop_in_source * width_source_to_view_scaling();
}

int
FFmpegPlayer::top_crop_in_view () const
{
	return _top_crop_in_source * height_source_to_view_scaling();
}

void
FFmpegPlayer::panel_sized (wxSizeEvent& ev)
{
	_panel_width = ev.GetSize().GetWidth();
	_panel_height = ev.GetSize().GetHeight();
	allocate_buffer_and_scaler ();

	convert_frame ();
	update_panel ();
}

void
FFmpegPlayer::set_file (string f)
{
	if (_video_codec_context) {
		avcodec_close (_video_codec_context);
	}

	if (_format_context) {
		avformat_close_input (&_format_context);
	}
	
	if (avformat_open_input (&_format_context, f.c_str(), 0, 0) < 0) {
		cerr << "avformat_open_input failed.\n";
	}
	
	if (avformat_find_stream_info (_format_context, 0) < 0) {
		cerr << "avformat_find_stream_info failed.\n";
	}
	
	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		AVStream* s = _format_context->streams[i];
		if (s->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			_video_stream = i;
		} else if (s->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
			/* XXX */
		}
	}
	
	if (_video_stream < 0) {
		cerr << "could not find video stream\n";
	}
	
	_video_codec_context = _format_context->streams[_video_stream]->codec;
	_video_codec = avcodec_find_decoder (_video_codec_context->codec_id);
	
	if (_video_codec == 0) {
		cerr << "could not find video decoder\n";
	}
	
	if (avcodec_open2 (_video_codec_context, _video_codec, 0) < 0) {
		cerr << "could not open video decoder\n";
	}

	allocate_buffer_and_scaler ();
	check_play_state ();
	update_panel ();
}

void
FFmpegPlayer::set_top_crop (int t)
{
	_top_crop_in_source = t;

	allocate_buffer_and_scaler ();
	convert_frame ();
	update_panel ();
}

void
FFmpegPlayer::set_bottom_crop (int b)
{
	_bottom_crop_in_source = b;

	allocate_buffer_and_scaler ();
	convert_frame ();
	update_panel ();
}

void
FFmpegPlayer::set_left_crop (int l)
{
	_left_crop_in_source = l;

	allocate_buffer_and_scaler ();
	convert_frame ();
	update_panel ();
}

void
FFmpegPlayer::set_right_crop (int r)
{
	_right_crop_in_source = r;

	allocate_buffer_and_scaler ();
	convert_frame ();
	update_panel ();
}

void
FFmpegPlayer::set_ratio (float r)
{
	_ratio = r;

	allocate_buffer_and_scaler ();
	convert_frame ();
	update_panel ();
}

void
FFmpegPlayer::play_clicked (wxCommandEvent &)
{
	check_play_state ();
}

void
FFmpegPlayer::check_play_state ()
{
	if (_play_button->GetValue()) {
		_timer.Start (1000 / frames_per_second());
	} else {
		_timer.Stop ();
	}
}

bool
FFmpegPlayer::can_display () const
{
	return (_format_context && _scale_context);
}
	
