/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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
#include <dcp/types.h>
#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <map>

class Cinema;
class Screen;
class Film;
class ScreensPanel;
class KDMTimingPanel;
class KDMOutputPanel;
class KDMCPLPanel;
struct CPLSummary;

class KDMDialog : public wxDialog
{
public:
	KDMDialog (wxWindow *, boost::shared_ptr<const Film> film);

private:
	void setup_sensitivity ();
	void make_clicked ();

	boost::weak_ptr<const Film> _film;
	ScreensPanel* _screens;
	KDMTimingPanel* _timing;
	KDMCPLPanel* _cpl;
	KDMOutputPanel* _output;
	wxButton* _make;
};
