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

/** @file src/film_editor.cc
 *  @brief A wx widget to edit a film's metadata, and perform various functions.
 */

#include <iostream>
#include <iomanip>
#include <wx/wx.h>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include "lib/format.h"
#include "lib/film.h"
#include "lib/transcode_job.h"
#include "lib/exceptions.h"
#include "lib/ab_transcode_job.h"
#include "lib/thumbs_job.h"
#include "lib/job_manager.h"
#include "lib/filter.h"
#include "lib/screen.h"
#include "lib/config.h"
#include "filter_dialog.h"
#include "wx_util.h"
#include "film_editor.h"
#include "dcp_range_dialog.h"
#include "gain_calculator_dialog.h"
#include "sound_processor.h"
#include "dci_name_dialog.h"

using namespace std;
using namespace boost;

/** @param f Film to edit */
FilmEditor::FilmEditor (Film* f, wxWindow* parent)
	: wxPanel (parent)
	, _ignore_changes (Film::NONE)
	, _film (f)
{
	_sizer = new wxFlexGridSizer (2, 4, 4);
	SetSizer (_sizer);

	add_label_to_sizer (_sizer, this, "Name");
	_name = new wxTextCtrl (this, wxID_ANY);
	_sizer->Add (_name, 1, wxEXPAND);

	_use_dci_name = new wxCheckBox (this, wxID_ANY, wxT ("Use DCI name"));
	_sizer->Add (_use_dci_name, 1, wxEXPAND);
	_edit_dci_button = new wxButton (this, wxID_ANY, wxT ("Edit..."));
	_sizer->Add (_edit_dci_button, 0);

	add_label_to_sizer (_sizer, this, "Content");
	_content = new wxFilePickerCtrl (this, wxID_ANY, wxT (""), wxT ("Select Content File"), wxT("*.*"));
	_sizer->Add (_content, 1, wxEXPAND);

	add_label_to_sizer (_sizer, this, "Content Type");
	_dcp_content_type = new wxComboBox (this, wxID_ANY);
	_sizer->Add (_dcp_content_type);

	add_label_to_sizer (_sizer, this, "Format");
	_format = new wxComboBox (this, wxID_ANY);
	_sizer->Add (_format);

	{
		add_label_to_sizer (_sizer, this, "Crop");
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);

		add_label_to_sizer (s, this, "L");
		_left_crop = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
		s->Add (_left_crop, 0);
		add_label_to_sizer (s, this, "R");
		_right_crop = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
		s->Add (_right_crop, 0);
		add_label_to_sizer (s, this, "T");
		_top_crop = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
		s->Add (_top_crop, 0);
		add_label_to_sizer (s, this, "B");
		_bottom_crop = new wxSpinCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
		s->Add (_bottom_crop, 0);

		_sizer->Add (s);
	}

	/* VIDEO-only stuff */
	{
		video_control (add_label_to_sizer (_sizer, this, "Filters"));
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_filters = new wxStaticText (this, wxID_ANY, wxT ("None"));
		video_control (_filters);
		s->Add (_filters, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);
		_filters_button = new wxButton (this, wxID_ANY, wxT ("Edit..."));
		video_control (_filters_button);
		s->Add (_filters_button, 0);
		_sizer->Add (s, 1);
	}

	video_control (add_label_to_sizer (_sizer, this, "Scaler"));
	_scaler = new wxComboBox (this, wxID_ANY);
	_sizer->Add (video_control (_scaler), 1);

	{
		video_control (add_label_to_sizer (_sizer, this, "Audio Gain"));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_audio_gain = new wxSpinCtrl (this);
		s->Add (video_control (_audio_gain), 1);
		video_control (add_label_to_sizer (s, this, "dB"));
		_audio_gain_calculate_button = new wxButton (this, wxID_ANY, _("Calculate..."));
		video_control (_audio_gain_calculate_button);
		s->Add (_audio_gain_calculate_button, 1, wxEXPAND);
		_sizer->Add (s);
	}

	{
		video_control (add_label_to_sizer (_sizer, this, "Audio Delay"));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_audio_delay = new wxSpinCtrl (this);
		s->Add (video_control (_audio_delay), 1);
		video_control (add_label_to_sizer (s, this, "ms"));
		_sizer->Add (s);
	}

	_with_subtitles = new wxCheckBox (this, wxID_ANY, wxT("With Subtitles"));
	video_control (_with_subtitles);
	_sizer->Add (_with_subtitles, 1);
	_sizer->AddSpacer (0);

	video_control (add_label_to_sizer (_sizer, this, "Subtitle Offset"));
	_subtitle_offset = new wxSpinCtrl (this);
	_sizer->Add (video_control (_subtitle_offset), 1);

	{
		video_control (add_label_to_sizer (_sizer, this, "Subtitle Scale"));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_subtitle_scale = new wxSpinCtrl (this);
		s->Add (video_control (_subtitle_scale));
		video_control (add_label_to_sizer (s, this, "%"));
		_sizer->Add (s);
	}
	
	video_control (add_label_to_sizer (_sizer, this, "Frames Per Second"));
	_frames_per_second = new wxStaticText (this, wxID_ANY, wxT (""));
	_sizer->Add (video_control (_frames_per_second), 1, wxALIGN_CENTER_VERTICAL);
	
	video_control (add_label_to_sizer (_sizer, this, "Original Size"));
	_original_size = new wxStaticText (this, wxID_ANY, wxT (""));
	_sizer->Add (video_control (_original_size), 1, wxALIGN_CENTER_VERTICAL);
	
	video_control (add_label_to_sizer (_sizer, this, "Length"));
	_length = new wxStaticText (this, wxID_ANY, wxT (""));
	_sizer->Add (video_control (_length), 1, wxALIGN_CENTER_VERTICAL);

	video_control (add_label_to_sizer (_sizer, this, "Audio"));
	_audio = new wxStaticText (this, wxID_ANY, wxT (""));
	_sizer->Add (video_control (_audio), 1, wxALIGN_CENTER_VERTICAL);

	{
		video_control (add_label_to_sizer (_sizer, this, "Range"));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_dcp_range = new wxStaticText (this, wxID_ANY, wxT (""));
		s->Add (video_control (_dcp_range), 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);
		_change_dcp_range_button = new wxButton (this, wxID_ANY, wxT ("Edit..."));
		s->Add (video_control (_change_dcp_range_button), 0, 0, 6);
		_sizer->Add (s);
	}

	_dcp_ab = new wxCheckBox (this, wxID_ANY, wxT ("A/B"));
	video_control (_dcp_ab);
	_sizer->Add (_dcp_ab, 1);
	_sizer->AddSpacer (0);

	/* STILL-only stuff */
	{
		still_control (add_label_to_sizer (_sizer, this, "Duration"));
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_still_duration = new wxSpinCtrl (this);
		still_control (_still_duration);
		s->Add (_still_duration, 1, wxEXPAND);
		still_control (add_label_to_sizer (s, this, "s"));
		_sizer->Add (s);
	}

	/* Set up our editing widgets */
	
	_left_crop->SetRange (0, 1024);
	_top_crop->SetRange (0, 1024);
	_right_crop->SetRange (0, 1024);
	_bottom_crop->SetRange (0, 1024);
	_audio_gain->SetRange (-60, 60);
	_audio_delay->SetRange (-1000, 1000);
	_still_duration->SetRange (0, 60 * 60);
	_subtitle_offset->SetRange (-1024, 1024);
	_subtitle_scale->SetRange (1, 1000);

	vector<DCPContentType const *> const ct = DCPContentType::all ();
	for (vector<DCPContentType const *>::const_iterator i = ct.begin(); i != ct.end(); ++i) {
		_dcp_content_type->Append (std_to_wx ((*i)->pretty_name ()));
	}

	vector<Scaler const *> const sc = Scaler::all ();
	for (vector<Scaler const *>::const_iterator i = sc.begin(); i != sc.end(); ++i) {
		_scaler->Append (std_to_wx ((*i)->name()));
	}

	/* And set their values from the Film */
	set_film (f);
	
	/* Now connect to them, since initial values are safely set */
	_name->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (FilmEditor::name_changed), 0, this);
	_use_dci_name->Connect (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler (FilmEditor::use_dci_name_toggled), 0, this);
	_edit_dci_button->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (FilmEditor::edit_dci_button_clicked), 0, this);
	_format->Connect (wxID_ANY, wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler (FilmEditor::format_changed), 0, this);
	_content->Connect (wxID_ANY, wxEVT_COMMAND_FILEPICKER_CHANGED, wxCommandEventHandler (FilmEditor::content_changed), 0, this);
	_left_crop->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::left_crop_changed), 0, this);
	_right_crop->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::right_crop_changed), 0, this);
	_top_crop->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::top_crop_changed), 0, this);
	_bottom_crop->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::bottom_crop_changed), 0, this);
	_filters_button->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (FilmEditor::edit_filters_clicked), 0, this);
	_scaler->Connect (wxID_ANY, wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler (FilmEditor::scaler_changed), 0, this);
	_dcp_content_type->Connect (wxID_ANY, wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler (FilmEditor::dcp_content_type_changed), 0, this);
	_dcp_ab->Connect (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler (FilmEditor::dcp_ab_toggled), 0, this);
	_audio_gain->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::audio_gain_changed), 0, this);
	_audio_gain_calculate_button->Connect (
		wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (FilmEditor::audio_gain_calculate_button_clicked), 0, this
		);
	_audio_delay->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::audio_delay_changed), 0, this);
	_still_duration->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::still_duration_changed), 0, this);
	_change_dcp_range_button->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (FilmEditor::change_dcp_range_clicked), 0, this);
	_with_subtitles->Connect (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler (FilmEditor::with_subtitles_toggled), 0, this);
	_subtitle_offset->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::subtitle_offset_changed), 0, this);
	_subtitle_scale->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::subtitle_scale_changed), 0, this);

	setup_visibility ();
	setup_formats ();
}

