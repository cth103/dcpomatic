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
	wxGridBagSizer* grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (grid, 0, wxALL, 8);

	int r = 0;

	_show = new wxButton (this, wxID_ANY, _("Show Audio..."));
	grid->Add (_show, wxGBPosition (r, 0));
	++r;

	add_label_to_grid_bag_sizer (grid, this, _("Audio Gain"), true, wxGBPosition (r, 0));
	_gain = new ContentSpinCtrl<AudioContent> (
		this,
		new wxSpinCtrl (this),
		AudioContentProperty::AUDIO_GAIN,
		boost::mem_fn (&AudioContent::audio_gain),
		boost::mem_fn (&AudioContent::set_audio_gain)
		);
	
	_gain->add (grid, wxGBPosition (r, 1));
	add_label_to_grid_bag_sizer (grid, this, _("dB"), false, wxGBPosition (r, 2));
	_gain_calculate_button = new wxButton (this, wxID_ANY, _("Calculate..."));
	grid->Add (_gain_calculate_button, wxGBPosition (r, 3));
	++r;

	add_label_to_grid_bag_sizer (grid, this, _("Audio Delay"), true, wxGBPosition (r, 0));
	_delay = new ContentSpinCtrl<AudioContent> (
		this,
		new wxSpinCtrl (this),
		AudioContentProperty::AUDIO_DELAY,
		boost::mem_fn (&AudioContent::audio_delay),
		boost::mem_fn (&AudioContent::set_audio_delay)
		);
	
	_delay->add (grid, wxGBPosition (r, 1));
	/// TRANSLATORS: this is an abbreviation for milliseconds, the unit of time
	add_label_to_grid_bag_sizer (grid, this, _("ms"), false, wxGBPosition (r, 2));
	++r;

	add_label_to_grid_bag_sizer (grid, this, _("Audio Stream"), true, wxGBPosition (r, 0));
	_stream = new wxChoice (this, wxID_ANY);
	grid->Add (_stream, wxGBPosition (r, 1));
	_description = add_label_to_grid_bag_sizer (grid, this, "", false, wxGBPosition (r, 3));
	++r;
	
	_mapping = new AudioMappingView (this);
	_sizer->Add (_mapping, 1, wxEXPAND | wxALL, 6);

	_gain->wrapped()->SetRange (-60, 60);
	_delay->wrapped()->SetRange (-1000, 1000);

	_stream->Bind                (wxEVT_COMMAND_CHOICE_SELECTED,  boost::bind (&AudioPanel::stream_changed, this));
	_show->Bind                  (wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&AudioPanel::show_clicked, this));
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
AudioPanel::film_content_changed (int property)
{
	AudioContentList ac = _editor->selected_audio_content ();
	shared_ptr<AudioContent> acs;
	shared_ptr<FFmpegContent> fcs;
	if (ac.size() == 1) {
		acs = ac.front ();
		fcs = dynamic_pointer_cast<FFmpegContent> (acs);
	}
	
	if (property == AudioContentProperty::AUDIO_MAPPING) {
		_mapping->set (acs ? acs->audio_mapping () : AudioMapping ());
		_sizer->Layout ();
	} else if (property == FFmpegContentProperty::AUDIO_STREAM) {
		setup_stream_description ();
		_mapping->set (acs ? acs->audio_mapping () : AudioMapping ());
		_sizer->Layout ();
	} else if (property == FFmpegContentProperty::AUDIO_STREAMS) {
		_stream->Clear ();
		if (fcs) {
			vector<shared_ptr<FFmpegAudioStream> > a = fcs->audio_streams ();
			for (vector<shared_ptr<FFmpegAudioStream> >::iterator i = a.begin(); i != a.end(); ++i) {
				_stream->Append (std_to_wx ((*i)->name), new wxStringClientData (std_to_wx (lexical_cast<string> ((*i)->id))));
			}
			
			if (fcs->audio_stream()) {
				checked_set (_stream, lexical_cast<string> (fcs->audio_stream()->id));
				setup_stream_description ();
			}
		}
	}
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
	
	_gain->wrapped()->SetValue (
		Config::instance()->sound_processor()->db_for_fader_change (
			d->wanted_fader (),
			d->actual_fader ()
			)
		);

	/* This appears to be necessary, as the change is not signalled,
	   I think.
	*/
	_gain->update_from_model ();
	
	d->Destroy ();
}

void
AudioPanel::show_clicked ()
{
	if (_audio_dialog) {
		_audio_dialog->Destroy ();
		_audio_dialog = 0;
	}

	AudioContentList ac = _editor->selected_audio_content ();
	if (ac.size() != 1) {
		return;
	}
	
	_audio_dialog = new AudioDialog (this);
	_audio_dialog->Show ();
	_audio_dialog->set_content (ac.front ());
}

void
AudioPanel::stream_changed ()
{
	FFmpegContentList fc = _editor->selected_ffmpeg_content ();
	if (fc.size() != 1) {
		return;
	}

	shared_ptr<FFmpegContent> fcs = fc.front ();
	
	if (_stream->GetSelection() == -1) {
		return;
	}
	
	vector<shared_ptr<FFmpegAudioStream> > a = fcs->audio_streams ();
	vector<shared_ptr<FFmpegAudioStream> >::iterator i = a.begin ();
	string const s = string_client_data (_stream->GetClientObject (_stream->GetSelection ()));
	while (i != a.end() && lexical_cast<string> ((*i)->id) != s) {
		++i;
	}

	if (i != a.end ()) {
		fcs->set_audio_stream (*i);
	}

	setup_stream_description ();
}

void
AudioPanel::setup_stream_description ()
{
	FFmpegContentList fc = _editor->selected_ffmpeg_content ();
	if (fc.size() != 1) {
		_description->SetLabel ("");
		return;
	}

	shared_ptr<FFmpegContent> fcs = fc.front ();

	if (!fcs->audio_stream ()) {
		_description->SetLabel (wxT (""));
	} else {
		wxString s;
		if (fcs->audio_channels() == 1) {
			s << _("1 channel");
		} else {
			s << fcs->audio_channels() << wxT (" ") << _("channels");
		}
		s << wxT (", ") << fcs->content_audio_frame_rate() << _("Hz");
		_description->SetLabel (s);
	}
}

void
AudioPanel::mapping_changed (AudioMapping m)
{
	AudioContentList c = _editor->selected_audio_content ();
	if (c.size() == 1) {
		c.front()->set_audio_mapping (m);
	}
}

void
AudioPanel::content_selection_changed ()
{
	AudioContentList sel = _editor->selected_audio_content ();

	if (_audio_dialog && sel.size() == 1) {
		_audio_dialog->set_content (sel.front ());
	}
	
	_gain->set_content (sel);
	_delay->set_content (sel);

	_show->Enable (sel.size() == 1);
	_stream->Enable (sel.size() == 1);
	_mapping->Enable (sel.size() == 1);

	film_content_changed (AudioContentProperty::AUDIO_MAPPING);
	film_content_changed (FFmpegContentProperty::AUDIO_STREAM);
	film_content_changed (FFmpegContentProperty::AUDIO_STREAMS);
}
