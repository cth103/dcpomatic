/*
    Copyright (C) 2015-2016 Carl Hetherington <cth@carlh.net>

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

#include "kdm_timing_panel.h"
#include "wx_util.h"
#include <wx/datectrl.h>
#include <wx/timectrl.h>
#include <wx/dateevt.h>

using boost::bind;

KDMTimingPanel::KDMTimingPanel (wxWindow* parent)
	: wxPanel (parent, wxID_ANY)
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);

	wxFlexGridSizer* table = new wxFlexGridSizer (6, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	add_label_to_sizer (table, this, _("From"), true);
	wxDateTime from;
	from.SetToCurrent ();
	_from_date = new wxDatePickerCtrl (this, wxID_ANY, from);
	table->Add (_from_date, 1, wxEXPAND);
	_from_time = new wxTimePickerCtrl (this, wxID_ANY, from);
	table->Add (_from_time, 1, wxEXPAND);

	add_label_to_sizer (table, this, _("until"), true);
	wxDateTime to = from;
	/* 1 week from now */
	to.Add (wxDateSpan (0, 0, 1, 0));
	_until_date = new wxDatePickerCtrl (this, wxID_ANY, to);
	table->Add (_until_date, 1, wxEXPAND);
	_until_time = new wxTimePickerCtrl (this, wxID_ANY, to);
	table->Add (_until_time, 1, wxEXPAND);

	overall_sizer->Add (table);

	_warning = new wxStaticText (this, wxID_ANY, wxT (""));
	overall_sizer->Add (_warning, 0, wxTOP, DCPOMATIC_SIZER_GAP);
	wxFont font = _warning->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_warning->SetForegroundColour (wxColour (255, 0, 0));
	_warning->SetFont(font);

	_from_date->Bind (wxEVT_DATE_CHANGED, bind (&KDMTimingPanel::changed, this));
	_until_date->Bind (wxEVT_DATE_CHANGED, bind (&KDMTimingPanel::changed, this));
	_from_time->Bind (wxEVT_TIME_CHANGED, bind (&KDMTimingPanel::changed, this));
	_until_time->Bind (wxEVT_TIME_CHANGED, bind (&KDMTimingPanel::changed, this));

	SetSizer (overall_sizer);
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

bool
KDMTimingPanel::valid () const
{
	return until() > from();
}

void
KDMTimingPanel::changed () const
{
	if (valid ()) {
		_warning->SetLabel (wxT (""));
	} else {
		_warning->SetLabel (_("The 'until' time must be after the 'from' time."));
	}

	TimingChanged ();
}
