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

AudioDialog::AudioDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("Audio"), wxDefaultPosition, wxSize (640, 512), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	, _plot (0)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxHORIZONTAL);

	_plot = new AudioPlot (this);
	sizer->Add (_plot, 1);

	wxFlexGridSizer* table = new wxFlexGridSizer (2, 6, 6);

	add_label_to_sizer (table, this, _("Channel"));
	_channel = new wxChoice (this, wxID_ANY);
	table->Add (_channel, 1, wxEXPAND | wxALL, 6);

	sizer->Add (table);

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);

	_channel->Connect (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler (AudioDialog::channel_changed), 0, this);
}

void
AudioDialog::set_film (boost::shared_ptr<Film> f)
{
	_film_changed_connection.disconnect ();
	_film_audio_analysis_finished_connection.disconnect ();
	
	_film = f;

	try_to_load_analysis ();
	setup_channels ();

	_channel->SetSelection (0);

	_film_changed_connection = _film->Changed.connect (bind (&AudioDialog::film_changed, this, _1));
	_film_audio_analysis_finished_connection = _film->AudioAnalysisFinished.connect (bind (&AudioDialog::try_to_load_analysis, this));
}

void
AudioDialog::setup_channels ()
{
	_channel->Clear ();

	if (!_film->audio_stream()) {
		return;
	}
	
	for (int i = 0; i < _film->audio_stream()->channels(); ++i) {
		_channel->Append (audio_channel_name (i));
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
}

void
AudioDialog::channel_changed (wxCommandEvent &)
{
	_plot->set_channel (_channel->GetSelection ());
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
