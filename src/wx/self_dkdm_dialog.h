/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

class Film;
class KDMCPLPanel;
class wxDirPickerCtrl;
class DirPickerCtrl;

class SelfDKDMDialog : public wxDialog
{
public:
	SelfDKDMDialog (wxWindow *, boost::shared_ptr<const Film>);

	boost::filesystem::path cpl () const;

	bool internal () const;
	boost::filesystem::path directory () const;

private:
	void setup_sensitivity ();

	KDMCPLPanel* _cpl;
	wxRadioButton* _internal;
	wxRadioButton* _write_to;
#ifdef DCPOMATIC_USE_OWN_PICKER
	DirPickerCtrl* _folder;
#else
	wxDirPickerCtrl* _folder;
#endif
};
