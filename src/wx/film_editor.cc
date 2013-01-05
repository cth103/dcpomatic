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
#include <wx/notebook.h>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include "lib/format.h"
#include "lib/film.h"
#include "lib/transcode_job.h"
#include "lib/exceptions.h"
#include "lib/ab_transcode_job.h"
#include "lib/job_manager.h"
#include "lib/filter.h"
#include "lib/config.h"
#include "lib/ffmpeg_decoder.h"
#include "lib/external_audio_decoder.h"
#include "filter_dialog.h"
#include "wx_util.h"
#include "film_editor.h"
#include "gain_calculator_dialog.h"
#include "sound_processor.h"
#include "dci_name_dialog.h"
#include "scaler.h"

using std::string;
using std::cout;
using std::stringstream;
using std::pair;
using std::fixed;
using std::setprecision;
using std::list;
using std::vector;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

/** @param f Film to edit */
FilmEditor::FilmEditor (shared_ptr<Film> f, wxWindow* parent)
	: wxPanel (parent)
	, _film (f)
	, _generally_sensitive (true)
{
	wxBoxSizer* s = new wxBoxSizer (wxVERTICAL);
	SetSizer (s);
	_notebook = new wxNotebook (this, wxID_ANY);
	s->Add (_notebook, 1);

	make_film_panel ();
	_notebook->AddPage (_film_panel, _("Film"), true);
	make_video_panel ();
	_notebook->AddPage (_video_panel, _("Video"), false);
	make_audio_panel ();
	_notebook->AddPage (_audio_panel, _("Audio"), false);
	make_subtitle_panel ();
	_notebook->AddPage (_subtitle_panel, _("Subtitles"), false);

	set_film (_film);
	connect_to_widgets ();

	JobManager::instance()->ActiveJobsChanged.connect (
		bind (&FilmEditor::active_jobs_changed, this, _1)
		);
	
	setup_visibility ();
	setup_formats ();
}

