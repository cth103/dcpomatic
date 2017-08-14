/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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
#include "lib/video_content.h"
#include "lib/dcp_content.h"

using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;

PlayerInformation::PlayerInformation (wxWindow* parent, FilmViewer* viewer)
	: wxPanel (parent)
	, _viewer (viewer)
	, _sizer (new wxBoxSizer (wxHORIZONTAL))
{
	wxFont title_font (*wxNORMAL_FONT);
	title_font.SetWeight (wxFONTWEIGHT_BOLD);

	{
		wxSizer* s = new wxBoxSizer (wxVERTICAL);
		add_label_to_sizer(s, this, _("DCP"), false, 0)->SetFont(title_font);
		_cpl_name = add_label_to_sizer(s, this, wxT(""), false, 0);
		_size = add_label_to_sizer(s, this, wxT(""), false, 0);
		_length = add_label_to_sizer(s, this, wxT(""), false, 0);
		_sizer->Add (s, 1, wxEXPAND | wxALL, 6);
	}

	{
		wxSizer* s = new wxBoxSizer (wxVERTICAL);
		add_label_to_sizer(s, this, _("Performance"), false, 0)->SetFont(title_font);
		_dropped = add_label_to_sizer(s, this, wxT(""), false, 0);
		_sizer->Add (s, 1, wxEXPAND | wxALL, 6);
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
		checked_set (_cpl_name, _("No DCP loaded."));
		checked_set (_size, wxT(""));
		checked_set (_length, wxT(""));
		return;
	}

	DCPOMATIC_ASSERT (dcp->video);

	checked_set (_cpl_name, std_to_wx(dcp->name()));
	checked_set (_size, wxString::Format(_("Size: %dx%d"), dcp->video->size().width, dcp->video->size().height));

	optional<double> vfr;
	vfr = dcp->video_frame_rate ();
	DCPOMATIC_ASSERT (vfr);
	checked_set (
		_length,
		wxString::Format(
			_("Length: %s (%ld frames)"),
			std_to_wx(time_to_hmsf(dcp->full_length(), lrint(*vfr))).data(),
			dcp->full_length().frames_round(*vfr)
			)
		);
}
