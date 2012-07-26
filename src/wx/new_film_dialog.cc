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
#include "new_film_dialog.h"
#include "wx_util.h"

using namespace std;
using namespace boost;

NewFilmDialog::NewFilmDialog (wxWindow* parent)
	: wxDialog (parent, wxID_ANY, _("New Film"))
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	wxFlexGridSizer* table = new wxFlexGridSizer (2, 6, 6);
	table->AddGrowableCol (1, 1);
	overall_sizer->Add (table, 1, wxEXPAND | wxALL, 6);

	add_label_to_sizer (table, this, "Film name");
	_name = new wxTextCtrl (this, wxID_ANY);
	table->Add (_name, 1, wxEXPAND);

	add_label_to_sizer (table, this, "Create in folder");
	_folder = new wxDirPickerCtrl (this, wxDD_DIR_MUST_EXIST);
	table->Add (_folder, 1, wxEXPAND);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (overall_sizer);
	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);
}

string
NewFilmDialog::get_path () const
{
	filesystem::path p;
	p /= wx_to_std (_folder->GetPath ());
	p /= wx_to_std (_name->GetValue ());
	return p.string ();
}
