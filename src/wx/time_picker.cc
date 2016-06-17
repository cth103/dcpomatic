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
#include "lib/raw_convert.h"
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

TimePicker::TimePicker (wxWindow* parent, wxDateTime time)
	: wxPanel (parent)
	, _block_update (false)
{
	wxClientDC dc (parent);
	wxSize size = dc.GetTextExtent (wxT ("9999"));
	size.SetHeight (-1);

	wxBoxSizer* sizer = new wxBoxSizer (wxHORIZONTAL);
	_hours = new wxTextCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, size);
	_hours->SetMaxLength (2);
	sizer->Add (_hours, 0);
	_hours_spin = new wxSpinButton (this, wxID_ANY);
	sizer->Add (_hours_spin, 0, wxLEFT | wxRIGHT, 2);
	sizer->Add (new wxStaticText (this, wxID_ANY, wxT (":")), 0, wxALIGN_CENTER_VERTICAL);
	_minutes = new wxTextCtrl (this, wxID_ANY, wxT(""), wxDefaultPosition, size);
	_minutes->SetMaxLength (2);
	sizer->Add (_minutes, 0);
	_minutes_spin = new wxSpinButton (this, wxID_ANY);
	sizer->Add (_minutes_spin, 0, wxLEFT | wxRIGHT, 2);

	SetSizerAndFit (sizer);

	_minutes->MoveAfterInTabOrder (_hours);

	_hours_spin->SetValue (time.GetHour ());
	_hours_spin->SetRange (0, 23);
	_minutes_spin->SetValue (time.GetMinute ());
	_minutes_spin->SetRange (0, 59);

	update_text ();

	_hours->Bind (wxEVT_COMMAND_TEXT_UPDATED, (bind (&TimePicker::update_spin, this)));
	_minutes->Bind (wxEVT_COMMAND_TEXT_UPDATED, (bind (&TimePicker::update_spin, this)));
	_hours_spin->Bind (wxEVT_SPIN, bind (&TimePicker::update_text, this));
	_minutes_spin->Bind (wxEVT_SPIN, bind (&TimePicker::update_text, this));
}

void
TimePicker::update_text ()
{
	if (_block_update) {
		return;
	}

	_block_update = true;

	_hours->SetValue (std_to_wx (raw_convert<string> (_hours_spin->GetValue ())));

	SafeStringStream m;
	m << setfill('0') << setw(2) << _minutes_spin->GetValue();
	_minutes->SetValue (std_to_wx (m.str()));

	_block_update = false;

	Changed ();
}

void
TimePicker::update_spin ()
{
	if (_block_update) {
		return;
	}

	_block_update = true;
	_hours_spin->SetValue (raw_convert<int> (wx_to_std (_hours->GetValue())));
	_minutes_spin->SetValue (raw_convert<int> (wx_to_std (_minutes->GetValue())));
	_block_update = false;

	Changed ();
}

int
TimePicker::hours () const
{
	return _hours_spin->GetValue();
}

int
TimePicker::minutes () const
{
	return _minutes_spin->GetValue();
}
