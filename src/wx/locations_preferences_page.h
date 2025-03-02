/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "preferences_page.h"


class wxDirPickerCtrl;


namespace dcpomatic {
namespace preferences {


class LocationsPage : public Page
{
public:
	LocationsPage(wxSize panel_size, int border);

	wxString GetName() const override;

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override;
#endif

private:
	void setup() override;
	void config_changed() override;
	void content_directory_changed();
	void playlist_directory_changed();
	void kdm_directory_changed();

	wxDirPickerCtrl* _content_directory;
	wxDirPickerCtrl* _playlist_directory;
	wxDirPickerCtrl* _kdm_directory;
};


}
}

