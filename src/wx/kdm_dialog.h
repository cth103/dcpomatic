/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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
struct CPLSummary;

class KDMDialog : public wxDialog
{
public:
	KDMDialog (wxWindow *, boost::shared_ptr<const Film>);

	std::list<boost::shared_ptr<Screen> > screens () const;
	/** @return KDM from time in local time */
	boost::posix_time::ptime from () const;
	/** @return KDM until time in local time */
	boost::posix_time::ptime until () const;

	boost::filesystem::path cpl () const;

	boost::filesystem::path directory () const;
	bool write_to () const;
	dcp::Formulation formulation () const;

private:
	void setup_sensitivity ();
	void update_cpl_choice ();
	void update_cpl_summary ();
	void cpl_browse_clicked ();

	ScreensPanel* _screens;
	KDMTimingPanel* _timing;
	KDMOutputPanel* _output;
	wxChoice* _cpl;
	wxButton* _cpl_browse;
	wxStaticText* _dcp_directory;
	wxStaticText* _cpl_id;
	wxStaticText* _cpl_annotation_text;

	std::vector<CPLSummary> _cpls;
};
