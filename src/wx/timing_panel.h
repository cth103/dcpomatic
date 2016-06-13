/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "content_sub_panel.h"
#include "timecode.h"

class FilmViewer;

class TimingPanel : public ContentSubPanel
{
public:
	TimingPanel (ContentPanel *, FilmViewer* viewer);

	void film_changed (Film::Property);
	void film_content_changed (int);
	void content_selection_changed ();

private:
	void position_changed ();
	void move_to_start_of_reel_clicked ();
	void full_length_changed ();
	void trim_start_changed ();
	void trim_start_to_playhead_clicked ();
	void trim_end_changed ();
	void trim_end_to_playhead_clicked ();
	void play_length_changed ();
	void video_frame_rate_changed ();
	void set_video_frame_rate ();
	void update_full_length ();
	void update_play_length ();
	void setup_sensitivity ();

	FilmViewer* _viewer;

	Timecode<DCPTime>* _position;
	wxButton* _move_to_start_of_reel;
	Timecode<DCPTime>* _full_length;
	Timecode<ContentTime>* _trim_start;
	wxButton* _trim_start_to_playhead;
	wxButton* _trim_end_to_playhead;
	Timecode<ContentTime>* _trim_end;
	Timecode<DCPTime>* _play_length;
	wxTextCtrl* _video_frame_rate;
	wxButton* _set_video_frame_rate;
};
