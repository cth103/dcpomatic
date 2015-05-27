/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "lib/config.h"
#include "lib/ffmpeg_content.h"
#include "lib/cinema_sound_processor.h"
#include "audio_dialog.h"
#include "audio_panel.h"
#include "audio_mapping_view.h"
#include "wx_util.h"
#include "gain_calculator_dialog.h"
#include "content_panel.h"
#include <wx/spinctrl.h>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

using std::vector;
using std::cout;
using std::string;
using std::list;
using std::pair;
using boost::dynamic_pointer_cast;
using boost::lexical_cast;
using boost::shared_ptr;

AudioPanel::AudioPanel (ContentPanel* p)
	: ContentSubPanel (p, _("Audio"))
	, _audio_dialog (0)
{
	wxGridBagSizer* grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (grid, 0, wxALL, 8);

	int r = 0;

	_show = new wxButton (this, wxID_ANY, _("Show Audio..."));
	grid->Add (_show, wxGBPosition (r, 0));
	++r;

	add_label_to_grid_bag_sizer (grid, this, _("Gain"), true, wxGBPosition (r, 0));
	_gain = new ContentSpinCtrlDouble<AudioContent> (
		this,
		new wxSpinCtrlDouble (this),
		AudioContentProperty::AUDIO_GAIN,
		boost::mem_fn (&AudioContent::audio_gain),
		boost::mem_fn (&AudioContent::set_audio_gain)
		);
	
	_gain->add (grid, wxGBPosition (r, 1));
	add_label_to_grid_bag_sizer (grid, this, _("dB"), false, wxGBPosition (r, 2));
	_gain_calculate_button = new wxButton (this, wxID_ANY, _("Calculate..."));
	grid->Add (_gain_calculate_button, wxGBPosition (r, 3));
	++r;

	add_label_to_grid_bag_sizer (grid, this, _("Delay"), true, wxGBPosition (r, 0));
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

	_mapping = new AudioMappingView (this);
	_sizer->Add (_mapping, 1, wxEXPAND | wxALL, 6);
	++r;

	_description = new wxStaticText (this, wxID_ANY, wxT (" \n"), wxDefaultPosition, wxDefaultSize);
	_sizer->Add (_description, 0, wxALL, 12);
	wxFont font = _description->GetFont();
	font.SetStyle (wxFONTSTYLE_ITALIC);
	font.SetPointSize (font.GetPointSize() - 1);
	_description->SetFont (font);
	++r;

	_gain->wrapped()->SetRange (-60, 60);
	_gain->wrapped()->SetDigits (1);
	_gain->wrapped()->SetIncrement (0.5);
	_delay->wrapped()->SetRange (-1000, 1000);

	_show->Bind                  (wxEVT_COMMAND_BUTTON_CLICKED,  boost::bind (&AudioPanel::show_clicked, this));
	_gain_calculate_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED,  boost::bind (&AudioPanel::gain_calculate_button_clicked, this));

	_mapping_connection = _mapping->Changed.connect (boost::bind (&AudioPanel::mapping_changed, this, _1));
}


void
AudioPanel::film_changed (Film::Property property)
{
	switch (property) {
	case Film::AUDIO_CHANNELS:
		_mapping->set_channels (_parent->film()->audio_channels ());
		_sizer->Layout ();
		break;
	case Film::VIDEO_FRAME_RATE:
		setup_description ();
		break;
	default:
		break;
	}
}

void
AudioPanel::film_content_changed (int property)
{
	AudioContentList ac = _parent->selected_audio ();
	shared_ptr<AudioContent> acs;
	shared_ptr<FFmpegContent> fcs;
	if (ac.size() == 1) {
		acs = ac.front ();
		fcs = dynamic_pointer_cast<FFmpegContent> (acs);
	}
	
	if (property == AudioContentProperty::AUDIO_STREAMS) {
		_mapping->set (acs ? acs->audio_mapping () : AudioMapping ());
		_sizer->Layout ();
		setup_description ();
	}
}

void
AudioPanel::gain_calculate_button_clicked ()
{
	GainCalculatorDialog* d = new GainCalculatorDialog (this);
	int const r = d->ShowModal ();

	if (r == wxID_CANCEL || d->wanted_fader() == 0 || d->actual_fader() == 0) {
		d->Destroy ();
		return;
	}
	
	_gain->wrapped()->SetValue (
		Config::instance()->cinema_sound_processor()->db_for_fader_change (
			d->wanted_fader (),
			d->actual_fader ()
			)
		);

	/* This appears to be necessary, as the change is not signalled,
	   I think.
	*/
	_gain->view_changed ();
	
	d->Destroy ();
}

void
AudioPanel::show_clicked ()
{
	if (_audio_dialog) {
		_audio_dialog->Destroy ();
		_audio_dialog = 0;
	}

	AudioContentList ac = _parent->selected_audio ();
	if (ac.size() != 1) {
		return;
	}
	
	_audio_dialog = new AudioDialog (this, _parent->film ());
	_audio_dialog->Show ();
	_audio_dialog->set_content (ac.front ());
}

void
AudioPanel::setup_description ()
{
	AudioContentList ac = _parent->selected_audio ();
	if (ac.size () != 1) {
		checked_set (_description, wxT (""));
		return;
	}

	checked_set (_description, ac.front()->processing_description ());
}

void
AudioPanel::mapping_changed (AudioMapping m)
{
	AudioContentList c = _parent->selected_audio ();
	if (c.size() == 1) {
		c.front()->set_audio_mapping (m);
	}
}

void
AudioPanel::content_selection_changed ()
{
	AudioContentList sel = _parent->selected_audio ();

	if (_audio_dialog && sel.size() == 1) {
		_audio_dialog->set_content (sel.front ());
	}
	
	_gain->set_content (sel);
	_delay->set_content (sel);

	_gain_calculate_button->Enable (sel.size() == 1);
	_show->Enable (sel.size() == 1);
	_mapping->Enable (sel.size() == 1);

	film_content_changed (FFmpegContentProperty::AUDIO_STREAMS);
}
