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
#include <wx/wx.h>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
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
//#include "filter_dialog.h"
#include "wx_util.h"
#include "film_editor.h"
//#include "dcp_range_dialog.h"

using namespace std;
using namespace boost;

/** @param f Film to edit */
FilmEditor::FilmEditor (Film* f, wxWindow* parent)
	: wxPanel (parent)
	, _film (f)
{
	_name = new wxTextCtrl (this, wxID_ANY);
	_frames_per_second = new wxSpinCtrl (this);
	_format = new wxComboBox (this, wxID_ANY);
//	_content = new wxFileCtrl (this, wxID_ANY, wxT("*.*"), false);
	_crop_panel = new wxPanel (this);
	_left_crop = new wxSpinCtrl (_crop_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
	_right_crop = new wxSpinCtrl (_crop_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
	_top_crop = new wxSpinCtrl (_crop_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
	_bottom_crop = new wxSpinCtrl (_crop_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
	_filters_panel = new wxPanel (this);
	_filters = new wxStaticText (_filters_panel, wxID_ANY, wxT (""));
	_filters_button = new wxButton (_filters_panel, wxID_ANY, wxT ("Edit..."));
	_scaler = new wxComboBox (this, wxID_ANY);
	_audio_gain_panel = new wxPanel (this);
	_audio_gain = new wxSpinCtrl (_audio_gain_panel);
	_audio_delay_panel = new wxPanel (this);
	_audio_delay = new wxSpinCtrl (_audio_delay_panel);
	_dcp_content_type = new wxComboBox (this, wxID_ANY);
	_original_size = new wxStaticText (this, wxID_ANY, wxT (""));
	_dcp_range_panel = new wxPanel (this);
	_change_dcp_range_button = new wxButton (_dcp_range_panel, wxID_ANY, wxT ("Edit..."));
	_dcp_ab = new wxCheckBox (this, wxID_ANY, wxT ("A/B"));

//XXX	_vbox.set_border_width (12);
//XXX	_vbox.set_spacing (12);
	
	/* Set up our editing widgets */
	_left_crop->SetRange (0, 1024);
//XXX	_left_crop.set_increments (1, 16);
	_top_crop->SetRange (0, 1024);
//XXX	_top_crop.set_increments (1, 16);
	_right_crop->SetRange (0, 1024);
//XXX	_right_crop.set_increments (1, 16);
	_bottom_crop->SetRange (0, 1024);
//XXX	_bottom_crop.set_increments (1, 16);
//XXX	_filters.set_alignment (0, 0.5);
	_audio_gain->SetRange (-60, 60);
//XXX	_audio_gain.set_increments (1, 3);
	_audio_delay->SetRange (-1000, 1000);
//XXX	_audio_delay.set_increments (1, 20);
	_still_duration = new wxSpinCtrl (this);
	_still_duration->SetRange (0, 60 * 60);
//XXX	_still_duration.set_increments (1, 5);
	_dcp_range = new wxStaticText (_dcp_range_panel, wxID_ANY, wxT (""));
//	_dcp_range.set_alignment (0, 0.5);

	vector<Format const *> fmt = Format::all ();
	for (vector<Format const *>::iterator i = fmt.begin(); i != fmt.end(); ++i) {
		_format->Append (wxString ((*i)->name().c_str(), wxConvUTF8));
	}

//XXX	_frames_per_second.set_increments (1, 5);
//XXX	_frames_per_second.set_digits (2);
	_frames_per_second->SetRange (0, 60);

	vector<DCPContentType const *> const ct = DCPContentType::all ();
	for (vector<DCPContentType const *>::const_iterator i = ct.begin(); i != ct.end(); ++i) {
		_dcp_content_type->Append (wxString ((*i)->pretty_name().c_str(), wxConvUTF8));
	}

	vector<Scaler const *> const sc = Scaler::all ();
	for (vector<Scaler const *>::const_iterator i = sc.begin(); i != sc.end(); ++i) {
		_scaler->Append (wxString ((*i)->name().c_str(), wxConvUTF8));
	}

//XXX	_original_size.set_alignment (0, 0.5);
	_length = new wxStaticText (this, wxID_ANY, wxT (""));
//XXX	_length.set_alignment (0, 0.5);
	_audio = new wxStaticText (this, wxID_ANY, wxT (""));
//XXX	_audio.set_alignment (0, 0.5);

	/* And set their values from the Film */
	set_film (f);
	
	/* Now connect to them, since initial values are safely set */
	_name->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (FilmEditor::name_changed), 0, this);
	_frames_per_second->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::frames_per_second_changed), 0, this);
	_format->Connect (wxID_ANY, wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler (FilmEditor::format_changed), 0, this);
//XXX	_content.signal_file_set().connect (sigc::mem_fun (*this, &FilmEditor::content_changed));
	_left_crop->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::left_crop_changed), 0, this);
	_right_crop->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::right_crop_changed), 0, this);
	_top_crop->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::top_crop_changed), 0, this);
	_bottom_crop->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::bottom_crop_changed), 0, this);
	_filters_button->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (FilmEditor::edit_filters_clicked), 0, this);
	_scaler->Connect (wxID_ANY, wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler (FilmEditor::scaler_changed), 0, this);
	_dcp_content_type->Connect (wxID_ANY, wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler (FilmEditor::dcp_content_type_changed), 0, this);
	_dcp_ab->Connect (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler (FilmEditor::dcp_ab_toggled), 0, this);
	_audio_gain->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::audio_gain_changed), 0, this);
	_audio_delay->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::audio_delay_changed), 0, this);
	_still_duration->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::still_duration_changed), 0, this);
	_change_dcp_range_button->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (FilmEditor::change_dcp_range_clicked), 0, this);


	/* Set up the sizer */
	_sizer = new wxFlexGridSizer (2, 6, 6);
	this->SetSizer (_sizer);

	add_label_to_sizer (_sizer, this, _labels, "Name");
	_sizer->Add (_name, 1, wxEXPAND);

