/*
    Copyright (C) 2012-2013 Carl Hetherington <cth@carlh.net>

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

#include <boost/lexical_cast.hpp>
#include <wx/spinctrl.h>
#include "lib/config.h"
#include "lib/sound_processor.h"
#include "lib/ffmpeg_content.h"
#include "audio_dialog.h"
#include "audio_panel.h"
#include "audio_mapping_view.h"
#include "wx_util.h"
#include "gain_calculator_dialog.h"
#include "film_editor.h"

using std::vector;
using std::cout;
using std::string;
using boost::dynamic_pointer_cast;
using boost::lexical_cast;
using boost::shared_ptr;

AudioPanel::AudioPanel (FilmEditor* e)
	: FilmEditorPanel (e, _("Audio"))
	, _audio_dialog (0)
{
	wxFlexGridSizer* grid = new wxFlexGridSizer (3, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (grid, 0, wxALL, 8);

	_show = new wxButton (this, wxID_ANY, _("Show Audio..."));
	grid->Add (_show, 1);
	grid->AddSpacer (0);
	grid->AddSpacer (0);

	add_label_to_sizer (grid, this, _("Audio Gain"), true);
	{
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_gain = new wxSpinCtrl (this);
		s->Add (_gain, 1);
		add_label_to_sizer (s, this, _("dB"), false);
		grid->Add (s, 1);
	}
	
	_gain_calculate_button = new wxButton (this, wxID_ANY, _("Calculate..."));
	grid->Add (_gain_calculate_button);

	add_label_to_sizer (grid, this, _("Audio Delay"), false);
	{
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_delay = new wxSpinCtrl (this);
		s->Add (_delay, 1);
		/// TRANSLATORS: this is an abbreviation for milliseconds, the unit of time
		add_label_to_sizer (s, this, _("ms"), false);
		grid->Add (s);
	}

	grid->AddSpacer (0);

	add_label_to_sizer (grid, this, _("Audio Stream"), true);
	_stream = new wxChoice (this, wxID_ANY);
	grid->Add (_stream, 1, wxEXPAND);
	_description = new wxStaticText (this, wxID_ANY, wxT (""));
	grid->AddSpacer (0);
	
	grid->Add (_description, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
	grid->AddSpacer (0);
	grid->AddSpacer (0);
	
	_mapping = new AudioMappingView (this);
	_sizer->Add (_mapping, 1, wxEXPAND | wxALL, 6);

	_gain->SetRange (-60, 60);
	_delay->SetRange (-1000, 1000);

	_delay->Bind  		     (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&AudioPanel::delay_changed, this));
	_stream->Bind                (wxEVT_COMMAND_CHOICE_SELECTED,  boost::bind (&AudioPanel::stream_changed, this));
	_show->Bind                  (wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&AudioPanel::show_clicked, this));
	_gain->Bind                  (wxEVT_COMMAND_SPINCTRL_UPDATED, boost::bind (&AudioPanel::gain_changed, this));
	_gain_calculate_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&AudioPanel::gain_calculate_button_clicked, this));

	_mapping->Changed.connect (boost::bind (&AudioPanel::mapping_changed, this, _1));
}


void
AudioPanel::film_changed (Film::Property property)
{
	switch (property) {
	case Film::AUDIO_CHANNELS:
		_mapping->set_channels (_editor->film()->audio_channels ());
		_sizer->Layout ();
		break;
	default:
		break;
	}
}

void
AudioPanel::film_content_changed (shared_ptr<Content> c, int property)
{
	shared_ptr<AudioContent> ac = dynamic_pointer_cast<AudioContent> (c);
	shared_ptr<FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> (c);

	if (_audio_dialog && _editor->selected_audio_content()) {
		_audio_dialog->set_content (_editor->selected_audio_content ());
	}
	
	if (property == AudioContentProperty::AUDIO_GAIN) {
		checked_set (_gain, ac ? ac->audio_gain() : 0);
	} else if (property == AudioContentProperty::AUDIO_DELAY) {
		checked_set (_delay, ac ? ac->audio_delay() : 0);
	} else if (property == AudioContentProperty::AUDIO_MAPPING) {
		_mapping->set (ac ? ac->audio_mapping () : AudioMapping ());
		_sizer->Layout ();
	} else if (property == FFmpegContentProperty::AUDIO_STREAM) {
		setup_stream_description ();
		_mapping->set (ac ? ac->audio_mapping () : AudioMapping ());
	} else if (property == FFmpegContentProperty::AUDIO_STREAMS) {
		_stream->Clear ();
		if (fc) {
			vector<shared_ptr<FFmpegAudioStream> > a = fc->audio_streams ();
			for (vector<shared_ptr<FFmpegAudioStream> >::iterator i = a.begin(); i != a.end(); ++i) {
				_stream->Append (std_to_wx ((*i)->name), new wxStringClientData (std_to_wx (lexical_cast<string> ((*i)->id))));
			}
			
			if (fc->audio_stream()) {
				checked_set (_stream, lexical_cast<string> (fc->audio_stream()->id));
				setup_stream_description ();
			}
		}
	}
}

void
AudioPanel::gain_changed ()
{
	shared_ptr<AudioContent> ac = _editor->selected_audio_content ();
	if (!ac) {
		return;
	}

	ac->set_audio_gain (_gain->GetValue ());
}

void
AudioPanel::delay_changed ()
{
	shared_ptr<AudioContent> ac = _editor->selected_audio_content ();
	if (!ac) {
		return;
	}

	ac->set_audio_delay (_delay->GetValue ());
}

void
AudioPanel::gain_calculate_button_clicked ()
{
	GainCalculatorDialog* d = new GainCalculatorDialog (this);
	d->ShowModal ();

	if (d->wanted_fader() == 0 || d->actual_fader() == 0) {
		d->Destroy ();
		return;
	}
	
	_gain->SetValue (
		Config::instance()->sound_processor()->db_for_fader_change (
			d->wanted_fader (),
			d->actual_fader ()
			)
		);

	/* This appears to be necessary, as the change is not signalled,
	   I think.
	*/
	gain_changed ();
	
	d->Destroy ();
}

