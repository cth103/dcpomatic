/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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
#include "audio_plot.h"
#include "check_box.h"
#include "static_text.h"
#include "wx_util.h"
#include "lib/analyse_audio_job.h"
#include "lib/audio_analysis.h"
#include "lib/audio_content.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include "lib/maths_util.h"
#include <libxml++/libxml++.h>
#include <boost/filesystem.hpp>
#include <iostream>


using std::cout;
using std::list;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::vector;
using std::weak_ptr;
using boost::bind;
using boost::optional;
using boost::const_pointer_cast;
using std::dynamic_pointer_cast;
using namespace dcpomatic;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


/** @param parent Parent window.
 *  @param film Film we are using.
 *  @param content Content to analyse, or 0 to analyse all of the film's audio.
 */
AudioDialog::AudioDialog (wxWindow* parent, shared_ptr<Film> film, FilmViewer& viewer, shared_ptr<Content> content)
	: wxDialog (
		parent,
		wxID_ANY,
		_("Audio"),
		wxDefaultPosition,
		wxSize (640, 512),
#ifdef DCPOMATIC_OSX
		/* I can't get wxFRAME_FLOAT_ON_PARENT to work on OS X, and although wxSTAY_ON_TOP keeps
		   the window above all others (and not just our own) it's better than nothing for now.
		*/
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxFULL_REPAINT_ON_RESIZE | wxSTAY_ON_TOP
#else
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxFULL_REPAINT_ON_RESIZE | wxFRAME_FLOAT_ON_PARENT
#endif
		)
	, _film (film)
	, _content (content)
	, _channels (film->audio_channels ())
	, _plot (nullptr)
{
	wxFont subheading_font (*wxNORMAL_FONT);
	subheading_font.SetWeight (wxFONTWEIGHT_BOLD);

	auto overall_sizer = new wxBoxSizer (wxVERTICAL);
	auto lr_sizer = new wxBoxSizer (wxHORIZONTAL);

	auto left = new wxBoxSizer (wxVERTICAL);

	_cursor = new StaticText (this, wxT("Cursor: none"));
	left->Add (_cursor, 0, wxTOP, DCPOMATIC_SIZER_Y_GAP);
	_plot = new AudioPlot (this, viewer);
	left->Add (_plot, 1, wxTOP | wxEXPAND, 12);
	_sample_peak = new StaticText (this, wxT (""));
	left->Add (_sample_peak, 0, wxTOP, DCPOMATIC_SIZER_Y_GAP);
	_true_peak = new StaticText (this, wxT (""));
	left->Add (_true_peak, 0, wxTOP, DCPOMATIC_SIZER_Y_GAP);
	_integrated_loudness = new StaticText (this, wxT (""));
	left->Add (_integrated_loudness, 0, wxTOP, DCPOMATIC_SIZER_Y_GAP);
	_loudness_range = new StaticText (this, wxT (""));
	left->Add (_loudness_range, 0, wxTOP, DCPOMATIC_SIZER_Y_GAP);
	_leqm = new StaticText (this, wxT(""));
	left->Add (_leqm, 0, wxTOP, DCPOMATIC_SIZER_Y_GAP);

	lr_sizer->Add (left, 1, wxALL | wxEXPAND, 12);

	auto right = new wxBoxSizer (wxVERTICAL);

	{
		auto m = new StaticText (this, _("Channels"));
		m->SetFont (subheading_font);
		right->Add (m, 1, wxTOP | wxBOTTOM, 16);
	}

	for (int i = 0; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		_channel_checkbox[i] = new CheckBox (this, std_to_wx(audio_channel_name(i)));
		_channel_checkbox[i]->SetForegroundColour(wxColour(_plot->colour(i)));
		right->Add (_channel_checkbox[i], 0, wxEXPAND | wxALL, 3);
		_channel_checkbox[i]->bind(&AudioDialog::channel_clicked, this, _1);
	}

	show_or_hide_channel_checkboxes ();

	{
		auto m = new StaticText (this, _("Type"));
		m->SetFont (subheading_font);
		right->Add (m, 1, wxTOP, 16);
	}

	wxString const types[] = {
		_("Peak"),
		_("RMS")
	};

	for (int i = 0; i < AudioPoint::COUNT; ++i) {
		_type_checkbox[i] = new CheckBox (this, types[i]);
		right->Add (_type_checkbox[i], 0, wxEXPAND | wxALL, 3);
		_type_checkbox[i]->bind(&AudioDialog::type_clicked, this, _1);
	}

	{
		auto m = new StaticText (this, _("Smoothing"));
		m->SetFont (subheading_font);
		right->Add (m, 1, wxTOP, 16);
	}

	_smoothing = new wxSlider (this, wxID_ANY, AudioPlot::max_smoothing / 2, 1, AudioPlot::max_smoothing);
	_smoothing->Bind (wxEVT_SCROLL_THUMBTRACK, boost::bind (&AudioDialog::smoothing_changed, this));
	right->Add (_smoothing, 0, wxEXPAND);

	lr_sizer->Add (right, 0, wxALL, 12);

	overall_sizer->Add (lr_sizer, 0, wxEXPAND);

#ifdef DCPOMATIC_LINUX
	auto buttons = CreateSeparatedButtonSizer (wxCLOSE);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}
