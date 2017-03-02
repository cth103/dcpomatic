/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

/** @class SystemFontDialog
 *  @brief A dialog box to select one of the "system" fonts on Windows.
 *
 *  This is necessary because wxFileDialog on Windows will not display
 *  the contents of `c:\Windows\Fonts`, so we need a different way to choose
 *  one of those fonts.
 */

#include <wx/wx.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <vector>

class wxListCtrl;

class SystemFontDialog : public wxDialog
{
public:
	SystemFontDialog (wxWindow* parent);

	boost::optional<boost::filesystem::path> get_font () const;

private:
	void setup_sensitivity ();

	wxListCtrl* _list;
	std::vector<boost::filesystem::path> _fonts;
};
