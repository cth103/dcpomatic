/*
    Copyright (C) 2015-2020 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_choice.h"
#include "kdm_timing_panel.h"
#include "static_text.h"
#include "time_picker.h"
#include "wx_util.h"
#include "lib/config.h"
#include <dcp/utc_offset.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/datectrl.h>
#include <wx/dateevt.h>
LIBDCP_ENABLE_WARNINGS


using std::cout;
using boost::bind;


KDMTimingPanel::KDMTimingPanel (wxWindow* parent)
	: wxPanel (parent, wxID_ANY)
{
	auto overall_sizer = new wxBoxSizer (wxVERTICAL);

#ifdef __WXGTK3__
	/* wxDatePickerCtrl is too small with the GTK3 backend so we need to make it bigger with some fudge factors */
	wxClientDC dc (parent);
	auto size = dc.GetTextExtent(wxT("99/99/9999"));
	size.SetWidth (size.GetWidth() * 1.75);
	size.SetHeight (-1);
#else
	auto size = wxDefaultSize;
#endif

	auto table = new wxBoxSizer (wxHORIZONTAL);
	add_label_to_sizer (table, this, _("From"), false, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
	wxDateTime from;
	from.SetToCurrent ();
	_from_date = new wxDatePickerCtrl (this, wxID_ANY, from, wxDefaultPosition, size);
#ifdef DCPOMATIC_OSX
	/* Hack to tweak alignment, which I can't get right by "proper" means for some reason */
	table->Add (_from_date, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, 4);
#else
	table->Add (_from_date, 0, wxALIGN_CENTER_VERTICAL);
#endif

#ifdef __WXGTK3__
	_from_time = new TimePickerText (this, from);
#else
	_from_time = new TimePickerSpin (this, from);
#endif

	table->Add (_from_time, 0, wxALIGN_CENTER_VERTICAL);

	add_label_to_sizer (table, this, _("until"), false, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
	auto to = from;

	auto const duration = Config::instance()->default_kdm_duration();
	switch (duration.unit) {
	case RoughDuration::Unit::DAYS:
		to.Add(wxDateSpan(0, 0, 0, duration.duration));
		break;
	case RoughDuration::Unit::WEEKS:
		to.Add(wxDateSpan(0, 0, duration.duration, 0));
		break;
	case RoughDuration::Unit::MONTHS:
		to.Add(wxDateSpan(0, duration.duration, 0, 0));
		break;
	case RoughDuration::Unit::YEARS:
		to.Add(wxDateSpan(duration.duration, 0, 0, 0));
		break;
	}

	_until_date = new wxDatePickerCtrl (this, wxID_ANY, to, wxDefaultPosition, size);
#ifdef DCPOMATIC_OSX
	/* Hack to tweak alignment, which I can't get right by "proper" means for some reason */
	table->Add (_until_date, 0, wxALIGN_CENTER_VERTICAL | wxBOTTOM, 4);
#else
	table->Add (_until_date, 0, wxALIGN_CENTER_VERTICAL);
#endif

#ifdef __WXGTK3__
	_until_time = new TimePickerText (this, to);
#else
	_until_time = new TimePickerSpin (this, to);
#endif

	table->Add (_until_time, 0, wxALIGN_CENTER_VERTICAL);

	add_label_to_sizer(table, this, _("UTC offset (time zone)"), true, 1, wxALIGN_CENTRE_VERTICAL);
	_utc_offset = new Choice(this);
	table->Add(_utc_offset, 0, wxALIGN_CENTRE_VERTICAL | wxLEFT, DCPOMATIC_SIZER_X_GAP);

	overall_sizer->Add (table, 0, wxTOP, DCPOMATIC_SIZER_GAP);

	_warning = new StaticText (this, wxT(""));
	overall_sizer->Add (_warning, 0, wxTOP, DCPOMATIC_SIZER_GAP);
	wxFont font = _warning->GetFont();
	font.SetStyle(wxFONTSTYLE_ITALIC);
	font.SetPointSize(font.GetPointSize() - 1);
	_warning->SetForegroundColour (wxColour (255, 0, 0));
	_warning->SetFont(font);

	/* Default to UTC */
	size_t sel = get_offsets(_offsets);
	for (size_t i = 0; i < _offsets.size(); ++i) {
		_utc_offset->add(_offsets[i].name);
		if (_offsets[i].hour == 0 && _offsets[i].minute == 0) {
			sel = i;
		}
	}

	_utc_offset->set(sel);

	/* I said I've been to the year 3000.  Not much has changed but they live underwater.  And your In-in-in-interop DCP
           is pretty fine.
	 */
	_from_date->SetRange(wxDateTime(1, wxDateTime::Jan, 1900, 0, 0, 0, 0), wxDateTime(31, wxDateTime::Dec, 3000, 0, 0, 0, 0));
	_until_date->SetRange(wxDateTime(1, wxDateTime::Jan, 1900, 0, 0, 0, 0), wxDateTime(31, wxDateTime::Dec, 3000, 0, 0, 0, 0));

	_from_date->Bind (wxEVT_DATE_CHANGED, bind (&KDMTimingPanel::changed, this));
	_until_date->Bind (wxEVT_DATE_CHANGED, bind (&KDMTimingPanel::changed, this));
	_from_time->Changed.connect (bind (&KDMTimingPanel::changed, this));
	_until_time->Changed.connect (bind (&KDMTimingPanel::changed, this));
	_utc_offset->bind(&KDMTimingPanel::changed, this);

	SetSizer (overall_sizer);
}


dcp::LocalTime
KDMTimingPanel::from () const
{
	return local_time(_from_date, _from_time, utc_offset());
}


dcp::LocalTime
KDMTimingPanel::local_time(wxDatePickerCtrl* date_picker, TimePicker* time_picker, dcp::UTCOffset offset)
{
	auto const date = date_picker->GetValue ();
	return dcp::LocalTime(
		date.GetYear(),
		date.GetMonth() + 1,
		date.GetDay(),
		time_picker->hours(),
		time_picker->minutes(),
		offset
		);
}


dcp::LocalTime
KDMTimingPanel::until () const
{
	return local_time(_until_date, _until_time, utc_offset());
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


dcp::UTCOffset
KDMTimingPanel::utc_offset() const
{
	auto const sel = _utc_offset->get();
	if (!sel || *sel >= int(_offsets.size())) {
		return {};
	}

	auto const& offset = _offsets[*sel];
	int const minute_scale = offset.hour < 0 ? -1 : 1;

	return { offset.hour, minute_scale * offset.minute };
}