/** Called when the left crop widget has been changed */
void
FilmEditor::left_crop_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::CROP;
	_film->set_left_crop (_left_crop->GetValue ());
	_ignore_changes = Film::NONE;
}

/** Called when the right crop widget has been changed */
void
FilmEditor::right_crop_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::CROP;
	_film->set_right_crop (_right_crop->GetValue ());
	_ignore_changes = Film::NONE;
}

/** Called when the top crop widget has been changed */
void
FilmEditor::top_crop_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::CROP;
	_film->set_top_crop (_top_crop->GetValue ());
	_ignore_changes = Film::NONE;
}

/** Called when the bottom crop value has been changed */
void
FilmEditor::bottom_crop_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::CROP;
	_film->set_bottom_crop (_bottom_crop->GetValue ());
	_ignore_changes = Film::NONE;
}

/** Called when the content filename has been changed */
void
FilmEditor::content_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::CONTENT;
	
	try {
		_film->set_content (wx_to_std (_content->GetPath ()));
	} catch (std::exception& e) {
		_content->SetPath (std_to_wx (_film->directory ()));
		error_dialog (this, String::compose ("Could not set content: %1", e.what ()));
	}

	_ignore_changes = Film::NONE;

	setup_visibility ();
	setup_formats ();
	setup_subtitle_button ();
}

