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

#include "config_move_dialog.h"
#include "wx_util.h"
#include <boost/filesystem.hpp>

ConfigMoveDialog::ConfigMoveDialog (wxWindow* parent, boost::filesystem::path new_file)
	: QuestionDialog (
		parent,
		_("Move configuration"),
		_("Use this file as new configuration"),
		_("Overwrite this file with current configuration")
		)
{
	wxString message = wxString::Format (
		_("The file %s already exists.  Do you want to use it as your new configuration or overwrite it with your current configuration?"),
		std_to_wx(new_file.string()).data()
		);

	_sizer->Add (new wxStaticText (this, wxID_ANY, message), 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	layout ();
}
