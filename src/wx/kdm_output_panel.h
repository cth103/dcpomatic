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

#include <dcp/types.h>
#include <wx/wx.h>
#include <boost/filesystem.hpp>

class wxDirPickerCtrl;
class DirPickerCtrl;

class KDMOutputPanel : public wxPanel
{
public:
	KDMOutputPanel (wxWindow* parent, bool interop);

	boost::filesystem::path directory () const;
	bool write_to () const;
	dcp::Formulation formulation () const;

	void setup_sensitivity ();

private:
	wxChoice* _type;
	wxRadioButton* _write_to;
#ifdef DCPOMATIC_USE_OWN_PICKER
	DirPickerCtrl* _folder;
#else
	wxDirPickerCtrl* _folder;
#endif
	wxRadioButton* _email;
};