/** Called when the DCP A/B switch has been toggled */
void
FilmEditor::dcp_ab_toggled (wxCommandEvent &)
{
	if (!_film) {
		return;
	}
	
	_ignore_changes = Film::DCP_AB;
	_film->set_dcp_ab (_dcp_ab->GetValue ());
	_ignore_changes = Film::NONE;
}

/** Called when the name widget has been changed */
void
FilmEditor::name_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::NAME;
	_film->set_name (string (_name->GetValue().mb_str()));
	_ignore_changes = Film::NONE;
}

void
FilmEditor::subtitle_offset_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::SUBTITLE_OFFSET;
	_film->set_subtitle_offset (_subtitle_offset->GetValue ());
	_ignore_changes = Film::NONE;
}

void
FilmEditor::subtitle_scale_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::SUBTITLE_OFFSET;
	_film->set_subtitle_scale (_subtitle_scale->GetValue() / 100.0);
	_ignore_changes = Film::NONE;
}


/** Called when the metadata stored in the Film object has changed;
 *  so that we can update the GUI.
 *  @param p Property of the Film that has changed.
 */
void
FilmEditor::film_changed (Film::Property p)
{
	if (!_film || _ignore_changes == p) {
		return;
	}

	stringstream s;
		
	switch (p) {
	case Film::NONE:
		break;
	case Film::CONTENT:
		_content->SetPath (std_to_wx (_film->content ()));
		setup_visibility ();
		setup_formats ();
		setup_subtitle_button ();
		break;
	case Film::FORMAT:
	{
		int n = 0;
		vector<Format const *>::iterator i = _formats.begin ();
		while (i != _formats.end() && *i != _film->format ()) {
			++i;
			++n;
		}
		_format->SetSelection (n);
		break;
	}
	case Film::CROP:
		_left_crop->SetValue (_film->crop().left);
		_right_crop->SetValue (_film->crop().right);
		_top_crop->SetValue (_film->crop().top);
		_bottom_crop->SetValue (_film->crop().bottom);
		break;
	case Film::FILTERS:
	{
		pair<string, string> p = Filter::ffmpeg_strings (_film->filters ());
		if (p.first.empty () && p.second.empty ()) {
			_filters->SetLabel (_("None"));
		} else {
			string const b = p.first + " " + p.second;
			_filters->SetLabel (std_to_wx (b));
		}
		_sizer->Layout ();
		break;
	}
	case Film::NAME:
		_name->ChangeValue (std_to_wx (_film->name ()));
		break;
	case Film::FRAMES_PER_SECOND:
	{
		stringstream s;
		s << fixed << setprecision(2) << _film->frames_per_second();
		_frames_per_second->SetLabel (std_to_wx (s.str ()));
		break;
	}
	case Film::AUDIO_CHANNELS:
	case Film::AUDIO_SAMPLE_RATE:
		if (_film->audio_channels() == 0 && _film->audio_sample_rate() == 0) {
			_audio->SetLabel (wxT (""));
		} else {
			s << _film->audio_channels () << " channels, " << _film->audio_sample_rate() << "Hz";
			_audio->SetLabel (std_to_wx (s.str ()));
		}
		break;
	case Film::SIZE:
		if (_film->size().width == 0 && _film->size().height == 0) {
			_original_size->SetLabel (wxT (""));
		} else {
			s << _film->size().width << " x " << _film->size().height;
			_original_size->SetLabel (std_to_wx (s.str ()));
		}
		break;
	case Film::LENGTH:
		if (_film->frames_per_second() > 0 && _film->length() > 0) {
			s << _film->length() << " frames; " << seconds_to_hms (_film->length() / _film->frames_per_second());
		} else if (_film->length() > 0) {
			s << _film->length() << " frames";
		} 
		_length->SetLabel (std_to_wx (s.str ()));
		break;
	case Film::DCP_CONTENT_TYPE:
		_dcp_content_type->SetSelection (DCPContentType::as_index (_film->dcp_content_type ()));
		break;
	case Film::THUMBS:
		break;
	case Film::DCP_FRAMES:
		if (_film->dcp_frames() == 0) {
			_dcp_range->SetLabel (wxT ("Whole film"));
		} else {
			stringstream s;
			s << "First " << _film->dcp_frames() << " frames";
			_dcp_range->SetLabel (std_to_wx (s.str ()));
		}
		_sizer->Layout ();
		break;
	case Film::DCP_TRIM_ACTION:
		break;
	case Film::DCP_AB:
		_dcp_ab->SetValue (_film->dcp_ab ());
		break;
	case Film::SCALER:
		_scaler->SetSelection (Scaler::as_index (_film->scaler ()));
		break;
	case Film::AUDIO_GAIN:
		_audio_gain->SetValue (_film->audio_gain ());
		break;
	case Film::AUDIO_DELAY:
		_audio_delay->SetValue (_film->audio_delay ());
		break;
	case Film::STILL_DURATION:
		_still_duration->SetValue (_film->still_duration ());
		break;
	case Film::WITH_SUBTITLES:
		_with_subtitles->SetValue (_film->with_subtitles ());
		_subtitle_scale->Enable (_film->with_subtitles ());
		_subtitle_offset->Enable (_film->with_subtitles ());
		break;
	case Film::SUBTITLE_OFFSET:
		_subtitle_offset->SetValue (_film->subtitle_offset ());
		break;
	case Film::SUBTITLE_SCALE:
		_subtitle_scale->SetValue (_film->subtitle_scale() * 100);
		break;
	case Film::USE_DCI_NAME:
		_use_dci_name->SetValue (_film->use_dci_name ());
		break;
	case Film::DCI_METADATA:
		_name->SetValue (std_to_wx (_film->state_copy()->dci_name()));
		break;
	}
}

