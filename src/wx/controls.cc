/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "controls.h"
#include "film_viewer.h"
#include "wx_util.h"
#include "playhead_to_timecode_dialog.h"
#include "playhead_to_frame_dialog.h"
#include "content_view.h"
#include "static_text.h"
#include "check_box.h"
#include "dcpomatic_button.h"
#include "lib/job_manager.h"
#include "lib/player_video.h"
#include "lib/dcp_content.h"
#include "lib/job.h"
#include "lib/examine_content_job.h"
#include "lib/content_factory.h"
#include "lib/cross.h"
#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <wx/wx.h>
#include <wx/tglbtn.h>
#include <wx/listctrl.h>
#include <wx/progdlg.h>

using std::string;
using std::list;
using std::cout;
using std::make_pair;
using std::exception;
using boost::optional;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using namespace dcpomatic;

Controls::Controls (wxWindow* parent, shared_ptr<FilmViewer> viewer, bool editor_controls)
	: wxPanel (parent)
	, _slider (new wxSlider (this, wxID_ANY, 0, 0, 4096))
	, _viewer (viewer)
	, _slider_being_moved (false)
	, _outline_content (0)
	, _eye (0)
	, _jump_to_selected (0)
	, _rewind_button (new Button (this, wxT("|<")))
	, _back_button (new Button (this, wxT("<")))
	, _forward_button (new Button (this, wxT(">")))
	, _frame_number (new StaticText (this, wxT("")))
	, _timecode (new StaticText (this, wxT("")))
{
	_v_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (_v_sizer);

	wxBoxSizer* view_options = new wxBoxSizer (wxHORIZONTAL);
	if (editor_controls) {
		_outline_content = new CheckBox (this, _("Outline content"));
		view_options->Add (_outline_content, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_GAP);
		_eye = new wxChoice (this, wxID_ANY);
		_eye->Append (_("Left"));
		_eye->Append (_("Right"));
		_eye->SetSelection (0);
		view_options->Add (_eye, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_GAP);
		_jump_to_selected = new CheckBox (this, _("Jump to selected content"));
		view_options->Add (_jump_to_selected, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_GAP);
	}

	_v_sizer->Add (view_options, 0, wxALL, DCPOMATIC_SIZER_GAP);

	wxBoxSizer* h_sizer = new wxBoxSizer (wxHORIZONTAL);

	wxBoxSizer* time_sizer = new wxBoxSizer (wxVERTICAL);
	time_sizer->Add (_frame_number, 0, wxEXPAND);
	time_sizer->Add (_timecode, 0, wxEXPAND);

	h_sizer->Add (_rewind_button, 0, wxALL, 2);
	h_sizer->Add (_back_button, 0, wxALL, 2);
	h_sizer->Add (time_sizer, 0, wxEXPAND);
	h_sizer->Add (_forward_button, 0, wxALL, 2);

	_button_sizer = new wxBoxSizer (wxHORIZONTAL);
	h_sizer->Add (_button_sizer, 0, wxEXPAND);

	h_sizer->Add (_slider, 1, wxEXPAND);

	_v_sizer->Add (h_sizer, 0, wxEXPAND | wxALL, 6);

	_frame_number->SetMinSize (wxSize (84, -1));
	_rewind_button->SetMinSize (wxSize (32, -1));
	_back_button->SetMinSize (wxSize (32, -1));
	_forward_button->SetMinSize (wxSize (32, -1));

	if (_eye) {
		_eye->Bind (wxEVT_CHOICE, boost::bind (&Controls::eye_changed, this));
	}
	if (_outline_content) {
		_outline_content->Bind (wxEVT_CHECKBOX, boost::bind (&Controls::outline_content_changed, this));
	}

	_slider->Bind           (wxEVT_SCROLL_THUMBTRACK,    boost::bind(&Controls::slider_moved,    this, false));
	_slider->Bind           (wxEVT_SCROLL_PAGEUP,        boost::bind(&Controls::slider_moved,    this, true));
	_slider->Bind           (wxEVT_SCROLL_PAGEDOWN,      boost::bind(&Controls::slider_moved,    this, true));
	_slider->Bind           (wxEVT_SCROLL_CHANGED,       boost::bind(&Controls::slider_released, this));
#ifdef DCPOMATIC_OSX
	/* _CHANGED is not received on OS X (at least, not when the
	   slider is dragged), so use this instead.  Perhaps all
	   platforms could just use _THUMBRELEASE.
	*/
	_slider->Bind           (wxEVT_SCROLL_THUMBRELEASE,  boost::bind(&Controls::slider_released, this));
#endif
	_rewind_button->Bind    (wxEVT_LEFT_DOWN,            boost::bind(&Controls::rewind_clicked,  this, _1));
	_back_button->Bind      (wxEVT_LEFT_DOWN,            boost::bind(&Controls::back_clicked,    this, _1));
	_forward_button->Bind   (wxEVT_LEFT_DOWN,            boost::bind(&Controls::forward_clicked, this, _1));
	_frame_number->Bind     (wxEVT_LEFT_DOWN,            boost::bind(&Controls::frame_number_clicked, this));
	_timecode->Bind         (wxEVT_LEFT_DOWN,            boost::bind(&Controls::timecode_clicked, this));
	if (_jump_to_selected) {
		_jump_to_selected->Bind (wxEVT_CHECKBOX, boost::bind (&Controls::jump_to_selected_clicked, this));
		_jump_to_selected->SetValue (Config::instance()->jump_to_selected ());
	}

	_viewer->PositionChanged.connect (boost::bind(&Controls::position_changed, this));
	_viewer->Started.connect (boost::bind(&Controls::started, this));
	_viewer->Stopped.connect (boost::bind(&Controls::stopped, this));

	set_film (_viewer->film());

	setup_sensitivity ();

	JobManager::instance()->ActiveJobsChanged.connect (
		bind (&Controls::active_jobs_changed, this, _2)
		);

	_config_changed_connection = Config::instance()->Changed.connect (bind(&Controls::config_changed, this, _1));
	config_changed (Config::OTHER);
}

