/*
    Copyright (C) 2017-2018 Carl Hetherington <cth@carlh.net>

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


#include "player_information.h"
#include "wx_util.h"
#include "film_viewer.h"
#include "lib/playlist.h"
#include "lib/compose.hpp"
#include "lib/video_content.h"
#include "lib/audio_content.h"
#include "lib/dcp_content.h"
#include "lib/film.h"


using std::cout;
using std::dynamic_pointer_cast;
using std::shared_ptr;
using std::string;
using std::weak_ptr;
using boost::optional;


/* This should be even */
static int const dcp_lines = 6;


PlayerInformation::PlayerInformation (wxWindow* parent, weak_ptr<FilmViewer> viewer)
	: wxPanel (parent)
	, _viewer (viewer)
	, _sizer (new wxBoxSizer (wxHORIZONTAL))
{
	wxFont title_font (*wxNORMAL_FONT);
	title_font.SetWeight (wxFONTWEIGHT_BOLD);

	_dcp = new wxStaticText*[dcp_lines];

	DCPOMATIC_ASSERT ((dcp_lines % 2) == 0);

	{
		auto s = new wxBoxSizer (wxVERTICAL);
		add_label_to_sizer(s, this, _("DCP"), false, 0)->SetFont(title_font);
		for (int i = 0; i < dcp_lines / 2; ++i) {
			_dcp[i] = add_label_to_sizer(s, this, wxT(""), false, 0);
		}
		_sizer->Add (s, 1, wxEXPAND | wxALL, 6);
	}

	{
		auto s = new wxBoxSizer (wxVERTICAL);
		add_label_to_sizer(s, this, wxT(" "), false, 0);
		for (int i = dcp_lines / 2; i < dcp_lines; ++i) {
			_dcp[i] = add_label_to_sizer(s, this, wxT(""), false, 0);
		}
		_sizer->Add (s, 1, wxEXPAND | wxALL, 6);
	}

	{
		_kdm_panel = new wxPanel(this, wxID_ANY);
		auto s = new wxBoxSizer(wxVERTICAL);
		add_label_to_sizer(s, _kdm_panel, _("KDM"), false, 0)->SetFont(title_font);
		auto g = new wxGridBagSizer(0, DCPOMATIC_SIZER_GAP);
		add_label_to_sizer(g, _kdm_panel, _("Valid from"), true, wxGBPosition(0, 0));
		_kdm_from = add_label_to_sizer(g, _kdm_panel, wxT(""), false, wxGBPosition(0, 1));
		add_label_to_sizer(g, _kdm_panel, _("Valid to"), true, wxGBPosition(1, 0));
		_kdm_to = add_label_to_sizer(g, _kdm_panel, wxT(""), false, wxGBPosition(1, 1));
		auto pad = new wxBoxSizer(wxVERTICAL);
		s->Add(g, 1, wxEXPAND | wxLEFT, DCPOMATIC_SIZER_GAP);
		_kdm_panel->SetSizer(s);
		_sizer->Add(_kdm_panel, 1, wxEXPAND | wxALL, 6);
	}

	{
		auto s = new wxBoxSizer (wxVERTICAL);
		add_label_to_sizer(s, this, _("Performance"), false, 0)->SetFont(title_font);
		_dropped = add_label_to_sizer(s, this, wxT(""), false, 0);
		_decode_resolution = add_label_to_sizer(s, this, wxT(""), false, 0);
		_sizer->Add (s, 2, wxEXPAND | wxALL, 6);
	}

	SetSizerAndFit (_sizer);

	triggered_update ();

	Bind (wxEVT_TIMER, boost::bind (&PlayerInformation::periodic_update, this));
	_timer.reset (new wxTimer (this));
	_timer->Start (500);
}


void
PlayerInformation::periodic_update ()
{
	auto fv = _viewer.lock ();
	if (fv) {
		auto s = wxString::Format(_("Dropped frames: %d"), fv->dropped() + fv->errored());
		if (fv->errored() == 1) {
			s += wxString::Format(_(" (%d error)"), fv->errored());
		} else if (fv->errored() > 1) {
			s += wxString::Format(_(" (%d errors)"), fv->errored());
		}
		checked_set (_dropped, s);
	}
}


void
PlayerInformation::triggered_update ()
{
	auto fv = _viewer.lock ();
	if (!fv) {
		return;
	}

	shared_ptr<DCPContent> dcp;
	if (fv->film()) {
		auto content = fv->film()->content();
		if (content.size() == 1) {
			dcp = dynamic_pointer_cast<DCPContent>(content.front());
		}
	}

	if (!dcp) {
		checked_set (_dcp[0], _("No DCP loaded."));
		for (int r = 1; r < dcp_lines; ++r) {
			checked_set (_dcp[r], wxT(""));
		}
		checked_set (_decode_resolution, wxT(""));
		_kdm_panel->Hide();
		return;
	}

	int r = 0;
	checked_set (_dcp[r++], std_to_wx(dcp->name()));

	if (dcp->needs_assets()) {
		checked_set (_dcp[r], _("Needs OV"));
		return;
	}

	if (dcp->needs_kdm()) {
		checked_set (_dcp[r], _("Needs KDM"));
		return;
	}

	DCPOMATIC_ASSERT (dcp->video);

	checked_set (_dcp[r++], wxString::Format(_("Size: %dx%d"), dcp->video->size().width, dcp->video->size().height));
	if (dcp->video_frame_rate()) {
		checked_set (_dcp[r++], wxString::Format(_("Frame rate: %d"), (int) lrint(*dcp->video_frame_rate())));
	}
	if (dcp->audio && !dcp->audio->streams().empty()) {
		checked_set (_dcp[r++], wxString::Format(_("Audio channels: %d"), dcp->audio->streams().front()->channels()));
	}
	if (!dcp->text.empty()) {
		checked_set (_dcp[r++], _("Subtitles: yes"));
	} else {
		checked_set (_dcp[r++], _("Subtitles: no"));
	}

	optional<double> vfr;
	vfr = dcp->video_frame_rate ();
	DCPOMATIC_ASSERT (vfr);

	auto const len = String::compose(
		wx_to_std(_("Length: %1 (%2 frames)")),
		time_to_hmsf(dcp->full_length(fv->film()), lrint(*vfr)),
		dcp->full_length(fv->film()).frames_round(*vfr)
		);

	checked_set (_dcp[r++], std_to_wx(len));

	auto decode = dcp->video->size();
	auto reduction = fv->dcp_decode_reduction();
	if (reduction) {
		decode.width /= pow(2, *reduction);
		decode.height /= pow(2, *reduction);
	}

	checked_set (_decode_resolution, wxString::Format(_("Decode resolution: %dx%d"), decode.width, decode.height));

	DCPOMATIC_ASSERT(r <= dcp_lines);

	if (dcp->encrypted() && dcp->kdm()) {
		_kdm_panel->Show();
		auto const kdm = *dcp->kdm();
		auto const before = kdm.not_valid_before();
		checked_set(_kdm_from, wxString::Format(_("%s %s"), std_to_wx(before.date()), std_to_wx(before.time_of_day(true, false))));
		auto const after = kdm.not_valid_after();
		checked_set(_kdm_to, wxString::Format(_("%s %s"), std_to_wx(after.date()), std_to_wx(after.time_of_day(true, false))));
	} else {
		_kdm_panel->Hide();
	}

	_sizer->Layout ();
}