#if 0
	add_label_to_sizer (_sizer, this, _labels, "Content");
	_sizer->Add (_content, 1, wxEXPAND);
#endif	

	add_label_to_sizer (_sizer, this, _labels, "Content Type");
	_sizer->Add (_dcp_content_type);

	add_label_to_sizer (_sizer, this, _labels, "Frames Per Second");
	_sizer->Add (video_control (_frames_per_second));

	add_label_to_sizer (_sizer, this, _labels, "Format");
	_sizer->Add (_format);

	add_label_to_sizer (_sizer, this, _labels, "Crop");
	_crop_sizer = new wxBoxSizer (wxHORIZONTAL);
	_crop_panel->SetSizer (_crop_sizer);

	add_label_to_sizer (_crop_sizer, _crop_panel, _labels, "L");
	_crop_sizer->Add (_left_crop, 0);
	add_label_to_sizer (_crop_sizer, _crop_panel, _labels, "R");
	_crop_sizer->Add (_right_crop, 0);
	add_label_to_sizer (_crop_sizer, _crop_panel, _labels, "T");
	_crop_sizer->Add (_top_crop, 0);
	add_label_to_sizer (_crop_sizer, _crop_panel, _labels, "B");
	_crop_sizer->Add (_bottom_crop, 0);

	_sizer->Add (_crop_panel);

	/* VIDEO-only stuff */
	video_control (add_label_to_sizer (_sizer, this, _labels, "Filters"));
	_filters_sizer = new wxBoxSizer (wxHORIZONTAL);
	_filters_panel->SetSizer (_filters_sizer);
	_filters_sizer->Add (_filters, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxALL, 6);
	_filters_sizer->Add (_filters_button, 0);
	_sizer->Add (_filters_panel, 1);

	video_control (add_label_to_sizer (_sizer, this, _labels, "Scaler"));
	_sizer->Add (video_control (_scaler), 1);

	video_control (add_label_to_sizer (_sizer, this, _labels, "Audio Gain"));
	_audio_gain_sizer = new wxBoxSizer (wxHORIZONTAL);
	_audio_gain_panel->SetSizer (_audio_gain_sizer);
	_audio_gain_sizer->Add (video_control (_audio_gain), 1);
	video_control (add_label_to_sizer (_audio_gain_sizer, _audio_gain_panel, _labels, "dB"));
	_sizer->Add (_audio_gain_panel);

	video_control (add_label_to_sizer (_sizer, this, _labels, "Audio Delay"));
	_audio_delay_sizer = new wxBoxSizer (wxHORIZONTAL);
	_audio_delay_panel->SetSizer (_audio_delay_sizer);
	_audio_delay_sizer->Add (video_control (_audio_delay), 1);
	video_control (add_label_to_sizer (_audio_delay_sizer, _audio_delay_panel, _labels, "ms"));
	_sizer->Add (_audio_delay_panel);

	video_control (add_label_to_sizer (_sizer, this, _labels, "Original Size"));
	_sizer->Add (video_control (_original_size), 1, wxALIGN_CENTER_VERTICAL);
	
	video_control (add_label_to_sizer (_sizer, this, _labels, "Length"));
	_sizer->Add (video_control (_length), 1, wxALIGN_CENTER_VERTICAL);

	video_control (add_label_to_sizer (_sizer, this, _labels, "Audio"));
	_sizer->Add (video_control (_audio), 1, wxALIGN_CENTER_VERTICAL);

	video_control (add_label_to_sizer (_sizer, this, _labels, "Range"));
	_dcp_range_sizer = new wxBoxSizer (wxHORIZONTAL);
	_dcp_range_panel->SetSizer (_dcp_range_sizer);
	_dcp_range_sizer->Add (video_control (_dcp_range), 1, wxALIGN_CENTER_VERTICAL);
	_dcp_range_sizer->Add (video_control (_change_dcp_range_button));
	_sizer->Add (_dcp_range_panel);

	_sizer->Add (_dcp_ab, 1);
	_sizer->AddSpacer (0);

	/* STILL-only stuff */
	still_control (add_label_to_sizer (_sizer, this, _labels, "Duration"));
	_sizer->Add (still_control (_still_duration));
	still_control (add_label_to_sizer (_sizer, this, _labels, "s"));

	setup_visibility ();
}

