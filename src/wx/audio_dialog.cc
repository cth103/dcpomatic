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
#include "lib/audio_analysis.h"
#include "lib/film.h"
#include "lib/audio_content.h"
#include "audio_dialog.h"
#include "audio_plot.h"
#include "wx_util.h"

using boost::shared_ptr;
using boost::bind;
using boost::optional;

AudioDialog::AudioDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Audio"), wxDefaultPosition, wxSize (640, 512), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxFULL_REPAINT_ON_RESIZE)
	, _plot (0)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxHORIZONTAL);

	_plot = new AudioPlot (this);
	sizer->Add (_plot, 1, wxALL | wxEXPAND, 12);

	wxBoxSizer* side = new wxBoxSizer (wxVERTICAL);

	{
		wxStaticText* m = new wxStaticText (this, wxID_ANY, _("Channels"));
		side->Add (m, 1, wxALIGN_CENTER_VERTICAL | wxTOP, 16);
	}

	for (int i = 0; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		_channel_checkbox[i] = new wxCheckBox (this, wxID_ANY, std_to_wx (audio_channel_name (i)));
		side->Add (_channel_checkbox[i], 1, wxEXPAND | wxALL, 3);
		_channel_checkbox[i]->Bind (wxEVT_COMMAND_CHECKBOX_CLICKED, boost::bind (&AudioDialog::channel_clicked, this, _1));
	}

	{
		wxStaticText* m = new wxStaticText (this, wxID_ANY, _("Type"));
		side->Add (m, 1, wxALIGN_CENTER_VERTICAL | wxTOP, 16);
	}
	
	wxString const types[] = {
		_("Peak"),
		_("RMS")
	};

	for (int i = 0; i < AudioPoint::COUNT; ++i) {
		_type_checkbox[i] = new wxCheckBox (this, wxID_ANY, types[i]);
		side->Add (_type_checkbox[i], 1, wxEXPAND | wxALL, 3);
		_type_checkbox[i]->Bind (wxEVT_COMMAND_CHECKBOX_CLICKED, boost::bind (&AudioDialog::type_clicked, this, _1));
	}

	{
		wxStaticText* m = new wxStaticText (this, wxID_ANY, _("Smoothing"));
		side->Add (m, 1, wxALIGN_CENTER_VERTICAL | wxTOP, 16);
	}
	
	_smoothing = new wxSlider (this, wxID_ANY, AudioPlot::max_smoothing / 2, 1, AudioPlot::max_smoothing);
	_smoothing->Bind (wxEVT_SCROLL_THUMBTRACK, boost::bind (&AudioDialog::smoothing_changed, this));
	side->Add (_smoothing, 1, wxEXPAND);

	sizer->Add (side, 0, wxALL, 12);

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);
}

void
AudioDialog::set_content (shared_ptr<AudioContent> c)
{
	_content_changed_connection.disconnect ();

	_content = c;

	try_to_load_analysis ();
	_plot->set_gain (_content->audio_gain ());

	_content_changed_connection = _content->Changed.connect (bind (&AudioDialog::content_changed, this, _2));

	SetTitle (wxString::Format (_("DCP-o-matic audio - %s"), std_to_wx(_content->path_summary()).data()));
}

void
AudioDialog::try_to_load_analysis ()
{
	if (!IsShown ()) {
		return;
	}

	if (!boost::filesystem::exists (_content->audio_analysis_path())) {
		_plot->set_analysis (shared_ptr<AudioAnalysis> ());
		_analysis_finished_connection = _content->analyse_audio (bind (&AudioDialog::analysis_finished, this));
		return;
	}
	
	shared_ptr<AudioAnalysis> a;
	
	a.reset (new AudioAnalysis (_content->audio_analysis_path ()));
	_plot->set_analysis (a);

	/* Set up some defaults if no check boxes are checked */
	
	int i = 0;
	while (i < MAX_DCP_AUDIO_CHANNELS && (!_channel_checkbox[i] || !_channel_checkbox[i]->GetValue ())) {
		++i;
	}

	if (i == MAX_DCP_AUDIO_CHANNELS && _channel_checkbox[0]) {
		_channel_checkbox[0]->SetValue (true);
		_plot->set_channel_visible (0, true);
	}

	i = 0;
	while (i < AudioPoint::COUNT && !_type_checkbox[i]->GetValue ()) {
		i++;
	}

	if (i == AudioPoint::COUNT) {
		for (int i = 0; i < AudioPoint::COUNT; ++i) {
			_type_checkbox[i]->SetValue (true);
			_plot->set_type_visible (i, true);
		}
	}
}

void
AudioDialog::analysis_finished ()
{
	if (!boost::filesystem::exists (_content->audio_analysis_path())) {
		/* We analysed and still nothing showed up, so maybe it was cancelled or it failed.
		   Give up.
		*/
		_plot->set_message (_("Could not analyse audio."));
		return;
	}

	try_to_load_analysis ();
}

void
AudioDialog::channel_clicked (wxCommandEvent& ev)
{
	int c = 0;
	while (c < MAX_DCP_AUDIO_CHANNELS && ev.GetEventObject() != _channel_checkbox[c]) {
		++c;
	}

	DCPOMATIC_ASSERT (c < MAX_DCP_AUDIO_CHANNELS);

	_plot->set_channel_visible (c, _channel_checkbox[c]->GetValue ());
}

void
AudioDialog::content_changed (int p)
{
	if (p == AudioContentProperty::AUDIO_GAIN) {
		_plot->set_gain (_content->audio_gain ());
	} else if (p == AudioContentProperty::AUDIO_MAPPING) {
		try_to_load_analysis ();
	}
}

void
AudioDialog::type_clicked (wxCommandEvent& ev)
{
	int t = 0;
	while (t < AudioPoint::COUNT && ev.GetEventObject() != _type_checkbox[t]) {
		++t;
	}

	DCPOMATIC_ASSERT (t < AudioPoint::COUNT);

	_plot->set_type_visible (t, _type_checkbox[t]->GetValue ());
}

void
AudioDialog::smoothing_changed ()
{
	_plot->set_smoothing (_smoothing->GetValue ());
}
