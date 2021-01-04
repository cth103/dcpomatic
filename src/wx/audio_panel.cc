/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "audio_panel.h"
#include "audio_mapping_view.h"
#include "wx_util.h"
#include "gain_calculator_dialog.h"
#include "content_panel.h"
#include "audio_dialog.h"
#include "static_text.h"
#include "check_box.h"
#include "dcpomatic_button.h"
#include "lib/config.h"
#include "lib/ffmpeg_audio_stream.h"
#include "lib/ffmpeg_content.h"
#include "lib/cinema_sound_processor.h"
#include "lib/job_manager.h"
#include "lib/dcp_content.h"
#include "lib/audio_content.h"
#include <wx/spinctrl.h>
#include <iostream>

using std::vector;
using std::cout;
using std::string;
using std::list;
using std::pair;
using std::dynamic_pointer_cast;
using std::shared_ptr;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif

AudioPanel::AudioPanel (ContentPanel* p)
	: ContentSubPanel (p, _("Audio"))
	, _audio_dialog (0)
{
	_reference = new CheckBox (this, _("Use this DCP's audio as OV and make VF"));
	_reference_note = new StaticText (this, wxT(""));
	_reference_note->Wrap (200);
	wxFont font = _reference_note->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_reference_note->SetFont(font);

	_show = new Button (this, _("Show graph of audio levels..."));
	_peak = new StaticText (this, wxT (""));

	_gain_label = create_label (this, _("Gain"), true);
	_gain = new ContentSpinCtrlDouble<AudioContent> (
		this,
		new wxSpinCtrlDouble (this),
		AudioContentProperty::GAIN,
		&Content::audio,
		boost::mem_fn (&AudioContent::gain),
		boost::mem_fn (&AudioContent::set_gain)
		);

	_gain_db_label = create_label (this, _("dB"), false);
	_gain_calculate_button = new Button (this, _("Calculate..."));

	_delay_label = create_label (this, _("Delay"), true);
	_delay = new ContentSpinCtrl<AudioContent> (
		this,
		new wxSpinCtrl (this),
		AudioContentProperty::DELAY,
		&Content::audio,
		boost::mem_fn (&AudioContent::delay),
		boost::mem_fn (&AudioContent::set_delay)
		);

	/// TRANSLATORS: this is an abbreviation for milliseconds, the unit of time
	_delay_ms_label = create_label (this, _("ms"), false);

	_mapping = new AudioMappingView (this, _("Content"), _("content"), _("DCP"), _("DCP"));
	_sizer->Add (_mapping, 1, wxEXPAND | wxALL, 6);

	_description = new StaticText (this, wxT(" \n"), wxDefaultPosition, wxDefaultSize);
	_sizer->Add (_description, 0, wxALL, 12);
	_description->SetFont (font);

	_gain->wrapped()->SetRange (-60, 60);
	_gain->wrapped()->SetDigits (1);
	_gain->wrapped()->SetIncrement (0.5);
	_delay->wrapped()->SetRange (-1000, 1000);

	content_selection_changed ();
	film_changed (Film::AUDIO_CHANNELS);
	film_changed (Film::VIDEO_FRAME_RATE);
	film_changed (Film::REEL_TYPE);

	_reference->Bind             (wxEVT_CHECKBOX, boost::bind (&AudioPanel::reference_clicked, this));
	_show->Bind                  (wxEVT_BUTTON,   boost::bind (&AudioPanel::show_clicked, this));
	_gain_calculate_button->Bind (wxEVT_BUTTON,   boost::bind (&AudioPanel::gain_calculate_button_clicked, this));

	_mapping_connection = _mapping->Changed.connect (boost::bind (&AudioPanel::mapping_changed, this, _1));
	_active_jobs_connection = JobManager::instance()->ActiveJobsChanged.connect (boost::bind (&AudioPanel::active_jobs_changed, this, _1, _2));

	add_to_grid ();
}