void
FilmEditor::make_film_panel ()
{
	_film_panel = new wxPanel (_notebook);
	_film_sizer = new wxFlexGridSizer (2, 4, 4);
	wxBoxSizer* pad = new wxBoxSizer (wxVERTICAL);
	pad->Add (_film_sizer, 0, wxALL, 8);
	_film_panel->SetSizer (pad);

	add_label_to_sizer (_film_sizer, _film_panel, "Name");
	_name = new wxTextCtrl (_film_panel, wxID_ANY);
	_film_sizer->Add (_name, 1, wxEXPAND);

	add_label_to_sizer (_film_sizer, _film_panel, "DCP Name");
	_dcp_name = new wxStaticText (_film_panel, wxID_ANY, wxT (""));
	_film_sizer->Add (_dcp_name, 0, wxALIGN_CENTER_VERTICAL | wxSHRINK);

	_use_dci_name = new wxCheckBox (_film_panel, wxID_ANY, wxT ("Use DCI name"));
	_film_sizer->Add (_use_dci_name, 1, wxEXPAND);
	_edit_dci_button = new wxButton (_film_panel, wxID_ANY, wxT ("Details..."));
	_film_sizer->Add (_edit_dci_button, 0);

	add_label_to_sizer (_film_sizer, _film_panel, "Content");
	_content = new wxFilePickerCtrl (_film_panel, wxID_ANY, wxT (""), wxT ("Select Content File"), wxT("*.*"));
	_film_sizer->Add (_content, 1, wxEXPAND);

	_trust_content_header = new wxCheckBox (_film_panel, wxID_ANY, wxT ("Trust content's header"));
	video_control (_trust_content_header);
	_film_sizer->Add (_trust_content_header, 1);
	_film_sizer->AddSpacer (0);

	add_label_to_sizer (_film_sizer, _film_panel, "Content Type");
	_dcp_content_type = new wxComboBox (_film_panel, wxID_ANY, wxT (""), wxDefaultPosition, wxDefaultSize, 0, 0, wxCB_READONLY);
	_film_sizer->Add (_dcp_content_type);

	video_control (add_label_to_sizer (_film_sizer, _film_panel, "Frames Per Second"));
	_frames_per_second = new wxStaticText (_film_panel, wxID_ANY, wxT (""));
	_film_sizer->Add (video_control (_frames_per_second), 1, wxALIGN_CENTER_VERTICAL);
	
	video_control (add_label_to_sizer (_film_sizer, _film_panel, "Original Size"));
	_original_size = new wxStaticText (_film_panel, wxID_ANY, wxT (""));
	_film_sizer->Add (video_control (_original_size), 1, wxALIGN_CENTER_VERTICAL);
	
	video_control (add_label_to_sizer (_film_sizer, _film_panel, "Length"));
	_length = new wxStaticText (_film_panel, wxID_ANY, wxT (""));
	_film_sizer->Add (video_control (_length), 1, wxALIGN_CENTER_VERTICAL);


	{
		video_control (add_label_to_sizer (_film_sizer, _film_panel, "Trim frames"));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		video_control (add_label_to_sizer (s, _film_panel, "Start"));
		_dcp_trim_start = new wxSpinCtrl (_film_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
		s->Add (video_control (_dcp_trim_start));
		video_control (add_label_to_sizer (s, _film_panel, "End"));
		_dcp_trim_end = new wxSpinCtrl (_film_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
		s->Add (video_control (_dcp_trim_end));

		_film_sizer->Add (s);
	}

	_encrypted = new wxCheckBox (_film_panel, wxID_ANY, wxT ("Encrypted"));
	_film_sizer->Add (_encrypted, 1);
	_film_sizer->AddSpacer (0);

	_dcp_ab = new wxCheckBox (_film_panel, wxID_ANY, wxT ("A/B"));
	video_control (_dcp_ab);
	_film_sizer->Add (_dcp_ab, 1);
	_film_sizer->AddSpacer (0);

	/* STILL-only stuff */
	{
		still_control (add_label_to_sizer (_film_sizer, _film_panel, "Duration"));
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_still_duration = new wxSpinCtrl (_film_panel);
		still_control (_still_duration);
		s->Add (_still_duration, 1, wxEXPAND);
		still_control (add_label_to_sizer (s, _film_panel, "s"));
		_film_sizer->Add (s);
	}

	vector<DCPContentType const *> const ct = DCPContentType::all ();
	for (vector<DCPContentType const *>::const_iterator i = ct.begin(); i != ct.end(); ++i) {
		_dcp_content_type->Append (std_to_wx ((*i)->pretty_name ()));
	}
}

void
FilmEditor::connect_to_widgets ()
{
	_name->Connect (wxID_ANY, wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler (FilmEditor::name_changed), 0, this);
	_use_dci_name->Connect (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler (FilmEditor::use_dci_name_toggled), 0, this);
	_edit_dci_button->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (FilmEditor::edit_dci_button_clicked), 0, this);
	_format->Connect (wxID_ANY, wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler (FilmEditor::format_changed), 0, this);
	_content->Connect (wxID_ANY, wxEVT_COMMAND_FILEPICKER_CHANGED, wxCommandEventHandler (FilmEditor::content_changed), 0, this);
	_trust_content_header->Connect (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler (FilmEditor::trust_content_header_changed), 0, this);
	_left_crop->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::left_crop_changed), 0, this);
	_right_crop->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::right_crop_changed), 0, this);
	_top_crop->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::top_crop_changed), 0, this);
	_bottom_crop->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::bottom_crop_changed), 0, this);
	_filters_button->Connect (wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (FilmEditor::edit_filters_clicked), 0, this);
	_scaler->Connect (wxID_ANY, wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler (FilmEditor::scaler_changed), 0, this);
	_dcp_content_type->Connect (wxID_ANY, wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler (FilmEditor::dcp_content_type_changed), 0, this);
	_dcp_ab->Connect (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler (FilmEditor::dcp_ab_toggled), 0, this);
	_encrypted->Connect (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler (FilmEditor::encrypted_toggled), 0, this);
	_still_duration->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::still_duration_changed), 0, this);
	_dcp_trim_start->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::dcp_trim_start_changed), 0, this);
	_dcp_trim_end->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::dcp_trim_end_changed), 0, this);
	_with_subtitles->Connect (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler (FilmEditor::with_subtitles_toggled), 0, this);
	_subtitle_offset->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::subtitle_offset_changed), 0, this);
	_subtitle_scale->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::subtitle_scale_changed), 0, this);
	_subtitle_stream->Connect (wxID_ANY, wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler (FilmEditor::subtitle_stream_changed), 0, this);
	_audio_stream->Connect (wxID_ANY, wxEVT_COMMAND_COMBOBOX_SELECTED, wxCommandEventHandler (FilmEditor::audio_stream_changed), 0, this);
	_audio_gain->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::audio_gain_changed), 0, this);
	_audio_gain_calculate_button->Connect (
		wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler (FilmEditor::audio_gain_calculate_button_clicked), 0, this
		);
	_audio_delay->Connect (wxID_ANY, wxEVT_COMMAND_SPINCTRL_UPDATED, wxCommandEventHandler (FilmEditor::audio_delay_changed), 0, this);
	_use_content_audio->Connect (wxID_ANY, wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler (FilmEditor::use_audio_changed), 0, this);
	_use_external_audio->Connect (wxID_ANY, wxEVT_COMMAND_RADIOBUTTON_SELECTED, wxCommandEventHandler (FilmEditor::use_audio_changed), 0, this);
	for (int i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
		_external_audio[i]->Connect (
			wxID_ANY, wxEVT_COMMAND_FILEPICKER_CHANGED, wxCommandEventHandler (FilmEditor::external_audio_changed), 0, this
			);
	}
}

