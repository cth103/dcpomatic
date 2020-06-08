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

#include "rating_dialog.h"
#include "wx_util.h"

RatingDialog::RatingDialog (wxWindow* parent)
	: TableDialog (parent, _("Ratings"), 2, 1, true)
{
	add (_("Agency"), true);
	_agency = new wxTextCtrl (this, wxID_ANY);
	add (_agency);

	add (_("Label"), true);
	_label = new wxTextCtrl (this, wxID_ANY);
	add (_label);

	layout ();

	_agency->SetFocus ();
}

void
RatingDialog::set (dcp::Rating r)
{
	_agency->SetValue (std_to_wx(r.agency));
	_label->SetValue (std_to_wx(r.label));
}

dcp::Rating
RatingDialog::get () const
{
	return dcp::Rating(wx_to_std(_agency->GetValue()), wx_to_std(_label->GetValue()));
}
