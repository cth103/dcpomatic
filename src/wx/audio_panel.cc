/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "audio_dialog.h"
#include "audio_mapping_view.h"
#include "audio_panel.h"
#include "check_box.h"
#include "content_panel.h"
#include "dcpomatic_button.h"
#include "gain_calculator_dialog.h"
#include "static_text.h"
#include "wx_util.h"
#include "lib/audio_content.h"
#include "lib/cinema_sound_processor.h"
#include "lib/config.h"
#include "lib/dcp_content.h"
#include "lib/ffmpeg_audio_stream.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include "lib/maths_util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/spinctrl.h>
LIBDCP_ENABLE_WARNINGS


using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::pair;
using std::set;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


std::map<boost::filesystem::path, float> AudioPanel::_peak_cache;


AudioPanel::AudioPanel (ContentPanel* p)
	: ContentSubPanel (p, _("Audio"))
{

}


void
AudioPanel::create ()
{
	_show = new Button (this, _("Show graph of audio levels..."));
	_peak = new StaticText(this, {});

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

	_fade_in_label = create_label (this, _("Fade in"), true);
	_fade_in = new Timecode<ContentTime> (this);

	_fade_out_label = create_label (this, _("Fade out"), true);
	_fade_out = new Timecode<ContentTime> (this);

	_use_same_fades_as_video = new CheckBox(this, _("Use same fades as video"));

	_mapping = new AudioMappingView (this, _("Content"), _("content"), _("DCP"), _("DCP"));
	_sizer->Add (_mapping, 1, wxEXPAND | wxALL, 6);

	_description = new StaticText(this, char_to_wx(" \n"), wxDefaultPosition, wxDefaultSize);
	_sizer->Add (_description, 0, wxALL, 12);
	auto font = _description->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_description->SetFont (font);

	_gain->wrapped()->SetRange (-60, 60);
	_gain->wrapped()->SetDigits (1);
	_gain->wrapped()->SetIncrement (0.5);
	_delay->wrapped()->SetRange (-1000, 1000);

	content_selection_changed ();
	film_changed(FilmProperty::AUDIO_CHANNELS);
	film_changed(FilmProperty::VIDEO_FRAME_RATE);
	film_changed(FilmProperty::REEL_TYPE);

	_show->Bind                  (wxEVT_BUTTON,   boost::bind (&AudioPanel::show_clicked, this));
	_gain_calculate_button->Bind (wxEVT_BUTTON,   boost::bind (&AudioPanel::gain_calculate_button_clicked, this));

	_fade_in->Changed.connect (boost::bind(&AudioPanel::fade_in_changed, this));
	_fade_out->Changed.connect (boost::bind(&AudioPanel::fade_out_changed, this));
	_use_same_fades_as_video->bind(&AudioPanel::use_same_fades_as_video_changed, this);

	_mapping_connection = _mapping->Changed.connect (boost::bind (&AudioPanel::mapping_changed, this, _1));
	_active_jobs_connection = JobManager::instance()->ActiveJobsChanged.connect (boost::bind (&AudioPanel::active_jobs_changed, this, _1, _2));

	add_to_grid ();

	layout();
}


