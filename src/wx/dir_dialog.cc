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


#include "dir_dialog.h"
#include "wx_util.h"
#include "lib/config.h"
#include "lib/cross.h"
#include <dcp/warnings.h>
#include <boost/filesystem.hpp>
#include <vector>
LIBDCP_DISABLE_WARNINGS
#include <wx/dirdlg.h>
LIBDCP_ENABLE_WARNINGS


using std::vector;
using boost::optional;


DirDialog::DirDialog(
	wxWindow* parent,
	wxString title,
	long style,
	std::string initial_path_key,
	optional<boost::filesystem::path> override_path
	)
	: wxDirDialog(
		parent,
		title,
		std_to_wx(
			override_path.get_value_or(
				Config::instance()->initial_path(initial_path_key).get_value_or(home_directory())
				).string()),
		style
		)
	, _initial_path_key(initial_path_key)
{

}


boost::filesystem::path
DirDialog::path() const
{
	return wx_to_std(GetPath());
}


vector<boost::filesystem::path>
DirDialog::paths() const
{
	wxArrayString wx;
#if wxCHECK_VERSION(3, 1, 4)
	GetPaths(wx);
#else
	wx.Append(GetPath());
#endif

	vector<boost::filesystem::path> std;
	for (size_t i = 0; i < wx.GetCount(); ++i) {
		std.push_back(wx_to_std(wx[i]));
	}

	return std;
}


bool
DirDialog::show()
{
	/* Call the specific ShowModal so that other classes can inherit from this one and
	 * override ShowModal without unexpected effects.
	 */
	auto response = wxDirDialog::ShowModal();
	if (response != wxID_OK) {
		return false;
	}

#if wxCHECK_VERSION(3, 1, 4)
	auto initial = GetWindowStyle() & wxDD_MULTIPLE ? paths()[0] : path();
#else
	auto initial = path();
#endif
	Config::instance()->set_initial_path(_initial_path_key, initial.parent_path());
	return true;
}


