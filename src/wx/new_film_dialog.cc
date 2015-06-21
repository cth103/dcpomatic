/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <boost/filesystem.hpp>
#include <wx/stdpaths.h>
#include "lib/config.h"
#include "new_film_dialog.h"
#include "wx_util.h"
#ifdef DCPOMATIC_USE_OWN_DIR_PICKER
#include "dir_picker_ctrl.h"
#endif

using namespace std;
using namespace boost;

boost::optional<boost::filesystem::path> NewFilmDialog::_directory;

NewFilmDialog::NewFilmDialog (wxWindow* parent)
	: TableDialog (parent, _("New Film"), 2, true)
{
	add (_("Film name"), true);
	_name = add (new wxTextCtrl (this, wxID_ANY));

	add (_("Create in folder"), true);

#ifdef DCPOMATIC_USE_OWN_DIR_PICKER
	_folder = new DirPickerCtrl (this);
#else
	_folder = new wxDirPickerCtrl (this, wxID_ANY, wxEmptyString, wxDirSelectorPromptStr, wxDefaultPosition, wxSize (300, -1));
#endif

	if (!_directory) {
		_directory = Config::instance()->default_directory_or (wx_to_std (wxStandardPaths::Get().GetDocumentsDir()));
	}

	_folder->SetPath (std_to_wx (_directory.get().string()));
	add (_folder);

	_name->SetFocus ();

	layout ();
}

NewFilmDialog::~NewFilmDialog ()
{
	_directory = wx_to_std (_folder->GetPath ());
}

boost::filesystem::path
NewFilmDialog::get_path () const
{
	filesystem::path p;
	p /= wx_to_std (_folder->GetPath ());
	p /= wx_to_std (_name->GetValue ());
	return p;
}
