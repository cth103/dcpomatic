#include "control_film_viewer.h"
#include "film_viewer.h"
#include "wx_util.h"
#include <wx/wx.h>
#include <wx/tglbtn.h>

using std::string;
using boost::optional;

ControlFilmViewer::ControlFilmViewer (wxWindow* parent, bool outline_content, bool jump_to_selected)
	: wxPanel (parent)
	, _viewer (new FilmViewer(this, outline_content, jump_to_selected))
	, _slider (new wxSlider (this, wxID_ANY, 0, 0, 4096))
	, _rewind_button (new wxButton (this, wxID_ANY, wxT("|<")))
	, _back_button (new wxButton (this, wxID_ANY, wxT("<")))
	, _forward_button (new wxButton (this, wxID_ANY, wxT(">")))
	, _frame_number (new wxStaticText (this, wxID_ANY, wxT("")))
	, _timecode (new wxStaticText (this, wxID_ANY, wxT("")))
	, _play_button (new wxToggleButton (this, wxID_ANY, _("Play")))
{
	_v_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (_v_sizer);
	_v_sizer->Add (_viewer->panel(), 1, wxEXPAND);

	wxBoxSizer* view_options = new wxBoxSizer (wxHORIZONTAL);
	if (outline_content) {
		_outline_content = new wxCheckBox (this, wxID_ANY, _("Outline content"));
		view_options->Add (_outline_content, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_GAP);
	}

	_eye = new wxChoice (this, wxID_ANY);
	_eye->Append (_("Left"));
	_eye->Append (_("Right"));
	_eye->SetSelection (0);
	view_options->Add (_eye, 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_GAP);

	if (jump_to_selected) {
		_jump_to_selected = new wxCheckBox (this, wxID_ANY, _("Jump to selected content"));
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
	h_sizer->Add (_play_button, 0, wxEXPAND);
	h_sizer->Add (_slider, 1, wxEXPAND);

	_v_sizer->Add (h_sizer, 0, wxEXPAND | wxALL, 6);

	_frame_number->SetMinSize (wxSize (84, -1));
	_rewind_button->SetMinSize (wxSize (32, -1));
	_back_button->SetMinSize (wxSize (32, -1));
	_forward_button->SetMinSize (wxSize (32, -1));

	_eye->Bind              (wxEVT_CHOICE,              boost::bind (&FilmViewer::slow_refresh, _viewer.get()));
	_slider->Bind           (wxEVT_SCROLL_THUMBTRACK,   boost::bind (&ControlFilmViewer::slider_moved,    this, false));
	_slider->Bind           (wxEVT_SCROLL_PAGEUP,       boost::bind (&ControlFilmViewer::slider_moved,    this, true));
	_slider->Bind           (wxEVT_SCROLL_PAGEDOWN,     boost::bind (&ControlFilmViewer::slider_moved,    this, true));
	_slider->Bind           (wxEVT_SCROLL_THUMBRELEASE, boost::bind (&ControlFilmViewer::slider_released, this));
	_play_button->Bind      (wxEVT_TOGGLEBUTTON,        boost::bind (&ControlFilmViewer::play_clicked,    this));
	_rewind_button->Bind    (wxEVT_LEFT_DOWN,           boost::bind (&ControlFilmViewer::rewind_clicked,  this, _1));
	_back_button->Bind      (wxEVT_LEFT_DOWN,           boost::bind (&ControlFilmViewer::back_clicked,    this, _1));
	_forward_button->Bind   (wxEVT_LEFT_DOWN,           boost::bind (&ControlFilmViewer::forward_clicked, this, _1));
	_frame_number->Bind     (wxEVT_LEFT_DOWN,           boost::bind (&ControlFilmViewer::frame_number_clicked, this));
	_timecode->Bind         (wxEVT_LEFT_DOWN,           boost::bind (&ControlFilmViewer::timecode_clicked, this));
	if (_jump_to_selected) {
		_jump_to_selected->Bind (wxEVT_CHECKBOX, boost::bind (&ControlFilmViewer::jump_to_selected_clicked, this));
		_jump_to_selected->SetValue (Config::instance()->jump_to_selected ());
	}
}

/** @param page true if this was a PAGEUP/PAGEDOWN event for which we won't receive a THUMBRELEASE */
void
ControlFilmViewer::slider_moved (bool page)
{
	if (!_film) {
		return;
	}

	if (!page && !_slider_being_moved) {
		/* This is the first event of a drag; stop playback for the duration of the drag */
		_was_running_before_slider = stop ();
		_slider_being_moved = true;
	}

	DCPTime t (_slider->GetValue() * _film->length().get() / 4096);
	t = t.round (_film->video_frame_rate());
	/* Ensure that we hit the end of the film at the end of the slider */
	if (t >= _film->length ()) {
		t = _film->length() - _viewer->one_video_frame();
	}
	_viewer->seek (t, false);
	update_position_label ();
}

void
ControlFilmViewer::slider_released ()
{
	if (_was_running_before_slider) {
		/* Restart after a drag */
		start ();
	}
	_slider_being_moved = false;
}

void
ControlFilmViewer::play_clicked ()
{
	check_play_state ();
}

void
ControlFilmViewer::check_play_state ()
{
	if (!_film || _film->video_frame_rate() == 0) {
		return;
	}

	if (_play_button->GetValue()) {
		start ();
	} else {
		stop ();
	}
}

void
ControlFilmViewer::update_position_slider ()
{
	if (!_film) {
		_slider->SetValue (0);
		return;
	}

	DCPTime const len = _film->length ();

	if (len.get ()) {
		int const new_slider_position = 4096 * _viewer->video_position().get() / len.get();
		if (new_slider_position != _slider->GetValue()) {
			_slider->SetValue (new_slider_position);
		}
	}
}

void
ControlFilmViewer::update_position_label ()
{
	if (!_film) {
		_frame_number->SetLabel ("0");
		_timecode->SetLabel ("0:0:0.0");
		return;
	}

	double const fps = _film->video_frame_rate ();
	/* Count frame number from 1 ... not sure if this is the best idea */
	_frame_number->SetLabel (wxString::Format (wxT("%ld"), lrint (_viewer->video_position().seconds() * fps) + 1));
	_timecode->SetLabel (time_to_timecode (_viewer->video_position(), fps));
}

void
ControlFilmViewer::active_jobs_changed (optional<string> j)
{
	/* examine content is the only job which stops the viewer working */
	bool const a = !j || *j != "examine_content";
	_slider->Enable (a);
	_play_button->Enable (a);
}

DCPTime
ControlFilmViewer::nudge_amount (wxKeyboardState& ev)
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
ControlFilmViewer::rewind_clicked (wxMouseEvent& ev)
{
	_viewer->go_to (DCPTime());
	ev.Skip();
}

void
ControlFilmViewer::back_frame ()
{
	_viewer->move (-_viewer->one_video_frame());
}

void
ControlFilmViewer::forward_frame ()
{
	_viewer->move (_viewer->one_video_frame());
}

void
ControlFilmViewer::back_clicked (wxKeyboardState& ev)
{
	_viewer->move (-nudge_amount(ev));
}

void
ControlFilmViewer::forward_clicked (wxKeyboardState& ev)
{
	_viewer->move (nudge_amount(ev));
}

void
ControlFilmViewer::setup_sensitivity ()
{
	bool const c = _film && !_film->content().empty ();

	_slider->Enable (c);
	_rewind_button->Enable (c);
	_back_button->Enable (c);
	_forward_button->Enable (c);
	_play_button->Enable (c);
	if (_outline_content) {
		_outline_content->Enable (c);
	}
	_frame_number->Enable (c);
	_timecode->Enable (c);
	if (_jump_to_selected) {
		_jump_to_selected->Enable (c);
	}

	_eye->Enable (c && _film->three_d ());
}

void
ControlFilmViewer::timecode_clicked ()
{
	PlayheadToTimecodeDialog* dialog = new PlayheadToTimecodeDialog (this, _film->video_frame_rate ());
	if (dialog->ShowModal() == wxID_OK) {
		_viewer->go_to (dialog->get ());
	}
	dialog->Destroy ();
}

void
ControlFilmViewer::frame_number_clicked ()
{
	PlayheadToFrameDialog* dialog = new PlayheadToFrameDialog (this, _film->video_frame_rate ());
	if (dialog->ShowModal() == wxID_OK) {
		_viewer->go_to (dialog->get ());
	}
	dialog->Destroy ();
}

void
ControlFilmViewer::jump_to_selected_clicked ()
{
	Config::instance()->set_jump_to_selected (_jump_to_selected->GetValue ());
}

void
ControlFilmViewer::set_film (shared_ptr<Film> film)
{
	_viewer->set_film (film);

	if (_film == film) {
		return;
	}

	_film = film;

	update_position_slider ();
	update_position_label ();
}

void
ControlFilmViewer::set_position (DCPTime p)
{
	_viewer->set_position (p);
}

void
ControlFilmViewer::set_position (shared_ptr<Content> content, ContentTime t)
{
	_viewer->set_position (content, t);
}

void
ControlFilmViewer::set_dcp_decode_reduction (boost::optional<int> reduction)
{
	_viewer->set_dcp_decode_reduction (reduction);
}

void
ControlFilmViewer::show_closed_captions ()
{
	_viewer->show_closed_captions ();
}

void
ControlFilmViewer::start ()
{
	_viewer->start ();
}

bool
ControlFilmViewer::stop ()
{
	return _viewer->stop ();
}

bool
ControlFilmViewer::playing () const
{
	return _viewer->playing ();
}
