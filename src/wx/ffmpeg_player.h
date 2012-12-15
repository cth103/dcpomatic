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
#include <stdint.h>
extern "C" {
#include <libavcodec/avcodec.h>	
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

class wxToggleButton;

class FFmpegPlayer
{
public:
	FFmpegPlayer (wxWindow* parent);
	~FFmpegPlayer ();

	void set_file (std::string);

	wxPanel* panel () const {
		return _panel;
	}

	wxSlider* slider () const {
		return _slider;
	}

	wxToggleButton* play_button () const {
		return _play_button;
	}

	void set_top_crop (int t);
	void set_bottom_crop (int b);
	void set_left_crop (int l);
	void set_right_crop (int r);
	void set_ratio (float r);

private:
	void timer (wxTimerEvent& ev);
	void decode_frame ();
	void convert_frame ();
	void paint_panel (wxPaintEvent& ev);
	void slider_moved (wxCommandEvent& ev);
	float frames_per_second () const;
	void allocate_buffer_and_scaler ();
	float width_source_to_view_scaling () const;
	float height_source_to_view_scaling () const;
	int cropped_width_in_view () const;
	int cropped_height_in_view () const;
	int left_crop_in_view () const;
	int top_crop_in_view () const;
	void panel_sized (wxSizeEvent &);
	void play_clicked (wxCommandEvent &);
	void check_play_state ();
	void update_panel ();
	bool can_display () const;

	AVFormatContext* _format_context;
	int _video_stream;
	AVFrame* _frame;
	AVCodecContext* _video_codec_context;
	AVCodec* _video_codec;
	AVPacket _packet;
	struct SwsContext* _scale_context;
	bool _frame_valid;
	uint8_t* _rgb[1];
	int _rgb_stride[1];

	wxPanel* _panel;
	wxSlider* _slider;
	wxToggleButton* _play_button;
	
	wxTimer _timer;
	int _panel_width;
	int _panel_height;
	int _full_width;
	int _full_height;
	int _top_crop_in_source;
	int _bottom_crop_in_source;
	int _left_crop_in_source;
	int _right_crop_in_source;
	float _ratio;
};