/** Called when the left crop widget has been changed */
void
FilmEditor::left_crop_changed (wxCommandEvent &)
{
	if (_film) {
		_film->set_left_crop (_left_crop->GetValue ());
	}
}

/** Called when the right crop widget has been changed */
void
FilmEditor::right_crop_changed (wxCommandEvent &)
{
	if (_film) {
		_film->set_right_crop (_right_crop->GetValue ());
	}
}

/** Called when the top crop widget has been changed */
void
FilmEditor::top_crop_changed (wxCommandEvent &)
{
	if (_film) {
		_film->set_top_crop (_top_crop->GetValue ());
	}
}

/** Called when the bottom crop value has been changed */
void
FilmEditor::bottom_crop_changed (wxCommandEvent &)
{
	if (_film) {
		_film->set_bottom_crop (_bottom_crop->GetValue ());
	}
}

/** Called when the content filename has been changed */
void
FilmEditor::content_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	try {
//XXX		_film->set_content (_content.get_filename ());
	} catch (std::exception& e) {
//XXX		_content.set_filename (_film->directory ());
		stringstream m;
		m << "Could not set content: " << e.what() << ".";
//XXX		Gtk::MessageDialog d (m.str(), false, MESSAGE_ERROR);
//XXX		d.set_title ("DVD-o-matic");
//XXX		d.run ();
	}
}

/** Called when the DCP A/B switch has been toggled */
void
FilmEditor::dcp_ab_toggled (wxCommandEvent &)
{
	if (_film) {
		_film->set_dcp_ab (_dcp_ab->GetValue ());
	}
}

/** Called when the name widget has been changed */
void
FilmEditor::name_changed (wxCommandEvent &)
{
	if (_film) {
		_film->set_name (string (_name->GetValue().mb_str()));
	}
}

/** Called when the metadata stored in the Film object has changed;
 *  so that we can update the GUI.
 *  @param p Property of the Film that has changed.
 */
