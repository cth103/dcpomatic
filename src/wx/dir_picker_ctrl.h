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


#ifndef DCPOMATIC_DIR_PICKER_CTRL
#define DCPOMATIC_DIR_PICKER_CTRL


#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/signals2.hpp>


class DirPickerCtrl : public wxPanel
{
public:
	explicit DirPickerCtrl (wxWindow *);

	wxString GetPath () const;
	void SetPath (wxString);

	boost::signals2::signal<void ()> Changed;

private:
	void browse_clicked ();

	wxStaticText* _folder;
	wxButton* _browse;
	wxString _path;
	wxSizer* _sizer;
};

#endif
