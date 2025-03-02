/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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


/** @file src/playlist_editor_config_dialog.cc
 *  @brief A dialogue to edit DCP-o-matic Playlist Editor configuration.
 */


#include "locations_preferences_page.h"
#include "playlist_editor_config_dialog.h"
#include "wx_variant.h"


using namespace dcpomatic;


wxPreferencesEditor*
create_playlist_editor_config_dialog ()
{
	auto e = new wxPreferencesEditor(variant::wx::insert_dcpomatic_playlist_editor(_("%s Preferences")));

#ifdef DCPOMATIC_OSX
	/* Width that we force some of the config panels to be on OSX so that
	   the containing window doesn't shrink too much when we select those panels.
	   This is obviously an unpleasant hack.
	*/
	auto ps = wxSize (520, -1);
	int const border = 16;
#else
	auto ps = wxSize (-1, -1);
	int const border = 8;
#endif

	e->AddPage(new preferences::LocationsPage(ps, border));
	return e;
}
