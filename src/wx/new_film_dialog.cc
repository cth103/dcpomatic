/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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
#ifdef DVDOMATIC_USE_OWN_DIR_PICKER
#include "dir_picker_ctrl.h"
#endif

using namespace std;
using namespace boost;

boost::optional<string> NewFilmDialog::_directory;

NewFilmDialog::NewFilmDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("New Film"))
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (overall_sizer);
	
	wxFlexGridSizer* table = new wxFlexGridSizer (2, DVDOMATIC_SIZER_X_GAP, DVDOMATIC_SIZER_Y_GAP);
	table->AddGrowableCol (1, 1);
	overall_sizer->Add (table, 1, wxEXPAND | wxALL, 6);

	add_label_to_sizer (table, this, _("Film name"));
	_name = new wxTextCtrl (this, wxID_ANY);
	table->Add (_name, 0, wxEXPAND);

	add_label_to_sizer (table, this, _("Create in folder"));

#ifdef DVDOMATIC_USE_OWN_DIR_PICKER
	_folder = new DirPickerCtrl (this); 
#else	
	_folder = new wxDirPickerCtrl (this, wxDD_DIR_MUST_EXIST);
#endif

	if (!_directory) {
		_directory = Config::instance()->default_directory_or (wx_to_std (wxStandardPaths::Get().GetDocumentsDir()));
	}
	
	_folder->SetPath (std_to_wx (_directory.get()));
	table->Add (_folder, 1, wxEXPAND);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);
}

NewFilmDialog::~NewFilmDialog ()
{
	_directory = wx_to_std (_folder->GetPath ());
}

string
NewFilmDialog::get_path () const
{
	filesystem::path p;
	p /= wx_to_std (_folder->GetPath ());
	p /= wx_to_std (_name->GetValue ());
	return p.string ();
}