#endif

	SetSizer (overall_sizer);
	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);

	_film_connection = film->Change.connect (boost::bind(&AudioDialog::film_change, this, _1, _2));
	_film_content_connection = film->ContentChange.connect(boost::bind(&AudioDialog::content_change, this, _1, _3));
	DCPOMATIC_ASSERT (film->directory());
	if (content) {
		SetTitle(wxString::Format(_("DCP-o-matic audio - %s"), std_to_wx(content->path(0).string())));
	} else {
		SetTitle(wxString::Format(_("DCP-o-matic audio - %s"), std_to_wx(film->directory().get().string())));
	}

	if (content) {
		_playlist = make_shared<Playlist>();
		const_pointer_cast<Playlist>(_playlist)->add(film, content);
	} else {
		_playlist = film->playlist ();
	}

	_plot->Cursor.connect (bind (&AudioDialog::set_cursor, this, _1, _2));
}


void
AudioDialog::show_or_hide_channel_checkboxes ()
{
	for (int i = 0; i < _channels; ++i) {
		_channel_checkbox[i]->Show ();
	}

	for (int i = _channels; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		_channel_checkbox[i]->Hide ();
	}
}


void
AudioDialog::try_to_load_analysis ()
{
	if (!IsShown ()) {
		return;
	}

	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	auto check = _content.lock();

	auto const path = film->audio_analysis_path (_playlist);
	if (!boost::filesystem::exists (path)) {
		_plot->set_analysis (shared_ptr<AudioAnalysis> ());
		_analysis.reset ();

		for (auto i: JobManager::instance()->get()) {
			if (dynamic_pointer_cast<AnalyseAudioJob>(i)) {
				i->cancel ();
			}
		}

		JobManager::instance()->analyse_audio (
			film, _playlist, !static_cast<bool>(check), _analysis_finished_connection, bind (&AudioDialog::analysis_finished, this)
			);
		return;
	}

	try {
		_analysis.reset (new AudioAnalysis (path));
	} catch (OldFormatError& e) {
		/* An old analysis file: recreate it */
		JobManager::instance()->analyse_audio (
			film, _playlist, !static_cast<bool>(check), _analysis_finished_connection, bind (&AudioDialog::analysis_finished, this)
			);
		return;
	} catch (xmlpp::exception& e) {
		/* Probably a (very) old-style analysis file: recreate it */
		JobManager::instance()->analyse_audio (
			film, _playlist, !static_cast<bool>(check), _analysis_finished_connection, bind (&AudioDialog::analysis_finished, this)
			);
		return;
        }

	_plot->set_analysis (_analysis);
	_plot->set_gain_correction (_analysis->gain_correction (_playlist));
	setup_statistics ();
	show_or_hide_channel_checkboxes ();

	/* Set up some defaults if no check boxes are checked */

	int i = 0;
	while (i < _channels && (!_channel_checkbox[i] || !_channel_checkbox[i]->GetValue ())) {
		++i;
	}

	if (i == _channels) {
		/* Nothing checked; check mapped ones */

		list<int> mapped;
		auto content = _content.lock ();

		if (content) {
			mapped = content->audio->mapping().mapped_output_channels ();
		} else {
			mapped = film->mapped_audio_channels ();
		}

		for (auto i: mapped) {
			if (_channel_checkbox[i]) {
				_channel_checkbox[i]->SetValue (true);
				_plot->set_channel_visible (i, true);
			}
		}
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

	Refresh ();
}

void
AudioDialog::analysis_finished ()
{
	auto film = _film.lock ();
	if (!film) {
		/* This should not happen, but if it does we should just give up quietly */
		return;
	}

	if (!boost::filesystem::exists (film->audio_analysis_path (_playlist))) {
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
	while (c < _channels && ev.GetEventObject() != _channel_checkbox[c]) {
		++c;
	}

	DCPOMATIC_ASSERT (c < _channels);

	_plot->set_channel_visible (c, _channel_checkbox[c]->GetValue ());
}

void
AudioDialog::film_change(ChangeType type, FilmProperty p)
{
	if (type != ChangeType::DONE) {
		return;
	}

	if (p == FilmProperty::AUDIO_CHANNELS) {
		auto film = _film.lock ();
		if (film) {
			_channels = film->audio_channels ();
			try_to_load_analysis ();
		}
	}
}

void
AudioDialog::content_change (ChangeType type, int p)
{
	if (type != ChangeType::DONE) {
		return;
	}

	if (p == AudioContentProperty::STREAMS) {
		try_to_load_analysis ();
	} else if (p == AudioContentProperty::GAIN) {
		if (_playlist->content().size() == 1 && _analysis) {
			/* We can use a short-cut to render the effect of this
			   change, rather than recalculating everything.
			*/
			_plot->set_gain_correction (_analysis->gain_correction (_playlist));
			setup_statistics ();
		} else {
			try_to_load_analysis ();
		}
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

void
AudioDialog::setup_statistics ()
{
	if (!_analysis) {
		return;
	}

	auto film = _film.lock ();
	if (!film) {
		return;
	}

	auto const peak = _analysis->overall_sample_peak ();
	float const peak_dB = linear_to_db(peak.first.peak) + _analysis->gain_correction(_playlist);
	_sample_peak->SetLabel (
		wxString::Format (
			_("Sample peak is %.2fdB at %s on %s"),
			peak_dB,
			time_to_timecode (peak.first.time, film->video_frame_rate ()).data (),
			std_to_wx (short_audio_channel_name (peak.second)).data ()
			)
		);

	wxColour const peaking = *wxRED;
	wxColour const not_peaking = gui_is_dark() ? *wxWHITE : *wxBLACK;

	if (peak_dB > -3) {
		_sample_peak->SetForegroundColour(peaking);
	} else {
		_sample_peak->SetForegroundColour(not_peaking);
	}

	if (_analysis->overall_true_peak()) {
		float const peak = _analysis->overall_true_peak().get();
		float const peak_dB = linear_to_db(peak) + _analysis->gain_correction(_playlist);

		_true_peak->SetLabel (wxString::Format (_("True peak is %.2fdB"), peak_dB));

		if (peak_dB > -3) {
			_true_peak->SetForegroundColour(peaking);
		} else {
			_true_peak->SetForegroundColour(not_peaking);
		}
	}

	/* XXX: check whether it's ok to add dB gain to these quantities */

	if (static_cast<bool>(_analysis->integrated_loudness())) {
		_integrated_loudness->SetLabel (
			wxString::Format (
				_("Integrated loudness %.2f LUFS"),
				_analysis->integrated_loudness().get() + _analysis->gain_correction (_playlist)
				)
			);
	}

	if (static_cast<bool>(_analysis->loudness_range())) {
		_loudness_range->SetLabel (
			wxString::Format (
				_("Loudness range %.2f LU"),
				_analysis->loudness_range().get() + _analysis->gain_correction (_playlist)
				)
			);
	}

	if (static_cast<bool>(_analysis->leqm())) {
		_leqm->SetLabel(
			wxString::Format(
				_("LEQ(m) %.2fdB"), _analysis->leqm().get() + _analysis->gain_correction(_playlist)
				)
			);
	}
}

bool
AudioDialog::Show (bool show)
{
	bool const r = wxDialog::Show (show);
	try_to_load_analysis ();
	return r;
}

void
AudioDialog::set_cursor (optional<DCPTime> time, optional<float> db)
{
	if (!time || !db) {
		_cursor->SetLabel (_("Cursor: none"));
		return;
	}

	auto film = _film.lock();
	DCPOMATIC_ASSERT (film);
	_cursor->SetLabel (wxString::Format (_("Cursor: %.1fdB at %s"), *db, time->timecode(film->video_frame_rate())));
}
