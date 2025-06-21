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


#include "wx_util.h"
#include <dcp/utc_offset.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/signals2.hpp>


class Choice;
class TimePicker;
class wxDatePickerCtrl;


class KDMTimingPanel : public wxPanel
{
public:
	explicit KDMTimingPanel (wxWindow* parent);

	boost::posix_time::ptime from() const;
	boost::posix_time::ptime until() const;

	bool valid () const;

	boost::signals2::signal<void ()> TimingChanged;

private:
	void changed () const;

	static boost::posix_time::ptime posix_time(wxDatePickerCtrl *, TimePicker *);

	wxDatePickerCtrl* _from_date;
	wxDatePickerCtrl* _until_date;
	TimePicker* _from_time;
	TimePicker* _until_time;
	wxStaticText* _warning;
	std::vector<Offset> _offsets;
};