void
FilmEditor::film_changed (Film::Property p)
{
	if (!_film) {
		return;
	}
	
	stringstream s;
		
	switch (p) {
	case Film::CONTENT:
//XXX		_content.set_filename (_film->content ());
		setup_visibility ();
		break;
	case Film::FORMAT:
		_format->SetSelection (Format::as_index (_film->format ()));
		break;
	case Film::LEFT_CROP:
		_left_crop->SetValue (_film->left_crop ());
		break;
	case Film::RIGHT_CROP:
		_right_crop->SetValue (_film->right_crop ());
		break;
	case Film::TOP_CROP:
		_top_crop->SetValue (_film->top_crop ());
		break;
	case Film::BOTTOM_CROP:
		_bottom_crop->SetValue (_film->bottom_crop ());
		break;
	case Film::FILTERS:
	{
		pair<string, string> p = Filter::ffmpeg_strings (_film->filters ());
		string const b = p.first + " " + p.second;
		_filters->SetLabel (wxString (b.c_str(), wxConvUTF8));
		break;
	}
	case Film::NAME:
		_name->SetValue (wxString (_film->name().c_str(), wxConvUTF8));
		break;
	case Film::FRAMES_PER_SECOND:
		_frames_per_second->SetValue (_film->frames_per_second ());
		break;
	case Film::AUDIO_CHANNELS:
	case Film::AUDIO_SAMPLE_RATE:
		if (_film->audio_channels() == 0 && _film->audio_sample_rate() == 0) {
			_audio->SetLabel (wxT (""));
		} else {
			s << _film->audio_channels () << " channels, " << _film->audio_sample_rate() << "Hz";
			_audio->SetLabel (wxString (s.str().c_str(), wxConvUTF8));
		}
		break;
	case Film::SIZE:
		if (_film->size().width == 0 && _film->size().height == 0) {
			_original_size->SetLabel (wxT (""));
		} else {
			s << _film->size().width << " x " << _film->size().height;
			_original_size->SetLabel (wxString (s.str().c_str(), wxConvUTF8));
		}
		break;
	case Film::LENGTH:
		if (_film->frames_per_second() > 0 && _film->length() > 0) {
			s << _film->length() << " frames; " << seconds_to_hms (_film->length() / _film->frames_per_second());
		} else if (_film->length() > 0) {
			s << _film->length() << " frames";
		} 
		_length->SetLabel (wxString (s.str().c_str(), wxConvUTF8));
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
			_dcp_range->SetLabel (wxString (s.str().c_str(), wxConvUTF8));
		}
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
	}
}

/** Called when the format widget has been changed */
void
FilmEditor::format_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	int const n = _format->GetSelection ();
	if (n >= 0) {
		_film->set_format (Format::from_index (n));
	}
}

/** Called when the DCP content type widget has been changed */
void
FilmEditor::dcp_content_type_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}
	
	int const n = _dcp_content_type->GetSelection ();
	if (n >= 0) {
		_film->set_dcp_content_type (DCPContentType::from_index (n));
	}
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
//		FileChanged (_film->directory ());
	} else {
//		FileChanged ("");
	}
	
	film_changed (Film::NAME);
	film_changed (Film::CONTENT);
	film_changed (Film::DCP_CONTENT_TYPE);
	film_changed (Film::FORMAT);
	film_changed (Film::LEFT_CROP);
	film_changed (Film::RIGHT_CROP);
	film_changed (Film::TOP_CROP);
	film_changed (Film::BOTTOM_CROP);
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
}

/** Updates the sensitivity of lots of widgets to a given value.
 *  @param s true to make sensitive, false to make insensitive.
 */
void
FilmEditor::set_things_sensitive (bool s)
{
	_name->Enable (s);
	_frames_per_second->Enable (s);
	_format->Enable (s);
//	_content->Enable (s);
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
	_audio_delay->Enable (s);
	_still_duration->Enable (s);
}

/** Called when the `Edit filters' button has been clicked */
void
FilmEditor::edit_filters_clicked (wxCommandEvent &)
{
//	FilterDialog d (_film->filters ());
//	d.ActiveChanged.connect (sigc::mem_fun (*_film, &Film::set_filters));
//	d.run ();
}

/** Called when the scaler widget has been changed */
void
FilmEditor::scaler_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}
	
	int const n = _scaler->GetSelection ();
	if (n >= 0) {
		_film->set_scaler (Scaler::from_index (n));
	}
}

/** Called when the frames per second widget has been changed */
void
FilmEditor::frames_per_second_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}
	
	_film->set_frames_per_second (_frames_per_second->GetValue ());
}

void
FilmEditor::audio_gain_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}
	
	_film->set_audio_gain (_audio_gain->GetValue ());
}

void
FilmEditor::audio_delay_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_audio_delay (_audio_delay->GetValue ());
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
}

void
FilmEditor::still_duration_changed (wxCommandEvent &)
{
	if (_film) {
		_film->set_still_duration (_still_duration->GetValue ());
	}
}

void
FilmEditor::change_dcp_range_clicked (wxCommandEvent &)
{
//XXX	DCPRangeDialog d (_film);
//XXX	d.Changed.connect (sigc::mem_fun (*this, &FilmEditor::dcp_range_changed));
//XXX	d.run ();
}

void
FilmEditor::dcp_range_changed (int frames, TrimAction action)
{
	_film->set_dcp_frames (frames);
	_film->set_dcp_trim_action (action);
}