void
AudioPanel::add_to_grid ()
{
	int r = 0;

	wxBoxSizer* reference_sizer = new wxBoxSizer (wxVERTICAL);
	reference_sizer->Add (_reference, 0);
	reference_sizer->Add (_reference_note, 0);
	_grid->Add (reference_sizer, wxGBPosition(r, 0), wxGBSpan(1, 4));
	++r;

	_grid->Add (_show, wxGBPosition (r, 0), wxGBSpan (1, 2));
	_grid->Add (_peak, wxGBPosition (r, 2), wxGBSpan (1, 2), wxALIGN_CENTER_VERTICAL);
	++r;

	add_label_to_sizer (_grid, _gain_label, true, wxGBPosition(r, 0));
	{
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (_gain->wrapped(), 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);
		s->Add (_gain_db_label, 0, wxALIGN_CENTER_VERTICAL);
		_grid->Add (s, wxGBPosition(r, 1));
	}

	_grid->Add (_gain_calculate_button, wxGBPosition(r, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	++r;

	add_label_to_sizer (_grid, _delay_label, true, wxGBPosition(r, 0));
	wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
	s->Add (_delay->wrapped(), 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT, 6);
	s->Add (_delay_ms_label, 0, wxALIGN_CENTER_VERTICAL);
	_grid->Add (s, wxGBPosition(r, 1));
	++r;
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
	if (!_parent->film()) {
		return;
	}

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
	case Film::INTEROP:
		setup_sensitivity ();
		break;
	default:
		break;
	}
}

void
AudioPanel::film_content_changed (int property)
{
	ContentList ac = _parent->selected_audio ();
	if (property == AudioContentProperty::STREAMS) {
		if (ac.size() == 1) {
			_mapping->set (ac.front()->audio->mapping());
			_mapping->set_input_channels (ac.front()->audio->channel_names ());

			vector<AudioMappingView::Group> groups;
			int c = 0;
			for (auto i: ac.front()->audio->streams()) {
				shared_ptr<const FFmpegAudioStream> f = dynamic_pointer_cast<const FFmpegAudioStream> (i);
				string name = "";
				if (f) {
					name = f->name;
					if (f->codec_name) {
						name += " (" + f->codec_name.get() + ")";
					}
				}
				groups.push_back (AudioMappingView::Group (c, c + i->channels() - 1, name));
				c += i->channels ();
			}
			_mapping->set_input_groups (groups);

		} else {
			_mapping->set (AudioMapping ());
		}
		setup_description ();
		setup_peak ();
		_sizer->Layout ();
	} else if (property == AudioContentProperty::GAIN) {
		setup_peak ();
	} else if (property == DCPContentProperty::REFERENCE_AUDIO) {
		if (ac.size() == 1) {
			shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent> (ac.front ());
			checked_set (_reference, dcp ? dcp->reference_audio () : false);
		} else {
			checked_set (_reference, false);
		}

		setup_sensitivity ();
	} else if (property == ContentProperty::VIDEO_FRAME_RATE) {
		setup_description ();
	}
}

void
AudioPanel::gain_calculate_button_clicked ()
{
	GainCalculatorDialog* d = new GainCalculatorDialog (this);
	int const r = d->ShowModal ();
	optional<float> c = d->db_change();

	if (r == wxID_CANCEL || !c) {
		d->Destroy ();
		return;
	}

	optional<float> old_peak_dB = peak ();
	double old_value = _gain->wrapped()->GetValue();
	_gain->wrapped()->SetValue(old_value + *c);

	/* This appears to be necessary, as the change is not signalled,
	   I think.
	*/
	_gain->view_changed ();

	optional<float> peak_dB = peak ();
	if (old_peak_dB && *old_peak_dB < -0.5 && peak_dB && *peak_dB > -0.5) {
		error_dialog (this, _("It is not possible to adjust the content's gain for this fader change as it would cause the DCP's audio to clip.  The gain has not been changed."));
		_gain->wrapped()->SetValue (old_value);
		_gain->view_changed ();
	}

	d->Destroy ();
}

void
AudioPanel::setup_description ()
{
	ContentList ac = _parent->selected_audio ();
	if (ac.size () != 1) {
		checked_set (_description, wxT (""));
		return;
	}

	checked_set (_description, ac.front()->audio->processing_description(_parent->film()));
}

void
AudioPanel::mapping_changed (AudioMapping m)
{
	ContentList c = _parent->selected_audio ();
	if (c.size() == 1) {
		c.front()->audio->set_mapping (m);
	}
}

void
AudioPanel::content_selection_changed ()
{
	ContentList sel = _parent->selected_audio ();

	_gain->set_content (sel);
	_delay->set_content (sel);

	film_content_changed (AudioContentProperty::STREAMS);
	film_content_changed (AudioContentProperty::GAIN);
	film_content_changed (DCPContentProperty::REFERENCE_AUDIO);

	setup_sensitivity ();
}

void
AudioPanel::setup_sensitivity ()
{
	ContentList sel = _parent->selected_audio ();

	shared_ptr<DCPContent> dcp;
	if (sel.size() == 1) {
		dcp = dynamic_pointer_cast<DCPContent> (sel.front ());
	}

	string why_not;
	bool const can_reference = dcp && dcp->can_reference_audio (_parent->film(), why_not);
	wxString cannot;
	if (why_not.empty()) {
		cannot = _("Cannot reference this DCP's audio.");
	} else {
		cannot = _("Cannot reference this DCP's audio: ") + std_to_wx(why_not);
	}
	setup_refer_button (_reference, _reference_note, dcp, can_reference, cannot);

	if (_reference->GetValue ()) {
		_gain->wrapped()->Enable (false);
		_gain_calculate_button->Enable (false);
		_show->Enable (true);
		_peak->Enable (false);
		_delay->wrapped()->Enable (false);
		_mapping->Enable (false);
		_description->Enable (false);
	} else {
		_gain->wrapped()->Enable (sel.size() == 1);
		_gain_calculate_button->Enable (sel.size() == 1);
		_show->Enable (sel.size() == 1);
		_peak->Enable (sel.size() == 1);
		_delay->wrapped()->Enable (sel.size() == 1);
		_mapping->Enable (sel.size() == 1);
		_description->Enable (sel.size() == 1);
	}
}

void
AudioPanel::show_clicked ()
{
	if (_audio_dialog) {
		_audio_dialog->Destroy ();
		_audio_dialog = 0;
	}

	ContentList ac = _parent->selected_audio ();
	if (ac.size() != 1) {
		return;
	}

	_audio_dialog = new AudioDialog (this, _parent->film(), _parent->film_viewer(), ac.front());
	_audio_dialog->Show ();
}

/** @return If there is one selected piece of audio content, return its peak value in dB (if known) */
optional<float>
AudioPanel::peak () const
{
	optional<float> peak_dB;

	ContentList sel = _parent->selected_audio ();
	if (sel.size() == 1) {
		shared_ptr<Playlist> playlist (new Playlist);
		playlist->add (_parent->film(), sel.front());
		try {
			shared_ptr<AudioAnalysis> analysis (new AudioAnalysis(_parent->film()->audio_analysis_path(playlist)));
			peak_dB = linear_to_db(analysis->overall_sample_peak().first.peak) + analysis->gain_correction(playlist);
		} catch (...) {

		}
	}

	return peak_dB;
}

void
AudioPanel::setup_peak ()
{
	ContentList sel = _parent->selected_audio ();

	optional<float> peak_dB = peak ();
	if (sel.size() != 1) {
		_peak->SetLabel (wxT(""));
	} else {
		peak_dB = peak ();
		if (peak_dB) {
			_peak->SetLabel (wxString::Format(_("Peak: %.2fdB"), *peak_dB));
		} else {
			_peak->SetLabel (_("Peak: unknown"));
		}
	}

	static wxColour normal = _peak->GetForegroundColour ();

	if (peak_dB && *peak_dB > -0.5) {
		_peak->SetForegroundColour (wxColour (255, 0, 0));
	} else if (peak_dB && *peak_dB > -3) {
		_peak->SetForegroundColour (wxColour (186, 120, 0));
	} else {
		_peak->SetForegroundColour (normal);
	}
}

void
AudioPanel::active_jobs_changed (optional<string> old_active, optional<string> new_active)
{
	if (old_active && *old_active == "analyse_audio") {
		setup_peak ();
		_mapping->Enable (true);
	} else if (new_active && *new_active == "analyse_audio") {
		_mapping->Enable (false);
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

void
AudioPanel::set_film (shared_ptr<Film>)
{
	/* We are changing film, so destroy any audio dialog for the old one */
	if (_audio_dialog) {
		_audio_dialog->Destroy ();
		_audio_dialog = 0;
	}
}