void
FilmEditor::make_video_panel ()
{
	_video_panel = new wxPanel (_notebook);
	_video_sizer = new wxFlexGridSizer (2, 4, 4);
	wxBoxSizer* pad = new wxBoxSizer (wxVERTICAL);
	pad->Add (_video_sizer, 0, wxALL, 8);
	_video_panel->SetSizer (pad);

	add_label_to_sizer (_video_sizer, _video_panel, "Format");
	_format = new wxComboBox (_video_panel, wxID_ANY, wxT (""), wxDefaultPosition, wxDefaultSize, 0, 0, wxCB_READONLY);
	_video_sizer->Add (_format);

	{
		add_label_to_sizer (_video_sizer, _video_panel, "Crop");
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);

		add_label_to_sizer (s, _video_panel, "L");
		_left_crop = new wxSpinCtrl (_video_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
		s->Add (_left_crop, 0);
		add_label_to_sizer (s, _video_panel, "R");
		_right_crop = new wxSpinCtrl (_video_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
		s->Add (_right_crop, 0);
		add_label_to_sizer (s, _video_panel, "T");
		_top_crop = new wxSpinCtrl (_video_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
		s->Add (_top_crop, 0);
		add_label_to_sizer (s, _video_panel, "B");
		_bottom_crop = new wxSpinCtrl (_video_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (64, -1));
		s->Add (_bottom_crop, 0);

		_video_sizer->Add (s);
	}

	/* VIDEO-only stuff */
	{
		video_control (add_label_to_sizer (_video_sizer, _video_panel, "Filters"));
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_filters = new wxStaticText (_video_panel, wxID_ANY, wxT ("None"));
		video_control (_filters);
		s->Add (_filters, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);
		_filters_button = new wxButton (_video_panel, wxID_ANY, wxT ("Edit..."));
		video_control (_filters_button);
		s->Add (_filters_button, 0);
		_video_sizer->Add (s, 1);
	}

	video_control (add_label_to_sizer (_video_sizer, _video_panel, "Scaler"));
	_scaler = new wxComboBox (_video_panel, wxID_ANY, wxT (""), wxDefaultPosition, wxDefaultSize, 0, 0, wxCB_READONLY);
	_video_sizer->Add (video_control (_scaler), 1);

	vector<Scaler const *> const sc = Scaler::all ();
	for (vector<Scaler const *>::const_iterator i = sc.begin(); i != sc.end(); ++i) {
		_scaler->Append (std_to_wx ((*i)->name()));
	}

	_left_crop->SetRange (0, 1024);
	_top_crop->SetRange (0, 1024);
	_right_crop->SetRange (0, 1024);
	_bottom_crop->SetRange (0, 1024);
	_still_duration->SetRange (1, 60 * 60);
	_dcp_trim_start->SetRange (0, 100);
	_dcp_trim_end->SetRange (0, 100);
}

void
FilmEditor::make_audio_panel ()
{
	_audio_panel = new wxPanel (_notebook);
	_audio_sizer = new wxFlexGridSizer (2, 4, 4);
	wxBoxSizer* pad = new wxBoxSizer (wxVERTICAL);
	pad->Add (_audio_sizer, 0, wxALL, 8);
	_audio_panel->SetSizer (pad);

	{
		video_control (add_label_to_sizer (_audio_sizer, _audio_panel, "Audio Gain"));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_audio_gain = new wxSpinCtrl (_audio_panel);
		s->Add (video_control (_audio_gain), 1);
		video_control (add_label_to_sizer (s, _audio_panel, "dB"));
		_audio_gain_calculate_button = new wxButton (_audio_panel, wxID_ANY, _("Calculate..."));
		video_control (_audio_gain_calculate_button);
		s->Add (_audio_gain_calculate_button, 1, wxEXPAND);
		_audio_sizer->Add (s);
	}

	{
		video_control (add_label_to_sizer (_audio_sizer, _audio_panel, "Audio Delay"));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_audio_delay = new wxSpinCtrl (_audio_panel);
		s->Add (video_control (_audio_delay), 1);
		video_control (add_label_to_sizer (s, _audio_panel, "ms"));
		_audio_sizer->Add (s);
	}

	{
		_use_content_audio = new wxRadioButton (_audio_panel, wxID_ANY, _("Use content's audio"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
		_audio_sizer->Add (video_control (_use_content_audio));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_audio_stream = new wxComboBox (_audio_panel, wxID_ANY, wxT (""), wxDefaultPosition, wxDefaultSize, 0, 0, wxCB_READONLY);
		s->Add (video_control (_audio_stream), 1);
		_audio = new wxStaticText (_audio_panel, wxID_ANY, wxT (""));
		s->Add (video_control (_audio), 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
		_audio_sizer->Add (s, 1, wxEXPAND);
	}

	_use_external_audio = new wxRadioButton (_audio_panel, wxID_ANY, _("Use external audio"));
	_audio_sizer->Add (_use_external_audio);
	_audio_sizer->AddSpacer (0);

	assert (MAX_AUDIO_CHANNELS == 6);

	char const * channels[] = {
		"Left",
		"Right",
		"Centre",
		"Lfe (sub)",
		"Left surround",
		"Right surround"
	};

	for (int i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
		add_label_to_sizer (_audio_sizer, _audio_panel, channels[i]);
		_external_audio[i] = new wxFilePickerCtrl (_audio_panel, wxID_ANY, wxT (""), wxT ("Select Audio File"), wxT ("*.wav"));
		_audio_sizer->Add (_external_audio[i], 1, wxEXPAND);
	}

	_audio_gain->SetRange (-60, 60);
	_audio_delay->SetRange (-1000, 1000);
}

void
FilmEditor::make_subtitle_panel ()
{
	_subtitle_panel = new wxPanel (_notebook);
	_subtitle_sizer = new wxFlexGridSizer (2, 4, 4);
	wxBoxSizer* pad = new wxBoxSizer (wxVERTICAL);
	pad->Add (_subtitle_sizer, 0, wxALL, 8);
	_subtitle_panel->SetSizer (pad);

	_with_subtitles = new wxCheckBox (_subtitle_panel, wxID_ANY, wxT("With Subtitles"));
	video_control (_with_subtitles);
	_subtitle_sizer->Add (_with_subtitles, 1);
	
	_subtitle_stream = new wxComboBox (_subtitle_panel, wxID_ANY, wxT (""), wxDefaultPosition, wxDefaultSize, 0, 0, wxCB_READONLY);
	_subtitle_sizer->Add (video_control (_subtitle_stream));

	video_control (add_label_to_sizer (_subtitle_sizer, _subtitle_panel, "Subtitle Offset"));
	_subtitle_offset = new wxSpinCtrl (_subtitle_panel);
	_subtitle_sizer->Add (video_control (_subtitle_offset), 1);

	{
		video_control (add_label_to_sizer (_subtitle_sizer, _subtitle_panel, "Subtitle Scale"));
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_subtitle_scale = new wxSpinCtrl (_subtitle_panel);
		s->Add (video_control (_subtitle_scale));
		video_control (add_label_to_sizer (s, _subtitle_panel, "%"));
		_subtitle_sizer->Add (s);
	}

	_subtitle_offset->SetRange (-1024, 1024);
	_subtitle_scale->SetRange (1, 1000);
}

/** Called when the left crop widget has been changed */
void
FilmEditor::left_crop_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_left_crop (_left_crop->GetValue ());
}

/** Called when the right crop widget has been changed */
void
FilmEditor::right_crop_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_right_crop (_right_crop->GetValue ());
}

/** Called when the top crop widget has been changed */
void
FilmEditor::top_crop_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_top_crop (_top_crop->GetValue ());
}

/** Called when the bottom crop value has been changed */
void
FilmEditor::bottom_crop_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_bottom_crop (_bottom_crop->GetValue ());
}

/** Called when the content filename has been changed */
void
FilmEditor::content_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	try {
		_film->set_content (wx_to_std (_content->GetPath ()));
	} catch (std::exception& e) {
		_content->SetPath (std_to_wx (_film->directory ()));
		error_dialog (this, String::compose ("Could not set content: %1", e.what ()));
	}
}

