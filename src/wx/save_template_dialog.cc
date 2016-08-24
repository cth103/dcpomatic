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

#include "save_template_dialog.h"
#include "wx_util.h"
#include <boost/foreach.hpp>

using std::string;

SaveTemplateDialog::SaveTemplateDialog (wxWindow* parent)
	: TableDialog (parent, _("Save template"), 2, 1, true)
{
	add (_("Template name"), true);
	_name = add (new wxTextCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (300, -1)));
	_name->SetFocus ();
	layout ();
}

string
SaveTemplateDialog::name () const
{
	return wx_to_std (_name->GetValue ());
}