/** Called when the format widget has been changed */
void
FilmEditor::format_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::FORMAT;
	int const n = _format->GetSelection ();
	if (n >= 0) {
		assert (n < int (_formats.size()));
		_film->set_format (_formats[n]);
	}
	_ignore_changes = Film::NONE;
}

/** Called when the DCP content type widget has been changed */
void
FilmEditor::dcp_content_type_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::DCP_CONTENT_TYPE;
	int const n = _dcp_content_type->GetSelection ();
	if (n >= 0) {
		_film->set_dcp_content_type (DCPContentType::from_index (n));
	}
	_ignore_changes = Film::NONE;
}

/** Sets the Film that we are editing */
void
FilmEditor::set_film (Film* f)
{
	_film = f;

	set_things_sensitive (_film != 0);

	if (_film) {
		_film->Changed.connect (sigc::mem_fun (*this, &FilmEditor::film_changed));
	}

	if (_film) {
		FileChanged (_film->directory ());
	} else {
		FileChanged ("");
	}
	
	film_changed (Film::NAME);
	film_changed (Film::CONTENT);
	film_changed (Film::DCP_CONTENT_TYPE);
	film_changed (Film::FORMAT);
	film_changed (Film::CROP);
	film_changed (Film::FILTERS);
	film_changed (Film::DCP_FRAMES);
	film_changed (Film::DCP_TRIM_ACTION);
	film_changed (Film::DCP_AB);
	film_changed (Film::SIZE);
	film_changed (Film::LENGTH);
	film_changed (Film::FRAMES_PER_SECOND);
	film_changed (Film::AUDIO_CHANNELS);
	film_changed (Film::AUDIO_SAMPLE_RATE);
	film_changed (Film::SCALER);
	film_changed (Film::AUDIO_GAIN);
	film_changed (Film::AUDIO_DELAY);
	film_changed (Film::STILL_DURATION);
	film_changed (Film::WITH_SUBTITLES);
	film_changed (Film::SUBTITLE_OFFSET);
	film_changed (Film::SUBTITLE_SCALE);
	film_changed (Film::USE_DCI_NAME);
	film_changed (Film::DCI_METADATA);
}