void
FilmEditor::trust_content_header_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_trust_content_header (_trust_content_header->GetValue ());
}

/** Called when the DCP A/B switch has been toggled */
void
FilmEditor::dcp_ab_toggled (wxCommandEvent &)
{
	if (!_film) {
		return;
	}
	
	_film->set_dcp_ab (_dcp_ab->GetValue ());
}

void
FilmEditor::encrypted_toggled (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_encrypted (_encrypted->GetValue ());
}
			       
/** Called when the name widget has been changed */
void
FilmEditor::name_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_name (string (_name->GetValue().mb_str()));
}

void
FilmEditor::subtitle_offset_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_subtitle_offset (_subtitle_offset->GetValue ());
}

void
FilmEditor::subtitle_scale_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_subtitle_scale (_subtitle_scale->GetValue() / 100.0);
}


/** Called when the metadata stored in the Film object has changed;
 *  so that we can update the GUI.
 *  @param p Property of the Film that has changed.
 */
void
FilmEditor::film_changed (Film::Property p)
{
	ensure_ui_thread ();
	
	if (!_film) {
		return;
	}

	stringstream s;
		
	switch (p) {
	case Film::NONE:
		break;
	case Film::CONTENT:
		checked_set (_content, _film->content ());
		setup_visibility ();
		setup_formats ();
		setup_subtitle_control_sensitivity ();
		setup_streams ();
		break;
	case Film::TRUST_CONTENT_HEADER:
		checked_set (_trust_content_header, _film->trust_content_header ());
		break;
	case Film::SUBTITLE_STREAMS:
		setup_subtitle_control_sensitivity ();
		setup_streams ();
		break;
	case Film::CONTENT_AUDIO_STREAMS:
		setup_streams ();
		break;
	case Film::FORMAT:
	{
		int n = 0;
		vector<Format const *>::iterator i = _formats.begin ();
		while (i != _formats.end() && *i != _film->format ()) {
			++i;
			++n;
		}
		if (i == _formats.end()) {
			checked_set (_format, -1);
		} else {
			checked_set (_format, n);
		}
		_dcp_name->SetLabel (std_to_wx (_film->dcp_name ()));
		break;
	}
	case Film::CROP:
		checked_set (_left_crop, _film->crop().left);
		checked_set (_right_crop, _film->crop().right);
		checked_set (_top_crop, _film->crop().top);
		checked_set (_bottom_crop, _film->crop().bottom);
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
		_film_sizer->Layout ();
		break;
	}
	case Film::NAME:
		checked_set (_name, _film->name());
		_film->set_dci_date_today ();
		_dcp_name->SetLabel (std_to_wx (_film->dcp_name ()));
		break;
	case Film::FRAMES_PER_SECOND:
		s << fixed << setprecision(2) << _film->frames_per_second();
		_frames_per_second->SetLabel (std_to_wx (s.str ()));
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
		if (_film->frames_per_second() > 0 && _film->length()) {
			s << _film->length().get() << " frames; " << seconds_to_hms (_film->length().get() / _film->frames_per_second());
		} else if (_film->length()) {
			s << _film->length().get() << " frames";
		} 
		_length->SetLabel (std_to_wx (s.str ()));
		if (_film->length()) {
			_dcp_trim_start->SetRange (0, _film->length().get());
			_dcp_trim_end->SetRange (0, _film->length().get());
		}
		break;
	case Film::DCP_CONTENT_TYPE:
		checked_set (_dcp_content_type, DCPContentType::as_index (_film->dcp_content_type ()));
		_dcp_name->SetLabel (std_to_wx (_film->dcp_name ()));
		break;
	case Film::DCP_AB:
		checked_set (_dcp_ab, _film->dcp_ab ());
		break;
	case Film::SCALER:
		checked_set (_scaler, Scaler::as_index (_film->scaler ()));
		break;
	case Film::DCP_TRIM_START:
		checked_set (_dcp_trim_start, _film->dcp_trim_start());
		break;
	case Film::DCP_TRIM_END:
		checked_set (_dcp_trim_end, _film->dcp_trim_end());
		break;
	case Film::AUDIO_GAIN:
		checked_set (_audio_gain, _film->audio_gain ());
		break;
	case Film::AUDIO_DELAY:
		checked_set (_audio_delay, _film->audio_delay ());
		break;
	case Film::STILL_DURATION:
		checked_set (_still_duration, _film->still_duration ());
		break;
	case Film::WITH_SUBTITLES:
		checked_set (_with_subtitles, _film->with_subtitles ());
		setup_subtitle_control_sensitivity ();
		_dcp_name->SetLabel (std_to_wx (_film->dcp_name ()));
		break;
	case Film::SUBTITLE_OFFSET:
		checked_set (_subtitle_offset, _film->subtitle_offset ());
		break;
	case Film::SUBTITLE_SCALE:
		checked_set (_subtitle_scale, _film->subtitle_scale() * 100);
		break;
	case Film::ENCRYPTED:
		checked_set (_encrypted, _film->encrypted ());
		break;
	case Film::USE_DCI_NAME:
		checked_set (_use_dci_name, _film->use_dci_name ());
		_dcp_name->SetLabel (std_to_wx (_film->dcp_name ()));
		break;
	case Film::DCI_METADATA:
		_dcp_name->SetLabel (std_to_wx (_film->dcp_name ()));
		break;
	case Film::CONTENT_AUDIO_STREAM:
		if (_film->content_audio_stream()) {
			checked_set (_audio_stream, _film->content_audio_stream()->to_string());
		}
		_dcp_name->SetLabel (std_to_wx (_film->dcp_name ()));
		setup_audio_details ();
		setup_audio_control_sensitivity ();
		break;
	case Film::USE_CONTENT_AUDIO:
		checked_set (_use_content_audio, _film->use_content_audio());
		checked_set (_use_external_audio, !_film->use_content_audio());
		_dcp_name->SetLabel (std_to_wx (_film->dcp_name ()));
		setup_audio_details ();
		setup_audio_control_sensitivity ();
		break;
	case Film::SUBTITLE_STREAM:
		if (_film->subtitle_stream()) {
			checked_set (_subtitle_stream, _film->subtitle_stream()->to_string());
		}
		break;
	case Film::EXTERNAL_AUDIO:
	{
		vector<string> a = _film->external_audio ();
		for (size_t i = 0; i < a.size() && i < MAX_AUDIO_CHANNELS; ++i) {
			checked_set (_external_audio[i], a[i]);
		}
		setup_audio_details ();
		break;
	}
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
		assert (n < int (_formats.size()));
		_film->set_format (_formats[n]);
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
	if (n != wxNOT_FOUND) {
		_film->set_dcp_content_type (DCPContentType::from_index (n));
	}
}

