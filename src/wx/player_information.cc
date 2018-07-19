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

using std::cout;
using std::string;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;

/* This should be even */
static int const dcp_lines = 6;

PlayerInformation::PlayerInformation (wxWindow* parent, FilmViewer* viewer)
	: wxPanel (parent)
	, _viewer (viewer)
	, _sizer (new wxBoxSizer (wxHORIZONTAL))
{
	wxFont title_font (*wxNORMAL_FONT);
	title_font.SetWeight (wxFONTWEIGHT_BOLD);

	_dcp = new wxStaticText*[dcp_lines];

	DCPOMATIC_ASSERT ((dcp_lines % 2) == 0);

	{
		wxSizer* s = new wxBoxSizer (wxVERTICAL);
		add_label_to_sizer(s, this, _("DCP"), false, 0)->SetFont(title_font);
		for (int i = 0; i < dcp_lines / 2; ++i) {
			_dcp[i] = add_label_to_sizer(s, this, wxT(""), false, 0);
		}
		_sizer->Add (s, 1, wxEXPAND | wxALL, 6);
	}

	{
		wxSizer* s = new wxBoxSizer (wxVERTICAL);
		add_label_to_sizer(s, this, wxT(" "), false, 0);
		for (int i = dcp_lines / 2; i < dcp_lines; ++i) {
			_dcp[i] = add_label_to_sizer(s, this, wxT(""), false, 0);
		}
		_sizer->Add (s, 1, wxEXPAND | wxALL, 6);
	}

	{
		wxSizer* s = new wxBoxSizer (wxVERTICAL);
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
	checked_set (_dropped, wxString::Format(_("Dropped frames: %d"), _viewer->dropped()));
}

void
PlayerInformation::triggered_update ()
{
	shared_ptr<DCPContent> dcp;
	if (_viewer->film()) {
		ContentList content = _viewer->film()->content();
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
	if (dcp->caption) {
		checked_set (_dcp[r++], _("Subtitles: yes"));
	} else {
		checked_set (_dcp[r++], _("Subtitles: no"));
	}

	optional<double> vfr;
	vfr = dcp->video_frame_rate ();
	DCPOMATIC_ASSERT (vfr);

	string const len = String::compose(
		wx_to_std(_("Length: %1 (%2 frames)")),
		time_to_hmsf(dcp->full_length(), lrint(*vfr)),
		dcp->full_length().frames_round(*vfr)
		);

	checked_set (_dcp[r++], std_to_wx(len));

	dcp::Size decode = dcp->video->size();
	optional<int> reduction = _viewer->dcp_decode_reduction();
	if (reduction) {
		decode.width /= pow(2, *reduction);
		decode.height /= pow(2, *reduction);
	}

	checked_set (_decode_resolution, wxString::Format(_("Decode resolution: %dx%d"), decode.width, decode.height));

	DCPOMATIC_ASSERT(r <= dcp_lines);

	_sizer->Layout ();
}
