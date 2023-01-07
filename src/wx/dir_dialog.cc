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
#include <boost/filesystem.hpp>
#include <vector>


using std::vector;


DirDialog::DirDialog(
	wxWindow* parent,
	wxString title,
	long style,
	std::string initial_path_key
	)
	: wxDirDialog(
		parent,
		title,
		std_to_wx(Config::instance()->initial_path(initial_path_key).get_value_or(home_directory()).string()),
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

	Config::instance()->set_initial_path(_initial_path_key, path().parent_path());
	return true;
}