/** Sets the Film that we are editing */
void
FilmEditor::set_film (shared_ptr<Film> f)
{
	_film = f;

	set_things_sensitive (_film != 0);

	if (_film) {
		_film->Changed.connect (bind (&FilmEditor::film_changed, this, _1));
	}

	if (_film) {
		FileChanged (_film->directory ());
	} else {
		FileChanged ("");
	}
	
	film_changed (Film::NAME);
	film_changed (Film::USE_DCI_NAME);
	film_changed (Film::CONTENT);
	film_changed (Film::TRUST_CONTENT_HEADER);
	film_changed (Film::DCP_CONTENT_TYPE);
	film_changed (Film::FORMAT);
	film_changed (Film::CROP);
	film_changed (Film::FILTERS);
	film_changed (Film::SCALER);
	film_changed (Film::DCP_TRIM_START);
	film_changed (Film::DCP_TRIM_END);
	film_changed (Film::DCP_AB);
	film_changed (Film::CONTENT_AUDIO_STREAM);
	film_changed (Film::EXTERNAL_AUDIO);
	film_changed (Film::USE_CONTENT_AUDIO);
	film_changed (Film::AUDIO_GAIN);
	film_changed (Film::AUDIO_DELAY);
	film_changed (Film::STILL_DURATION);
	film_changed (Film::WITH_SUBTITLES);
	film_changed (Film::SUBTITLE_OFFSET);
	film_changed (Film::SUBTITLE_SCALE);
	film_changed (Film::ENCRYPTED);
	film_changed (Film::DCI_METADATA);
	film_changed (Film::SIZE);
	film_changed (Film::LENGTH);
	film_changed (Film::CONTENT_AUDIO_STREAMS);
	film_changed (Film::SUBTITLE_STREAMS);
	film_changed (Film::FRAMES_PER_SECOND);
}