void
AudioPanel::show_clicked ()
{
	if (_audio_dialog) {
		_audio_dialog->Destroy ();
		_audio_dialog = 0;
	}

	shared_ptr<Content> c = _editor->selected_content ();
	if (!c) {
		return;
	}

	shared_ptr<AudioContent> ac = dynamic_pointer_cast<AudioContent> (c);
	if (!ac) {
		return;
	}
	
	_audio_dialog = new AudioDialog (this);
	_audio_dialog->Show ();
	_audio_dialog->set_content (ac);
}

void
AudioPanel::stream_changed ()
{
	shared_ptr<Content> c = _editor->selected_content ();
	if (!c) {
		return;
	}
	
	shared_ptr<FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> (c);
	if (!fc) {
		return;
	}

	if (_stream->GetSelection() == -1) {
		return;
	}
	
	vector<shared_ptr<FFmpegAudioStream> > a = fc->audio_streams ();
	vector<shared_ptr<FFmpegAudioStream> >::iterator i = a.begin ();
	string const s = string_client_data (_stream->GetClientObject (_stream->GetSelection ()));
	while (i != a.end() && lexical_cast<string> ((*i)->id) != s) {
		++i;
	}

	if (i != a.end ()) {
		fc->set_audio_stream (*i);
	}

	setup_stream_description ();
}

void
AudioPanel::setup_stream_description ()
{
	shared_ptr<Content> c = _editor->selected_content ();
	if (!c) {
		return;
	}
	
	shared_ptr<FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> (c);
	if (!fc) {
		return;
	}

	if (!fc->audio_stream ()) {
		_description->SetLabel (wxT (""));
	} else {
		wxString s;
		if (fc->audio_channels() == 1) {
			s << _("1 channel");
		} else {
			s << fc->audio_channels() << wxT (" ") << _("channels");
		}
		s << wxT (", ") << fc->content_audio_frame_rate() << _("Hz");
		_description->SetLabel (s);
	}
}

void
AudioPanel::mapping_changed (AudioMapping m)
{
	shared_ptr<AudioContent> c = _editor->selected_audio_content ();
	if (!c) {
		return;
	}

	c->set_audio_mapping (m);
}

