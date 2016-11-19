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

#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/filepicker.h>
#include <boost/filesystem.hpp>
#include "dir_picker_ctrl.h"
#include "wx_util.h"

using namespace std;
using namespace boost;

DirPickerCtrl::DirPickerCtrl (wxWindow* parent)
	: wxPanel (parent)
{
	_sizer = new wxBoxSizer (wxHORIZONTAL);

	_folder = new wxStaticText (this, wxID_ANY, wxT (""));
	_sizer->Add (_folder, 1, wxEXPAND | wxALL, 6);
	_browse = new wxButton (this, wxID_ANY, _("Browse..."));
	_sizer->Add (_browse, 0);

	SetSizer (_sizer);

	_browse->Bind (wxEVT_BUTTON, boost::bind (&DirPickerCtrl::browse_clicked, this));
}

void
DirPickerCtrl::SetPath (wxString p)
{
	_path = p;

	if (_path == wxStandardPaths::Get().GetDocumentsDir()) {
		_folder->SetLabel (_("My Documents"));
	} else {
		_folder->SetLabel (std_to_wx (filesystem::path (wx_to_std (_path)).leaf().string()));
	}

	wxCommandEvent ev (wxEVT_DIRPICKER_CHANGED, wxID_ANY);
	GetEventHandler()->ProcessEvent (ev);

	_sizer->Layout ();
	SetMinSize (wxSize (max (400, _sizer->GetSize().GetWidth()), -1));
}

wxString
DirPickerCtrl::GetPath () const
{
	return _path;
}

void
DirPickerCtrl::browse_clicked ()
{
	wxDirDialog* d = new wxDirDialog (this);
	if (d->ShowModal () == wxID_OK) {
		SetPath (d->GetPath ());
	}
	d->Destroy ();
}
