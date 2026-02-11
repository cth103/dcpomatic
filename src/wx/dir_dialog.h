/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <vector>


class DirDialog : public wxDirDialog
{
public:
	/** @param initial_path_key key used to find the directory seen on opening the dialog, if @ref override_path is empty.
	 *  @param override_path path to show on opening the dialog.
	 */
	DirDialog(
		wxWindow* parent,
		wxString title,
		long style,
		std::string initial_path_key,
		boost::optional<boost::filesystem::path> override_path = boost::optional<boost::filesystem::path>()
		);

	/** @return true if OK was clicked */
	bool show();

	boost::filesystem::path path() const;
	std::vector<boost::filesystem::path> paths() const;

private:
	std::string _initial_path_key;
};

