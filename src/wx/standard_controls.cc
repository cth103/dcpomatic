/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#include "film_viewer.h"
#include "standard_controls.h"
#include <wx/tglbtn.h>
#include <wx/wx.h>


using std::shared_ptr;


StandardControls::StandardControls (wxWindow* parent, shared_ptr<FilmViewer> viewer, bool editor_controls)
	: Controls (parent, viewer, editor_controls)
	, _play_button (new wxToggleButton(this, wxID_ANY, _("Play")))
{
	_button_sizer->Add (_play_button, 0, wxALL | wxALIGN_CENTER_VERTICAL, 2);
	_play_button->Bind (wxEVT_TOGGLEBUTTON, boost::bind(&StandardControls::play_clicked, this));
}


void
StandardControls::started ()
{
	Controls::started ();
	_play_button->SetValue (true);
}


void
StandardControls::stopped ()
{
	Controls::stopped ();
	_play_button->SetValue (false);
}


void
StandardControls::play_clicked ()
{
	check_play_state ();
}


void
StandardControls::check_play_state ()
{
	auto viewer = _viewer.lock ();
	if (!_film || _film->video_frame_rate() == 0 || !viewer) {
		return;
	}

	if (_play_button->GetValue()) {
		viewer->start ();
	} else {
		viewer->stop ();
	}
}


void
StandardControls::setup_sensitivity ()
{
	Controls::setup_sensitivity ();
	bool const active_job = _active_job && *_active_job != "examine_content";
	_play_button->Enable (_film && !_film->content().empty() && !active_job);
}


void
StandardControls::play ()
{
	_play_button->SetValue (true);
	play_clicked ();
}


void
StandardControls::stop ()
{
	_play_button->SetValue (false);
	play_clicked ();
}