/** Updates the sensitivity of lots of widgets to a given value.
 *  @param s true to make sensitive, false to make insensitive.
 */
void
FilmEditor::set_things_sensitive (bool s)
{
	_name->Enable (s);
	_use_dci_name->Enable (s);
	_edit_dci_button->Enable (s);
	_frames_per_second->Enable (s);
	_format->Enable (s);
	_content->Enable (s);
	_left_crop->Enable (s);
	_right_crop->Enable (s);
	_top_crop->Enable (s);
	_bottom_crop->Enable (s);
	_filters_button->Enable (s);
	_scaler->Enable (s);
	_dcp_content_type->Enable (s);
	_dcp_range->Enable (s);
	_change_dcp_range_button->Enable (s);
	_dcp_ab->Enable (s);
	_audio_gain->Enable (s);
	_audio_gain_calculate_button->Enable (s);
	_audio_delay->Enable (s);
	_still_duration->Enable (s);
	_with_subtitles->Enable (s);
	_subtitle_offset->Enable (s);
	_subtitle_scale->Enable (s);
}

/** Called when the `Edit filters' button has been clicked */
void
FilmEditor::edit_filters_clicked (wxCommandEvent &)
{
	FilterDialog* d = new FilterDialog (this, _film->filters ());
	d->ActiveChanged.connect (sigc::mem_fun (*_film, &Film::set_filters));
	d->ShowModal ();
	d->Destroy ();
}

/** Called when the scaler widget has been changed */
void
FilmEditor::scaler_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::SCALER;
	int const n = _scaler->GetSelection ();
	if (n >= 0) {
		_film->set_scaler (Scaler::from_index (n));
	}
	_ignore_changes = Film::NONE;
}

