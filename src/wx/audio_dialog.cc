/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include <boost/filesystem.hpp>
#include "audio_dialog.h"
#include "audio_plot.h"
#include "audio_analysis.h"
#include "film.h"
#include "wx_util.h"

using boost::shared_ptr;
using boost::bind;
using boost::optional;

AudioDialog::AudioDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Audio"), wxDefaultPosition, wxSize (640, 512), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	, _plot (0)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxHORIZONTAL);

	_plot = new AudioPlot (this);
	sizer->Add (_plot, 1, wxALL, 12);

	wxFlexGridSizer* table = new wxFlexGridSizer (2, 6, 6);

	for (int i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
		_channel_checkbox[i] = new wxCheckBox (this, wxID_ANY, audio_channel_name (i));
		table->Add (_channel_checkbox[i], 1, wxEXPAND);
		table->AddSpacer (0);
		_channel_checkbox[i]->Connect (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler (AudioDialog::channel_clicked), 0, this);
	}

	table->AddSpacer (0);
	table->AddSpacer (0);

	wxString const types[] = {
		_("Peak"),
		_("RMS")
	};

	for (int i = 0; i < AudioPoint::COUNT; ++i) {
		_type_checkbox[i] = new wxCheckBox (this, wxID_ANY, types[i]);
		table->Add (_type_checkbox[i], 1, wxEXPAND);
		table->AddSpacer (0);
		_type_checkbox[i]->Connect (wxID_ANY, wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler (AudioDialog::type_clicked), 0, this);
	}

	_smoothing = new wxSlider (this, wxID_ANY, 1, 1, 128);
	_smoothing->Connect (wxID_ANY, wxEVT_SCROLL_THUMBTRACK, wxScrollEventHandler (AudioDialog::smoothing_changed), 0, this);
	table->Add (_smoothing, 1, wxEXPAND);
	table->AddSpacer (0);

	sizer->Add (table, 0, wxALL, 12);

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);
}

void
AudioDialog::set_film (boost::shared_ptr<Film> f)
{
	_film_changed_connection.disconnect ();
	_film_audio_analysis_finished_connection.disconnect ();
	
	_film = f;

	try_to_load_analysis ();
	setup_channels ();
	_plot->set_gain (_film->audio_gain ());

	_film_changed_connection = _film->Changed.connect (bind (&AudioDialog::film_changed, this, _1));
	_film_audio_analysis_finished_connection = _film->AudioAnalysisFinished.connect (bind (&AudioDialog::try_to_load_analysis, this));

	SetTitle (String::compose ("DVD-o-matic audio - %1", _film->name()));
}

void
AudioDialog::setup_channels ()
{
	if (!_film->audio_stream()) {
		return;
	}

	AudioMapping m (_film->audio_stream()->channels ());
	
	for (int i = 0; i < MAX_AUDIO_CHANNELS; ++i) {
		if (m.dcp_to_source(static_cast<libdcp::Channel>(i))) {
			_channel_checkbox[i]->Show ();
		} else {
			_channel_checkbox[i]->Hide ();
		}
	}
}	

void
AudioDialog::try_to_load_analysis ()
{
	shared_ptr<AudioAnalysis> a;

	if (boost::filesystem::exists (_film->audio_analysis_path())) {
		a.reset (new AudioAnalysis (_film->audio_analysis_path ()));
	} else {
		if (IsShown ()) {
			_film->analyse_audio ();
		}
	}
		
	_plot->set_analysis (a);

	AudioMapping m (_film->audio_stream()->channels ());
	optional<libdcp::Channel> c = m.source_to_dcp (0);
	if (c) {
		_channel_checkbox[c.get()]->SetValue (true);
		_plot->set_channel_visible (0, true);
	}

	for (int i = 0; i < AudioPoint::COUNT; ++i) {
		_type_checkbox[i]->SetValue (true);
		_plot->set_type_visible (i, true);
	}
}

void
AudioDialog::channel_clicked (wxCommandEvent& ev)
{
	int c = 0;
	while (c < MAX_AUDIO_CHANNELS && ev.GetEventObject() != _channel_checkbox[c]) {
		++c;
	}

	assert (c < MAX_AUDIO_CHANNELS);

	AudioMapping m (_film->audio_stream()->channels ());
	optional<int> s = m.dcp_to_source (static_cast<libdcp::Channel> (c));
	if (s) {
		_plot->set_channel_visible (s.get(), _channel_checkbox[c]->GetValue ());
	}
}

void
AudioDialog::film_changed (Film::Property p)
{
	switch (p) {
	case Film::AUDIO_GAIN:
		_plot->set_gain (_film->audio_gain ());
		break;
	case Film::CONTENT_AUDIO_STREAM:
	case Film::EXTERNAL_AUDIO:
	case Film::USE_CONTENT_AUDIO:
		setup_channels ();
		break;
	default:
		break;
	}
}

void
AudioDialog::type_clicked (wxCommandEvent& ev)
{
	int t = 0;
	while (t < AudioPoint::COUNT && ev.GetEventObject() != _type_checkbox[t]) {
		++t;
	}

	assert (t < AudioPoint::COUNT);

	_plot->set_type_visible (t, _type_checkbox[t]->GetValue ());
}

void
AudioDialog::smoothing_changed (wxScrollEvent &)
{
	_plot->set_smoothing (_smoothing->GetValue ());
}