/** Updates the sensitivity of lots of widgets to a given value.
 *  @param s true to make sensitive, false to make insensitive.
 */
void
FilmEditor::set_things_sensitive (bool s)
{
	_generally_sensitive = s;
	
	_name->Enable (s);
	_use_dci_name->Enable (s);
	_edit_dci_button->Enable (s);
	_format->Enable (s);
	_content->Enable (s);
	_trust_content_header->Enable (s);
	_left_crop->Enable (s);
	_right_crop->Enable (s);
	_top_crop->Enable (s);
	_bottom_crop->Enable (s);
	_filters_button->Enable (s);
	_scaler->Enable (s);
	_audio_stream->Enable (s);
	_dcp_content_type->Enable (s);
	_dcp_trim_start->Enable (s);
	_dcp_trim_end->Enable (s);
	_dcp_ab->Enable (s);
	_encrypted->Enable (s);
	_audio_gain->Enable (s);
	_audio_gain_calculate_button->Enable (s);
	_audio_delay->Enable (s);
	_still_duration->Enable (s);

	setup_subtitle_control_sensitivity ();
	setup_audio_control_sensitivity ();
}

/** Called when the `Edit filters' button has been clicked */
void
FilmEditor::edit_filters_clicked (wxCommandEvent &)
{
	FilterDialog* d = new FilterDialog (this, _film->filters());
	d->ActiveChanged.connect (bind (&Film::set_filters, _film, _1));
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

	int const n = _scaler->GetSelection ();
	if (n >= 0) {
		_film->set_scaler (Scaler::from_index (n));
	}
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

	_notebook->InvalidateBestSize ();
	
	_film_sizer->Layout ();
	_film_sizer->SetSizeHints (_film_panel);
	_video_sizer->Layout ();
	_video_sizer->SetSizeHints (_video_panel);
	_audio_sizer->Layout ();
	_audio_sizer->SetSizeHints (_audio_panel);
	_subtitle_sizer->Layout ();
	_subtitle_sizer->SetSizeHints (_subtitle_panel);

	_notebook->Fit ();
	Fit ();
}

