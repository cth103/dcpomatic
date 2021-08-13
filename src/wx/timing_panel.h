/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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
#include "suspender.h"

class FilmViewer;

class TimingPanel : public ContentSubPanel
{
public:
	TimingPanel (ContentPanel *, std::weak_ptr<FilmViewer> viewer);

	void create () override;
	void film_changed (Film::Property) override;
	void film_content_changed (int) override;
	void content_selection_changed () override;

private:
	void position_changed ();
	void move_to_start_of_reel_clicked ();
	void full_length_changed ();
	void trim_start_changed ();
	void trim_start_to_playhead_clicked ();
	void trim_end_changed ();
	void trim_end_to_playhead_clicked ();
	void play_length_changed ();
	void update_full_length ();
	void update_play_length ();
	void setup_sensitivity ();
	void add_to_grid () override;

	std::weak_ptr<FilmViewer> _viewer;

	wxStaticText* _h_label;
	wxStaticText* _m_label;
	wxStaticText* _s_label;
	wxStaticText* _f_label;
	wxStaticText* _colon[3];
	wxStaticText* _position_label;
	Timecode<dcpomatic::DCPTime>* _position;
	wxButton* _move_to_start_of_reel;
	wxStaticText* _full_length_label;
	Timecode<dcpomatic::DCPTime>* _full_length;
	wxStaticText* _trim_start_label;
	Timecode<dcpomatic::ContentTime>* _trim_start;
	wxButton* _trim_start_to_playhead;
	wxButton* _trim_end_to_playhead;
	wxStaticText* _trim_end_label;
	Timecode<dcpomatic::ContentTime>* _trim_end;
	wxStaticText* _play_length_label;
	Timecode<dcpomatic::DCPTime>* _play_length;

	Suspender _film_content_changed_suspender;
};
