/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "make_signer_chain_dialog.h"

MakeSignerChainDialog::MakeSignerChainDialog (wxWindow* parent)
	: TableDialog (parent, _("Make certificate chain"), 2, true)
{
	add (_("Organisation"), true);
	add (_organisation = new wxTextCtrl (this, wxID_ANY));
	add (_("Organisational unit"), true);
	add (_organisational_unit = new wxTextCtrl (this, wxID_ANY));
	add (_("Root common name"), true);
	add (_root_common_name = new wxTextCtrl (this, wxID_ANY));
	add (_("Intermediate common name"), true);
	add (_intermediate_common_name = new wxTextCtrl (this, wxID_ANY));
	add (_("Leaf common name"), true);
	add (_leaf_common_name = new wxTextCtrl (this, wxID_ANY));

	layout ();

	SetSize (640, -1);
}
