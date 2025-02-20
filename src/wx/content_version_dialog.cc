/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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


#include "content_version_dialog.h"
#include "wx_util.h"


using std::string;
using std::vector;
using boost::optional;


ContentVersionDialog::ContentVersionDialog (wxWindow* parent)
	: TableDialog (parent, _("Content version"), 2, 1, true)
{
	add (_("Content version"), true);
	_version= new wxTextCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(300, -1));
	add (_version);

	layout ();

	_version->SetFocus ();
}


void
ContentVersionDialog::set (string r)
{
	_version->SetValue (std_to_wx(r));
}


vector<string>
ContentVersionDialog::get () const
{
	return { wx_to_std(_version->GetValue()) };
}
