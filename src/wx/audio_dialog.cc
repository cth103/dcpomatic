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

#include "audio_dialog.h"
#include "audio_plot.h"
#include "audio_analysis.h"
#include "film.h"
#include "wx_util.h"

using boost::shared_ptr;

AudioDialog::AudioDialog (wxWindow* parent, boost::shared_ptr<Film> film)
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

	set_film (film);

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);

	_channel->Connect (wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler (AudioDialog::channel_changed), 0, this);
}

void
AudioDialog::set_film (boost::shared_ptr<Film> f)
{
	_film_connection.disconnect ();
	_film = f;
	
	shared_ptr<AudioAnalysis> a;

	try {
		a.reset (new AudioAnalysis (f->audio_analysis_path ()));
	} catch (...) {

	}
		
	_plot->set_analysis (a);

	_channel->Clear ();
	for (int i = 0; i < f->audio_stream()->channels(); ++i) {
		_channel->Append (audio_channel_name (i));
	}

	_channel->SetSelection (0);

	_film_connection = f->Changed.connect (bind (&AudioDialog::film_changed, this, _1));
}

void
AudioDialog::channel_changed (wxCommandEvent &)
{
	_plot->set_channel (_channel->GetSelection ());
}

void
AudioDialog::film_changed (Film::Property p)
{
	if (p == Film::AUDIO_GAIN) {
		_plot->set_gain (_film->audio_gain ());
	}
}
