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

#include <wx/wx.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/signals2.hpp>

class wxDatePickerCtrl;
class wxTimePickerCtrl;

class KDMTimingPanel : public wxPanel
{
public:
	KDMTimingPanel (wxWindow* parent);

	/** @return KDM from time in local time */
	boost::posix_time::ptime from () const;
	/** @return KDM until time in local time */
	boost::posix_time::ptime until () const;

	bool valid () const;

	boost::signals2::signal<void ()> TimingChanged;

private:
	void changed () const;
	static boost::posix_time::ptime posix_time (wxDatePickerCtrl *, wxTimePickerCtrl *);

	wxDatePickerCtrl* _from_date;
	wxDatePickerCtrl* _until_date;
	wxTimePickerCtrl* _from_time;
	wxTimePickerCtrl* _until_time;
	wxStaticText* _warning;
};