void
FilmEditor::audio_gain_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::AUDIO_GAIN;
	_film->set_audio_gain (_audio_gain->GetValue ());
	_ignore_changes = Film::NONE;
}

void
FilmEditor::audio_delay_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::AUDIO_DELAY;
	_film->set_audio_delay (_audio_delay->GetValue ());
	_ignore_changes = Film::NONE;
}

wxControl *
FilmEditor::video_control (wxControl* c)
{
	_video_controls.push_back (c);
	return c;
}

wxControl *
FilmEditor::still_control (wxControl* c)
{
	_still_controls.push_back (c);
	return c;
}

void
FilmEditor::setup_visibility ()
{
	ContentType c = VIDEO;

	if (_film) {
		c = _film->content_type ();
	}

	for (list<wxControl*>::iterator i = _video_controls.begin(); i != _video_controls.end(); ++i) {
		(*i)->Show (c == VIDEO);
	}

	for (list<wxControl*>::iterator i = _still_controls.begin(); i != _still_controls.end(); ++i) {
		(*i)->Show (c == STILL);
	}

	_sizer->Layout ();
}

void
FilmEditor::still_duration_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::STILL_DURATION;
	_film->set_still_duration (_still_duration->GetValue ());
	_ignore_changes = Film::NONE;
}

void
FilmEditor::change_dcp_range_clicked (wxCommandEvent &)
{
	DCPRangeDialog* d = new DCPRangeDialog (this, _film);
	d->Changed.connect (sigc::mem_fun (*this, &FilmEditor::dcp_range_changed));
	d->ShowModal ();
}

void
FilmEditor::dcp_range_changed (int frames, TrimAction action)
{
	_film->set_dcp_frames (frames);
	_film->set_dcp_trim_action (action);
}

void
FilmEditor::audio_gain_calculate_button_clicked (wxCommandEvent &)
{
	GainCalculatorDialog* d = new GainCalculatorDialog (this);
	d->ShowModal ();

	if (d->wanted_fader() == 0 || d->actual_fader() == 0) {
		d->Destroy ();
		return;
	}
	
	_audio_gain->SetValue (
		Config::instance()->sound_processor()->db_for_fader_change (
			d->wanted_fader (),
			d->actual_fader ()
			)
		);

	/* This appears to be necessary, as the change is not signalled,
	   I think.
	*/
	wxCommandEvent dummy;
	audio_gain_changed (dummy);
	
	d->Destroy ();
}

void
FilmEditor::setup_formats ()
{
	ContentType c = VIDEO;

	if (_film) {
		c = _film->content_type ();
	}
	
	_formats.clear ();

	vector<Format const *> fmt = Format::all ();
	for (vector<Format const *>::iterator i = fmt.begin(); i != fmt.end(); ++i) {
		if (c == VIDEO && dynamic_cast<FixedFormat const *> (*i)) {
			_formats.push_back (*i);
		} else if (c == STILL && dynamic_cast<VariableFormat const *> (*i)) {
			_formats.push_back (*i);
		}
	}

	_format->Clear ();
	for (vector<Format const *>::iterator i = _formats.begin(); i != _formats.end(); ++i) {
		_format->Append (std_to_wx ((*i)->name ()));
	}

	_sizer->Layout ();
}

void
FilmEditor::with_subtitles_toggled (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::WITH_SUBTITLES;
	_film->set_with_subtitles (_with_subtitles->GetValue ());
	_ignore_changes = Film::NONE;

	_subtitle_scale->Enable (_film->with_subtitles ());
	_subtitle_offset->Enable (_film->with_subtitles ());
}

void
FilmEditor::setup_subtitle_button ()
{
	_with_subtitles->Enable (_film->has_subtitles ());
	if (!_film->has_subtitles ()) {
		_with_subtitles->SetValue (false);
	}
}

void
FilmEditor::use_dci_name_toggled (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_ignore_changes = Film::USE_DCI_NAME;
	_film->set_use_dci_name (_use_dci_name->GetValue ());
	_ignore_changes = Film::NONE;
}

void
FilmEditor::edit_dci_button_clicked (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	DCINameDialog* d = new DCINameDialog (this, _film);
	d->ShowModal ();
	d->Destroy ();
}
