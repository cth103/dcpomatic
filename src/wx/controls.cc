#include "controls.h"
#include "film_viewer.h"
#include "wx_util.h"
#include "playhead_to_timecode_dialog.h"
#include "playhead_to_frame_dialog.h"
#include "lib/job_manager.h"
#include <wx/wx.h>
#include <wx/tglbtn.h>

using std::string;
using boost::optional;
using boost::shared_ptr;
using boost::weak_ptr;

/** @param outline_content true if viewer should present an "outline content" checkbox.
 *  @param jump_to_selected true if viewer should present a "jump to selected" checkbox.
 */
Controls::Controls (wxWindow* parent, shared_ptr<FilmViewer> viewer, bool outline_content, bool jump_to_selected)
	: wxPanel (parent)
	, _viewer (viewer)
	, _slider_being_moved (false)
	, _was_running_before_slider (false)
	, _outline_content (0)
	, _eye (0)
	, _jump_to_selected (0)
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

	_eye->Bind (wxEVT_CHOICE, boost::bind (&Controls::eye_changed, this));
	if (_outline_content) {
		_outline_content->Bind (wxEVT_CHECKBOX, boost::bind (&Controls::outline_content_changed, this));
	}

	_slider->Bind           (wxEVT_SCROLL_THUMBTRACK,   boost::bind (&Controls::slider_moved,    this, false));
	_slider->Bind           (wxEVT_SCROLL_PAGEUP,       boost::bind (&Controls::slider_moved,    this, true));
	_slider->Bind           (wxEVT_SCROLL_PAGEDOWN,     boost::bind (&Controls::slider_moved,    this, true));
	_slider->Bind           (wxEVT_SCROLL_THUMBRELEASE, boost::bind (&Controls::slider_released, this));
	_play_button->Bind      (wxEVT_TOGGLEBUTTON,        boost::bind (&Controls::play_clicked,    this));
	_rewind_button->Bind    (wxEVT_LEFT_DOWN,           boost::bind (&Controls::rewind_clicked,  this, _1));
	_back_button->Bind      (wxEVT_LEFT_DOWN,           boost::bind (&Controls::back_clicked,    this, _1));
	_forward_button->Bind   (wxEVT_LEFT_DOWN,           boost::bind (&Controls::forward_clicked, this, _1));
	_frame_number->Bind     (wxEVT_LEFT_DOWN,           boost::bind (&Controls::frame_number_clicked, this));
	_timecode->Bind         (wxEVT_LEFT_DOWN,           boost::bind (&Controls::timecode_clicked, this));
	if (_jump_to_selected) {
		_jump_to_selected->Bind (wxEVT_CHECKBOX, boost::bind (&Controls::jump_to_selected_clicked, this));
		_jump_to_selected->SetValue (Config::instance()->jump_to_selected ());
	}

	_viewer->PositionChanged.connect (boost::bind(&Controls::position_changed, this));
	_viewer->Started.connect (boost::bind(&Controls::started, this));
	_viewer->Stopped.connect (boost::bind(&Controls::stopped, this));
	_viewer->FilmChanged.connect (boost::bind(&Controls::film_changed, this));

	film_changed ();

	setup_sensitivity ();

	JobManager::instance()->ActiveJobsChanged.connect (
		bind (&Controls::active_jobs_changed, this, _2)
		);
}

void
Controls::started ()
{
	_play_button->SetValue (true);
}

void
Controls::stopped ()
{
	_play_button->SetValue (false);
}

void
Controls::position_changed ()
{
	update_position_label ();
	update_position_slider ();
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

void
Controls::film_change (ChangeType type, Film::Property p)
{
	if (type != CHANGE_TYPE_DONE) {
		return;
	}

	if (p == Film::CONTENT || p == Film::THREE_D) {
		setup_sensitivity ();
	}
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
		_was_running_before_slider = _viewer->stop ();
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
Controls::slider_released ()
{
	if (_was_running_before_slider) {
		/* Restart after a drag */
		_viewer->start ();
	}
	_slider_being_moved = false;
}

void
Controls::play_clicked ()
{
	check_play_state ();
}

void
Controls::check_play_state ()
{
	if (!_film || _film->video_frame_rate() == 0) {
		return;
	}

	if (_play_button->GetValue()) {
		_viewer->start ();
	} else {
		_viewer->stop ();
	}
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
	/* examine content is the only job which stops the viewer working */
	bool const a = !j || *j != "examine_content";
	_slider->Enable (a);
	_play_button->Enable (a);
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
Controls::film_changed ()
{
	shared_ptr<Film> film = _viewer->film ();

	if (_film == film) {
		return;
	}

	_film = film;

	setup_sensitivity ();

	update_position_slider ();
	update_position_label ();

	_film->Change.connect (boost::bind (&Controls::film_change, this, _1, _2));
}

shared_ptr<Film>
Controls::film () const
{
	return _film;
}
