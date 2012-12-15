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

/** @file  src/film_viewer.cc
 *  @brief A wx widget to view a preview of a Film.
 */

#include <iostream>
#include <iomanip>
#include <wx/tglbtn.h>
#include "lib/film.h"
#include "lib/format.h"
#include "lib/util.h"
#include "lib/job_manager.h"
#include "lib/options.h"
#include "lib/subtitle.h"
#include "film_viewer.h"
#include "wx_util.h"
#include "ffmpeg_player.h"

using std::string;
using std::pair;
using std::max;
using boost::shared_ptr;

FilmViewer::FilmViewer (shared_ptr<Film> f, wxWindow* p)
	: wxPanel (p)
	, _player (new FFmpegPlayer (this))
{
	wxBoxSizer* v_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (v_sizer);

	v_sizer->Add (_player->panel(), 1, wxEXPAND);

	wxBoxSizer* h_sizer = new wxBoxSizer (wxHORIZONTAL);
	h_sizer->Add (_player->play_button(), 0, wxEXPAND);
	h_sizer->Add (_player->slider(), 1, wxEXPAND);

	v_sizer->Add (h_sizer, 0, wxEXPAND);

	set_film (_film);
}

void
FilmViewer::film_changed (Film::Property p)
{
	ensure_ui_thread ();
	
	switch (p) {
		
	case Film::CONTENT:
		_player->set_file (_film->content_path ());
		break;
		
	case Film::CROP:
	{
		Crop c = _film->crop ();
		_player->set_left_crop (c.left);
		_player->set_right_crop (c.right);
		_player->set_top_crop (c.top);
		_player->set_bottom_crop (c.bottom);
	}
	break;

	case Film::FORMAT:
		if (_film->format()) {
			_player->set_ratio (_film->format()->ratio_as_float(_film));
		}
		break;
		
	default:
		break;
	}
}

void
FilmViewer::set_film (shared_ptr<Film> f)
{
	if (_film == f) {
		return;
	}
	
	_film = f;

	if (!_film) {
		return;
	}

	_film->Changed.connect (bind (&FilmViewer::film_changed, this, _1));
	film_changed (Film::CONTENT);
	film_changed (Film::CROP);
	film_changed (Film::FORMAT);
}
