/*
    Copyright (C) 2016-2018 Carl Hetherington <cth@carlh.net>

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

#include "rgba_colour_picker.h"
#include "wx_util.h"
#include "static_text.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/clrpicker.h>
LIBDCP_ENABLE_WARNINGS

RGBAColourPicker::RGBAColourPicker (wxWindow* parent, RGBA colour)
	: wxPanel (parent, wxID_ANY)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxHORIZONTAL);

	_picker = new wxColourPickerCtrl (this, wxID_ANY);
	_picker->SetColour (wxColour (colour.r, colour.g, colour.b));
	sizer->Add (_picker, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_X_GAP);
	sizer->Add (new StaticText (this, _("Alpha   0")), 0, wxALIGN_CENTRE_VERTICAL);
	_alpha = new wxSlider (this, wxID_ANY, colour.a, 0, 255);
	sizer->Add (_alpha, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_X_GAP);
	sizer->Add (new StaticText (this, _("255")), 0, wxALIGN_CENTRE_VERTICAL);

	SetSizer (sizer);
}

RGBA
RGBAColourPicker::colour () const
{
	wxColour const c = _picker->GetColour ();
	return RGBA (c.Red(), c.Green(), c.Blue(), _alpha->GetValue());
}

void
RGBAColourPicker::set (RGBA colour)
{
	_picker->SetColour (wxColour (colour.r, colour.g, colour.b));
	_alpha->SetValue (colour.a);
}
