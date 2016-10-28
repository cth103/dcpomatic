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

#include "time_picker.h"
#include "wx_util.h"
#include <dcp/locale_convert.h>
#include <wx/spinctrl.h>
#include <boost/bind.hpp>
#include <iomanip>

using std::setfill;
using std::setw;
using std::min;
using std::max;
using std::string;
using std::cout;
using boost::bind;
using dcp::locale_convert;

TimePicker::TimePicker (wxWindow* parent, wxDateTime time)
	: wxPanel (parent)
{
	wxClientDC dc (parent);
	wxSize size = dc.GetTextExtent (wxT ("9999999"));
	size.SetHeight (-1);

	wxBoxSizer* sizer = new wxBoxSizer (wxHORIZONTAL);
	_hours = new wxSpinCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, size);
	sizer->Add (_hours, 1, wxEXPAND | wxLEFT, DCPOMATIC_SIZER_GAP);
	sizer->Add (new wxStaticText (this, wxID_ANY, wxT (":")), 0, wxALIGN_CENTER_VERTICAL);
	_minutes = new wxSpinCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, size);
	sizer->Add (_minutes, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);

	SetSizerAndFit (sizer);

	_minutes->MoveAfterInTabOrder (_hours);

	_hours->SetValue (time.GetHour ());
	_hours->SetRange (0, 23);
	_minutes->SetValue (time.GetMinute ());
	_minutes->SetRange (0, 59);

	_hours->Bind (wxEVT_COMMAND_SPINCTRL_UPDATED, (bind (&TimePicker::spin_changed, this)));
	_minutes->Bind (wxEVT_COMMAND_SPINCTRL_UPDATED, (bind (&TimePicker::spin_changed, this)));
}

void
TimePicker::spin_changed ()
{
	Changed ();
}

int
TimePicker::hours () const
{
	return _hours->GetValue();
}

int
TimePicker::minutes () const
{
	return _minutes->GetValue();
}
