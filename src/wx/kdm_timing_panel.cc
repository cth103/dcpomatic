/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "kdm_timing_panel.h"
#include "wx_util.h"
#include <wx/datectrl.h>
#include <wx/timectrl.h>

KDMTimingPanel::KDMTimingPanel (wxWindow* parent)
	: wxPanel (parent, wxID_ANY)
{
	wxFlexGridSizer* table = new wxFlexGridSizer (3, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	add_label_to_sizer (table, this, _("From"), true);
	wxDateTime from;
	from.SetToCurrent ();
	_from_date = new wxDatePickerCtrl (this, wxID_ANY, from);
	table->Add (_from_date, 1, wxEXPAND);
	_from_time = new wxTimePickerCtrl (this, wxID_ANY, from);
	table->Add (_from_time, 1, wxEXPAND);

	add_label_to_sizer (table, this, _("Until"), true);
	wxDateTime to = from;
	/* 1 week from now */
	to.Add (wxDateSpan (0, 0, 1, 0));
	_until_date = new wxDatePickerCtrl (this, wxID_ANY, to);
	table->Add (_until_date, 1, wxEXPAND);
	_until_time = new wxTimePickerCtrl (this, wxID_ANY, to);
	table->Add (_until_time, 1, wxEXPAND);

	SetSizer (table);
}

boost::posix_time::ptime
KDMTimingPanel::from () const
{
	return posix_time (_from_date, _from_time);
}

boost::posix_time::ptime
KDMTimingPanel::posix_time (wxDatePickerCtrl* date_picker, wxTimePickerCtrl* time_picker)
{
	wxDateTime const date = date_picker->GetValue ();
	wxDateTime const time = time_picker->GetValue ();
	return boost::posix_time::ptime (
		boost::gregorian::date (date.GetYear(), date.GetMonth() + 1, date.GetDay()),
		boost::posix_time::time_duration (time.GetHour(), time.GetMinute(), time.GetSecond())
		);
}

boost::posix_time::ptime
KDMTimingPanel::until () const
{
	return posix_time (_until_date, _until_time);
}