void
FilmEditor::still_duration_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_still_duration (_still_duration->GetValue ());
}

void
FilmEditor::dcp_trim_start_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_dcp_trim_start (_dcp_trim_start->GetValue ());
}

void
FilmEditor::dcp_trim_end_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_dcp_trim_end (_dcp_trim_end->GetValue ());
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

	_film_sizer->Layout ();
}

void
FilmEditor::with_subtitles_toggled (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_with_subtitles (_with_subtitles->GetValue ());
}

void
FilmEditor::setup_subtitle_control_sensitivity ()
{
	bool h = false;
	if (_generally_sensitive && _film) {
		h = !_film->subtitle_streams().empty();
	}
	
	_with_subtitles->Enable (h);

	bool j = false;
	if (_film) {
		j = _film->with_subtitles ();
	}
	
	_subtitle_stream->Enable (j);
	_subtitle_offset->Enable (j);
	_subtitle_scale->Enable (j);
}

void
FilmEditor::setup_audio_control_sensitivity ()
{
	_use_content_audio->Enable (_generally_sensitive);
	_use_external_audio->Enable (_generally_sensitive);
	
	bool const source = _generally_sensitive && _use_content_audio->GetValue();
	bool const external = _generally_sensitive && _use_external_audio->GetValue();

	_audio_stream->Enable (source);
	for (int i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
		_external_audio[i]->Enable (external);
	}
}

