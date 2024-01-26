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


class FileDialog : public wxFileDialog
{
public:
	/** @param initial_path_key key in config to use to store and read the initial path
	 *  @param override_path if not-none, this is used as the initial path regardless of the initial_path_key
	 */
	FileDialog(
		wxWindow* parent,
		wxString title,
		wxString allowed,
		long style,
		std::string initial_path_key,
		boost::optional<std::string> initial_filename = boost::optional<std::string>(),
		boost::optional<boost::filesystem::path> override_path = boost::optional<boost::filesystem::path>()
		);

	/** @return true if OK was clicked */
	bool show();

	boost::filesystem::path path() const;
	std::vector<boost::filesystem::path> paths() const;

private:
	std::string _initial_path_key;
	bool _multiple;
};