void
AudioPanel::add_to_grid ()
{
	int r = 0;

	_grid->Add (_show, wxGBPosition (r, 0), wxGBSpan (1, 2));
	_grid->Add (_peak, wxGBPosition (r, 2), wxGBSpan (1, 2), wxALIGN_CENTER_VERTICAL);
	++r;

	add_label_to_sizer (_grid, _gain_label, true, wxGBPosition(r, 0));
	{
		auto s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (_gain->wrapped(), 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, DCPOMATIC_SIZER_GAP);
		s->Add (_gain_db_label, 0, wxALIGN_CENTER_VERTICAL);
		_grid->Add (s, wxGBPosition(r, 1));
	}

	_grid->Add (_gain_calculate_button, wxGBPosition(r, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	++r;

	add_label_to_sizer (_grid, _delay_label, true, wxGBPosition(r, 0));
	auto s = new wxBoxSizer (wxHORIZONTAL);
	s->Add (_delay->wrapped(), 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, DCPOMATIC_SIZER_GAP);
	s->Add (_delay_ms_label, 0, wxALIGN_CENTER_VERTICAL);
	_grid->Add (s, wxGBPosition(r, 1));
	++r;

	add_label_to_sizer (_grid, _fade_in_label, true, wxGBPosition(r, 0));
	_grid->Add (_fade_in, wxGBPosition(r, 1), wxGBSpan(1, 3));
	++r;

	add_label_to_sizer (_grid, _fade_out_label, true, wxGBPosition(r, 0));
	_grid->Add (_fade_out, wxGBPosition(r, 1), wxGBSpan(1, 3));
	++r;

	_grid->Add (_use_same_fades_as_video, wxGBPosition(r, 0), wxGBSpan(1, 4));
	++r;
}


void
AudioPanel::film_changed (FilmProperty property)
{
	if (!_parent->film()) {
		return;
	}

	switch (property) {
	case FilmProperty::AUDIO_CHANNELS:
	case FilmProperty::AUDIO_PROCESSOR:
		_mapping->set_output_channels (_parent->film()->audio_output_names ());
		setup_peak ();
		setup_sensitivity();
		break;
	case FilmProperty::VIDEO_FRAME_RATE:
		setup_description ();
		break;
	case FilmProperty::REEL_TYPE:
	case FilmProperty::INTEROP:
		setup_sensitivity ();
		break;
	default:
		break;
	}
}


void
AudioPanel::film_content_changed (int property)
{
	auto ac = _parent->selected_audio ();
	if (property == AudioContentProperty::STREAMS) {
		if (ac.size() == 1) {
			_mapping->set (ac.front()->audio->mapping());
			_mapping->set_input_channels (ac.front()->audio->channel_names ());

			vector<AudioMappingView::Group> groups;
			int c = 0;
			for (auto i: ac.front()->audio->streams()) {
				auto f = dynamic_pointer_cast<const FFmpegAudioStream> (i);
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
		layout ();
	} else if (property == AudioContentProperty::GAIN) {
		/* This is a bit aggressive but probably not so bad */
		_peak_cache.clear();
		setup_peak ();
	} else if (property == ContentProperty::VIDEO_FRAME_RATE) {
		setup_description ();
	} else if (property == AudioContentProperty::FADE_IN) {
		set<Frame> check;
		for (auto i: ac) {
			check.insert (i->audio->fade_in().get());
		}

		if (check.size() == 1) {
			_fade_in->set (
				ac.front()->audio->fade_in(),
				ac.front()->active_video_frame_rate(_parent->film())
				);
		} else {
			_fade_in->clear ();
		}
	} else if (property == AudioContentProperty::FADE_OUT) {
		set<Frame> check;
		for (auto i: ac) {
			check.insert (i->audio->fade_out().get());
		}

		if (check.size() == 1) {
			_fade_out->set (
				ac.front()->audio->fade_out(),
				ac.front()->active_video_frame_rate(_parent->film())
				);
		} else {
			_fade_out->clear ();
		}
	} else if (property == AudioContentProperty::USE_SAME_FADES_AS_VIDEO) {
		set<bool> check;
		for (auto i: ac) {
			check.insert(i->audio->use_same_fades_as_video());
		}

		if (check.size() == 1) {
			_use_same_fades_as_video->set(ac.front()->audio->use_same_fades_as_video());
		} else {
			_use_same_fades_as_video->set(false);
		}
		setup_sensitivity ();
	}
}


void
AudioPanel::gain_calculate_button_clicked ()
{
	GainCalculatorDialog dialog(this);
	auto const r = dialog.ShowModal();
	auto change = dialog.db_change();

	if (r == wxID_CANCEL || !change) {
		return;
	}

	auto old_peak_dB = peak ();
	auto old_value = _gain->wrapped()->GetValue();
	_gain->wrapped()->SetValue(old_value + *change);

	/* This appears to be necessary, as the change is not signalled,
	   I think.
	*/
	_gain->view_changed ();

	auto peak_dB = peak ();
	if (old_peak_dB && *old_peak_dB < -0.5 && peak_dB && *peak_dB > -0.5) {
		error_dialog (this, _("It is not possible to adjust the content's gain for this fader change as it would cause the DCP's audio to clip.  The gain has not been changed."));
		_gain->wrapped()->SetValue (old_value);
		_gain->view_changed ();
	}
}


void
AudioPanel::setup_description ()
{
	auto ac = _parent->selected_audio ();
	if (ac.size () != 1) {
		checked_set(_description, wxString{});
		return;
	}

	checked_set (_description, ac.front()->audio->processing_description(_parent->film()));
}


void
AudioPanel::mapping_changed (AudioMapping m)
{
	auto c = _parent->selected_audio ();
	if (c.size() == 1) {
		c.front()->audio->set_mapping (m);
	}
}


void
AudioPanel::content_selection_changed ()
{
	auto sel = _parent->selected_audio ();

	_gain->set_content (sel);
	_delay->set_content (sel);

	film_content_changed (AudioContentProperty::STREAMS);
	film_content_changed (AudioContentProperty::GAIN);
	film_content_changed (AudioContentProperty::FADE_IN);
	film_content_changed (AudioContentProperty::FADE_OUT);
	film_content_changed (AudioContentProperty::USE_SAME_FADES_AS_VIDEO);
	film_content_changed (DCPContentProperty::REFERENCE_AUDIO);

	setup_sensitivity ();
}


void
AudioPanel::setup_sensitivity ()
{
	auto sel = _parent->selected_audio ();

	shared_ptr<DCPContent> dcp;
	if (sel.size() == 1) {
		dcp = dynamic_pointer_cast<DCPContent> (sel.front ());
	}

	auto const ref = dcp && dcp->reference_audio();
	auto const single = sel.size() == 1;
	auto const all_have_video = std::all_of(sel.begin(), sel.end(), [](shared_ptr<const Content> c) { return static_cast<bool>(c->video); });

	_gain->wrapped()->Enable (!ref);
	_gain_calculate_button->Enable (!ref && single);
	_show->Enable (single);
	_peak->Enable (!ref && single);
	_delay->wrapped()->Enable (!ref);
	_mapping->Enable (!ref && single);
	_description->Enable (!ref && single);
	_fade_in->Enable (!_use_same_fades_as_video->GetValue());
	_fade_out->Enable (!_use_same_fades_as_video->GetValue());
	_use_same_fades_as_video->Enable (!ref && all_have_video);
}


void
AudioPanel::show_clicked ()
{
	_audio_dialog.reset();

	auto ac = _parent->selected_audio ();
	if (ac.size() != 1) {
		return;
	}

	_audio_dialog.emplace(this, _parent->film(), _parent->film_viewer(), ac.front());
	_audio_dialog->Show ();
}


/** @return If there is one selected piece of audio content, return its peak value in dB (if known) */
optional<float>
AudioPanel::peak () const
{
	optional<float> peak_dB;

	auto sel = _parent->selected_audio ();
	if (sel.size() == 1) {
		auto playlist = make_shared<Playlist>();
		playlist->add (_parent->film(), sel.front());
		try {
			/* Loading the audio analysis file is slow, and this ::peak() is called a few times when
			 * the content selection is changed, so cache it.
			 */
			auto const path = _parent->film()->audio_analysis_path(playlist);
			auto cached = _peak_cache.find(path);
			if (cached != _peak_cache.end()) {
				peak_dB = cached->second;
			} else {
				auto analysis = make_shared<AudioAnalysis>(path);
				peak_dB = linear_to_db(analysis->overall_sample_peak().first.peak) + analysis->gain_correction(playlist);
				_peak_cache[path] = *peak_dB;
			}
		} catch (...) {

		}
	}

	return peak_dB;
}


void
AudioPanel::setup_peak ()
{
	auto sel = _parent->selected_audio ();

	auto peak_dB = peak ();
	if (sel.size() != 1) {
		_peak->SetLabel({});
	} else {
		peak_dB = peak ();
		if (peak_dB) {
			_peak->SetLabel (wxString::Format(_("Peak: %.2fdB"), *peak_dB));
		} else {
			_peak->SetLabel (_("Peak: unknown"));
		}
	}

	static auto normal = _peak->GetForegroundColour ();

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
AudioPanel::set_film (shared_ptr<Film>)
{
	/* We are changing film, so destroy any audio dialog for the old one */
	_audio_dialog.reset();
}


void
AudioPanel::fade_in_changed ()
{
	auto const hmsf = _fade_in->get();
	for (auto i: _parent->selected_audio()) {
		auto const vfr = i->active_video_frame_rate(_parent->film());
		i->audio->set_fade_in (dcpomatic::ContentTime(hmsf, vfr));
	}
}


void
AudioPanel::fade_out_changed ()
{
	auto const hmsf = _fade_out->get();
	for (auto i: _parent->selected_audio()) {
		auto const vfr = i->active_video_frame_rate (_parent->film());
		i->audio->set_fade_out (dcpomatic::ContentTime(hmsf, vfr));
	}
}


void
AudioPanel::use_same_fades_as_video_changed ()
{
	for (auto content: _parent->selected_audio()) {
		content->audio->set_use_same_fades_as_video(_use_same_fades_as_video->GetValue());
	}
}