void
FilmEditor::use_dci_name_toggled (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_use_dci_name (_use_dci_name->GetValue ());
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

void
FilmEditor::setup_streams ()
{
	_audio_stream->Clear ();
	vector<shared_ptr<AudioStream> > a = _film->content_audio_streams ();
	for (vector<shared_ptr<AudioStream> >::iterator i = a.begin(); i != a.end(); ++i) {
		shared_ptr<FFmpegAudioStream> ffa = dynamic_pointer_cast<FFmpegAudioStream> (*i);
		assert (ffa);
		_audio_stream->Append (std_to_wx (ffa->name()), new wxStringClientData (std_to_wx (ffa->to_string ())));
	}
	
	if (_film->use_content_audio() && _film->audio_stream()) {
		checked_set (_audio_stream, _film->audio_stream()->to_string());
	}

	_subtitle_stream->Clear ();
	vector<shared_ptr<SubtitleStream> > s = _film->subtitle_streams ();
	for (vector<shared_ptr<SubtitleStream> >::iterator i = s.begin(); i != s.end(); ++i) {
		_subtitle_stream->Append (std_to_wx ((*i)->name()), new wxStringClientData (std_to_wx ((*i)->to_string ())));
	}
	if (_film->subtitle_stream()) {
		checked_set (_subtitle_stream, _film->subtitle_stream()->to_string());
	} else {
		_subtitle_stream->SetValue (wxT (""));
	}
}

void
FilmEditor::audio_stream_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_content_audio_stream (
		audio_stream_factory (
			string_client_data (_audio_stream->GetClientObject (_audio_stream->GetSelection ())),
			Film::state_version
			)
		);
}

void
FilmEditor::subtitle_stream_changed (wxCommandEvent &)
{
	if (!_film) {
		return;
	}

	_film->set_subtitle_stream (
		subtitle_stream_factory (
			string_client_data (_subtitle_stream->GetClientObject (_subtitle_stream->GetSelection ())),
			Film::state_version
			)
		);
}

void
FilmEditor::setup_audio_details ()
{
	if (!_film->audio_stream()) {
		_audio->SetLabel (wxT (""));
	} else {
		stringstream s;
		s << _film->audio_stream()->channels () << " channels, " << _film->audio_stream()->sample_rate() << "Hz";
		_audio->SetLabel (std_to_wx (s.str ()));
	}
}

void
FilmEditor::active_jobs_changed (bool a)
{
	set_things_sensitive (!a);
}

void
FilmEditor::use_audio_changed (wxCommandEvent &)
{
	_film->set_use_content_audio (_use_content_audio->GetValue());
}

void
FilmEditor::external_audio_changed (wxCommandEvent &)
{
	vector<string> a;
	for (int i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
		a.push_back (wx_to_std (_external_audio[i]->GetPath()));
	}

	_film->set_external_audio (a);
}