void
Controls::config_changed (int)
{
	setup_sensitivity ();
}

void
Controls::started ()
{
	setup_sensitivity ();
}

void
Controls::stopped ()
{
	setup_sensitivity ();
}

void
Controls::position_changed ()
{
	if (!_slider_being_moved) {
		update_position_label ();
		update_position_slider ();
	}
}

void
Controls::eye_changed ()
{
	_viewer->set_eyes (_eye->GetSelection() == 0 ? EYES_LEFT : EYES_RIGHT);
}

void
Controls::outline_content_changed ()
{
	_viewer->set_outline_content (_outline_content->GetValue());
}

/** @param page true if this was a PAGEUP/PAGEDOWN event for which we won't receive a THUMBRELEASE */
void
Controls::slider_moved (bool page)
{
	if (!_film) {
		return;
	}

	if (!page && !_slider_being_moved) {
		/* This is the first event of a drag; stop playback for the duration of the drag */
		_viewer->suspend ();
		_slider_being_moved = true;
	}

	DCPTime t (_slider->GetValue() * _film->length().get() / 4096);
	t = t.round (_film->video_frame_rate());
	/* Ensure that we hit the end of the film at the end of the slider.  In particular, we
	   need to do an accurate seek in case there isn't a keyframe near the end.
	*/
	bool accurate = false;
	if (t >= _film->length ()) {
		t = _film->length() - _viewer->one_video_frame();
		accurate = true;
	}
	_viewer->seek (t, accurate);
	update_position_label ();

	log (
		wxString::Format(
			"playback-seeked %s", t.timecode(_film->video_frame_rate()).c_str()
			)
		);
}

void
Controls::slider_released ()
{
	/* Restart after a drag */
	_viewer->resume ();
	_slider_being_moved = false;
}

void
Controls::update_position_slider ()
{
	if (!_film) {
		_slider->SetValue (0);
		return;
	}

	DCPTime const len = _film->length ();

	if (len.get ()) {
		int const new_slider_position = 4096 * _viewer->position().get() / len.get();
		if (new_slider_position != _slider->GetValue()) {
			_slider->SetValue (new_slider_position);
		}
	}
}

