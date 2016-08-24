/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "rename_template_dialog.h"

RenameTemplateDialog::RenameTemplateDialog (wxWindow* parent)
	: TableDialog (parent, _("Rename template"), 2, 1, true)
{
	add (_("New name"), true);
	_name = add (new wxTextCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (300, -1)));
	layout ();
}

wxString
RenameTemplateDialog::get () const
{
	return _name->GetValue ();
}

void
RenameTemplateDialog::set (wxString s)
{
	_name->SetValue (s);
}
