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
#include "lib/dcp_content.h"

using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

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
		_sizer->Add (s, 1, wxEXPAND | wxALL, 6);
	}

	{
		wxSizer* s = new wxBoxSizer (wxVERTICAL);
		add_label_to_sizer(s, this, _("Performance"), false, 0)->SetFont(title_font);
		_decoded_fps = add_label_to_sizer(s, this, wxT(""), false, 0);
		_sizer->Add (s, 1, wxEXPAND | wxALL, 6);
	}

	SetSizerAndFit (_sizer);

	update ();
}

void
PlayerInformation::update ()
{
	wxString cpl_name;
	if (_viewer->film()) {
		ContentList content = _viewer->film()->content();
		if (content.size() == 1) {
			shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent>(content.front());
			if (dcp) {
				cpl_name = std_to_wx (dcp->name());
			}
		}
	}

	checked_set (_cpl_name, cpl_name);
}