void
Controls::update_position_label ()
{
	if (!_film) {
		_frame_number->SetLabel ("0");
		_timecode->SetLabel ("0:0:0.0");
		return;
	}

	double const fps = _film->video_frame_rate ();
	/* Count frame number from 1 ... not sure if this is the best idea */
	_frame_number->SetLabel (wxString::Format (wxT("%ld"), lrint (_viewer->position().seconds() * fps) + 1));
	_timecode->SetLabel (time_to_timecode (_viewer->position(), fps));
}

void
Controls::active_jobs_changed (optional<string> j)
{
	_active_job = j;
	setup_sensitivity ();
}

DCPTime
Controls::nudge_amount (wxKeyboardState& ev)
{
	DCPTime amount = _viewer->one_video_frame ();

	if (ev.ShiftDown() && !ev.ControlDown()) {
		amount = DCPTime::from_seconds (1);
	} else if (!ev.ShiftDown() && ev.ControlDown()) {
		amount = DCPTime::from_seconds (10);
	} else if (ev.ShiftDown() && ev.ControlDown()) {
		amount = DCPTime::from_seconds (60);
	}

	return amount;
}

void
Controls::rewind_clicked (wxMouseEvent& ev)
{
	_viewer->seek (DCPTime(), true);
	ev.Skip();
}

void
Controls::back_frame ()
{
	_viewer->seek_by (-_viewer->one_video_frame(), true);
}

void
Controls::forward_frame ()
{
	_viewer->seek_by (_viewer->one_video_frame(), true);
}

void
Controls::back_clicked (wxKeyboardState& ev)
{
	_viewer->seek_by (-nudge_amount(ev), true);
}

void
Controls::forward_clicked (wxKeyboardState& ev)
{
	_viewer->seek_by (nudge_amount(ev), true);
}

void
Controls::setup_sensitivity ()
{
	/* examine content is the only job which stops the viewer working */
	bool const active_job = _active_job && *_active_job != "examine_content";
	bool const c = _film && !_film->content().empty() && !active_job;

	_slider->Enable (c);
	_rewind_button->Enable (c);
	_back_button->Enable (c);
	_forward_button->Enable (c);
	if (_outline_content) {
		_outline_content->Enable (c);
	}
	_frame_number->Enable (c);
	_timecode->Enable (c);
	if (_jump_to_selected) {
		_jump_to_selected->Enable (c);
	}

	if (_eye) {
		_eye->Enable (c && _film->three_d ());
	}
}

void
Controls::timecode_clicked ()
{
	PlayheadToTimecodeDialog* dialog = new PlayheadToTimecodeDialog (this, _film->video_frame_rate ());
	if (dialog->ShowModal() == wxID_OK) {
		_viewer->seek (dialog->get(), true);
	}
	dialog->Destroy ();
}

void
Controls::frame_number_clicked ()
{
	PlayheadToFrameDialog* dialog = new PlayheadToFrameDialog (this, _film->video_frame_rate ());
	if (dialog->ShowModal() == wxID_OK) {
		_viewer->seek (dialog->get(), true);
	}
	dialog->Destroy ();
}

void
Controls::jump_to_selected_clicked ()
{
	Config::instance()->set_jump_to_selected (_jump_to_selected->GetValue ());
}

void
Controls::set_film (shared_ptr<Film> film)
{
	if (_film == film) {
		return;
	}

	_film = film;

	if (_film) {
		_film_change_connection = _film->Change.connect (boost::bind(&Controls::film_change, this, _1, _2));
	}

	setup_sensitivity ();

	update_position_slider ();
	update_position_label ();
}

shared_ptr<Film>
Controls::film () const
{
	return _film;
}

void
Controls::film_change (ChangeType type, Film::Property p)
{
	if (type == CHANGE_TYPE_DONE) {
		if (p == Film::CONTENT) {
			setup_sensitivity ();
			update_position_label ();
			update_position_slider ();
		} else if (p == Film::THREE_D) {
			setup_sensitivity ();
		}
	}
}
