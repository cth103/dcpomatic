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

#include "audio_panel.h"
#include "audio_mapping_view.h"
#include "wx_util.h"
#include "gain_calculator_dialog.h"
#include "content_panel.h"
#include "audio_dialog.h"
#include "lib/config.h"
#include "lib/ffmpeg_content.h"
#include "lib/cinema_sound_processor.h"
#include "lib/job_manager.h"
#include "lib/dcp_content.h"
#include <wx/spinctrl.h>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#include <iostream>

using std::vector;
using std::cout;
using std::string;
using std::list;
using std::pair;
using boost::dynamic_pointer_cast;
using boost::lexical_cast;
using boost::shared_ptr;
using boost::optional;

AudioPanel::AudioPanel (ContentPanel* p)
	: ContentSubPanel (p, _("Audio"))
	, _audio_dialog (0)
{
	wxGridBagSizer* grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (grid, 0, wxALL, 8);

	int r = 0;

	_reference = new wxCheckBox (this, wxID_ANY, _("Refer to existing DCP"));
	grid->Add (_reference, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	{
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_show = new wxButton (this, wxID_ANY, _("Show graph of audio levels..."));
		s->Add (_show);
		_peak = new wxStaticText (this, wxID_ANY, wxT (""));
		s->Add (_peak, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, DCPOMATIC_SIZER_X_GAP);
		grid->Add (s, wxGBPosition (r, 0), wxGBSpan (1, 2));
		++r;
	}

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

	_reference->Bind             (wxEVT_COMMAND_CHECKBOX_CLICKED, boost::bind (&AudioPanel::reference_clicked, this));
	_show->Bind                  (wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&AudioPanel::show_clicked, this));
	_gain_calculate_button->Bind (wxEVT_COMMAND_BUTTON_CLICKED,   boost::bind (&AudioPanel::gain_calculate_button_clicked, this));

	_mapping_connection = _mapping->Changed.connect (boost::bind (&AudioPanel::mapping_changed, this, _1));

	JobManager::instance()->ActiveJobsChanged.connect (boost::bind (&AudioPanel::active_jobs_changed, this, _1));
}

AudioPanel::~AudioPanel ()
{
	if (_audio_dialog) {
		_audio_dialog->Destroy ();
		_audio_dialog = 0;
	}
}

void
AudioPanel::film_changed (Film::Property property)
{
	switch (property) {
	case Film::AUDIO_CHANNELS:
	case Film::AUDIO_PROCESSOR:
		_mapping->set_output_channels (_parent->film()->audio_output_names ());
		setup_peak ();
		break;
	case Film::VIDEO_FRAME_RATE:
		setup_description ();
		break;
	case Film::REEL_TYPE:
		setup_sensitivity ();
	default:
		break;
	}
}

void
AudioPanel::film_content_changed (int property)
{
	AudioContentList ac = _parent->selected_audio ();
	if (property == AudioContentProperty::AUDIO_STREAMS) {
		if (ac.size() == 1) {
			_mapping->set (ac.front()->audio_mapping());
			_mapping->set_input_channels (ac.front()->audio_channel_names ());
		} else {
			_mapping->set (AudioMapping ());
		}
		setup_description ();
		setup_peak ();
		_sizer->Layout ();
	} else if (property == AudioContentProperty::AUDIO_GAIN) {
		setup_peak ();
	} else if (property == DCPContentProperty::REFERENCE_AUDIO) {
		if (ac.size() == 1) {
			shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent> (ac.front ());
			checked_set (_reference, dcp ? dcp->reference_audio () : false);
		} else {
			checked_set (_reference, false);
		}

		setup_sensitivity ();
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

	_gain->set_content (sel);
	_delay->set_content (sel);

	film_content_changed (AudioContentProperty::AUDIO_STREAMS);
	film_content_changed (DCPContentProperty::REFERENCE_AUDIO);

	setup_sensitivity ();
}

void
AudioPanel::setup_sensitivity ()
{
	AudioContentList sel = _parent->selected_audio ();

	shared_ptr<DCPContent> dcp;
	if (sel.size() == 1) {
		dcp = dynamic_pointer_cast<DCPContent> (sel.front ());
	}

	list<string> why_not;
	bool const can_reference = dcp && dcp->can_reference_audio (why_not);
	_reference->Enable (can_reference);

	wxString s;
	if (!can_reference) {
		s = _("Cannot reference this DCP.  ");
		BOOST_FOREACH (string i, why_not) {
			s += std_to_wx(i) + wxT("  ");
		}
	}
	_reference->SetToolTip (s);

	if (_reference->GetValue ()) {
		_gain->wrapped()->Enable (false);
		_gain_calculate_button->Enable (false);
		_peak->Enable (false);
		_delay->wrapped()->Enable (false);
		_mapping->Enable (false);
		_description->Enable (false);
	} else {
		_gain->wrapped()->Enable (true);
		_gain_calculate_button->Enable (sel.size() == 1);
		_peak->Enable (true);
		_delay->wrapped()->Enable (true);
		_mapping->Enable (sel.size() == 1);
		_description->Enable (true);
	}
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

	_audio_dialog = new AudioDialog (this, _parent->film (), ac.front ());
	_audio_dialog->Show ();
}

void
AudioPanel::setup_peak ()
{
	AudioContentList sel = _parent->selected_audio ();
	bool alert = false;

	if (sel.size() != 1) {
		_peak->SetLabel (wxT (""));
	} else {
		shared_ptr<Playlist> playlist (new Playlist);
		playlist->add (sel.front ());
		try {
			shared_ptr<AudioAnalysis> analysis (new AudioAnalysis (_parent->film()->audio_analysis_path (playlist)));
			if (analysis->peak ()) {
				float const peak_dB = 20 * log10 (analysis->peak().get()) + analysis->gain_correction (playlist);
				if (peak_dB > -3) {
					alert = true;
				}
				_peak->SetLabel (wxString::Format (_("Peak: %.2fdB"), peak_dB));
			} else {
				_peak->SetLabel (_("Peak: unknown"));
			}
		} catch (...) {
			_peak->SetLabel (_("Peak: unknown"));
		}
	}

	static wxColour normal = _peak->GetForegroundColour ();

	if (alert) {
		_peak->SetForegroundColour (wxColour (255, 0, 0));
	} else {
		_peak->SetForegroundColour (normal);
	}
}

void
AudioPanel::active_jobs_changed (optional<string> j)
{
	if (j && *j == "analyse_audio") {
		setup_peak ();
	}
}

void
AudioPanel::reference_clicked ()
{
	ContentList c = _parent->selected ();
	if (c.size() != 1) {
		return;
	}

	shared_ptr<DCPContent> d = dynamic_pointer_cast<DCPContent> (c.front ());
	if (!d) {
		return;
	}

	d->set_reference_audio (_reference->GetValue ());
}
