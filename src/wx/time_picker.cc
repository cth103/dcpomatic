/*
    Copyright (C) 2016-2020 Carl Hetherington <cth@carlh.net>

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
#include "static_text.h"
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


TimePicker::TimePicker (wxWindow* parent)
	: wxPanel (parent)
{

}


TimePickerSpin::TimePickerSpin (wxWindow* parent, wxDateTime time)
	: TimePicker (parent)
{
	wxClientDC dc (parent);
	wxSize size = dc.GetTextExtent (wxT ("9999999"));
	size.SetHeight (-1);

	wxBoxSizer* sizer = new wxBoxSizer (wxHORIZONTAL);
	_hours = new wxSpinCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, size);
	sizer->Add (_hours, 1, wxLEFT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_GAP);
	sizer->Add (new StaticText(this, wxT(":")), 0, wxALIGN_CENTER_VERTICAL);
	_minutes = new wxSpinCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, size);
	sizer->Add (_minutes, 1, wxRIGHT | wxALIGN_CENTER_VERTICAL, DCPOMATIC_SIZER_GAP);

	SetSizerAndFit (sizer);

	_minutes->MoveAfterInTabOrder (_hours);

	_hours->SetValue (time.GetHour ());
	_hours->SetRange (0, 23);
	_minutes->SetValue (time.GetMinute ());
	_minutes->SetRange (0, 59);

	_hours->Bind (wxEVT_SPINCTRL, (bind (&TimePickerSpin::changed, this)));
	_minutes->Bind (wxEVT_SPINCTRL, (bind (&TimePickerSpin::changed, this)));
}


void
TimePickerSpin::changed ()
{
	Changed ();
}


int
TimePickerSpin::hours () const
{
	return _hours->GetValue();
}


int
TimePickerSpin::minutes () const
{
	return _minutes->GetValue();
}


TimePickerText::TimePickerText (wxWindow* parent, wxDateTime time)
	: TimePicker (parent)
{
	wxClientDC dc (parent);
	wxSize size = dc.GetTextExtent (wxT("99999"));
	size.SetHeight (-1);

	wxBoxSizer* sizer = new wxBoxSizer (wxHORIZONTAL);
	_hours = new wxTextCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, size);
	sizer->Add (_hours, 1, wxEXPAND | wxLEFT, DCPOMATIC_SIZER_GAP);
	sizer->Add (new StaticText (this, wxT (":")), 0, wxLEFT | wxRIGHT | wxALIGN_CENTER_VERTICAL, 4);
	_minutes = new wxTextCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, size);
	sizer->Add (_minutes, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);

	SetSizerAndFit (sizer);

	_minutes->MoveAfterInTabOrder (_hours);

	_hours->SetValue (wxString::Format("%d", time.GetHour()));
	_minutes->SetValue (wxString::Format("%d", time.GetMinute()));

	_hours->Bind (wxEVT_TEXT, (bind(&TimePickerText::changed, this)));
	_minutes->Bind (wxEVT_TEXT, (bind(&TimePickerText::changed, this)));
}


void
TimePickerText::changed ()
{
	Changed ();
}


int
TimePickerText::hours () const
{
	return max(0, min(23, wxAtoi(_hours->GetValue())));
}


int
TimePickerText::minutes () const
{
	return max(0, min(59, wxAtoi(_minutes->GetValue())));
}



