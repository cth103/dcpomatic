/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "file_picker_ctrl.h"
#include "wx_util.h"
#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/filepicker.h>
#include <boost/filesystem.hpp>

using namespace std;
using namespace boost;

FilePickerCtrl::FilePickerCtrl (wxWindow* parent, wxString prompt, wxString wildcard)
	: wxPanel (parent)
	, _prompt (prompt)
	, _wildcard (wildcard)
{
	_sizer = new wxBoxSizer (wxHORIZONTAL);

        wxClientDC dc (parent);
        wxSize size = dc.GetTextExtent (wxT ("This is the length of the file label it should be quite long"));
        size.SetHeight (-1);

	_file = new wxButton (this, wxID_ANY, _("(None)"), wxDefaultPosition, size, wxBU_LEFT);
	_sizer->Add (_file, 1, wxEXPAND | wxALL, 6);

	SetSizerAndFit (_sizer);

	_file->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&FilePickerCtrl::browse_clicked, this));
}

void
FilePickerCtrl::SetPath (wxString p)
{
	_path = p;

	if (!_path.IsEmpty ()) {
		_file->SetLabel (std_to_wx (filesystem::path (wx_to_std (_path)).leaf().string()));
	} else {
		_file->SetLabel (_("(None)"));
	}

	wxCommandEvent ev (wxEVT_COMMAND_FILEPICKER_CHANGED, wxID_ANY);
	GetEventHandler()->ProcessEvent (ev);
}

wxString
FilePickerCtrl::GetPath () const
{
	return _path;
}

void
FilePickerCtrl::browse_clicked ()
{
	wxFileDialog* d = new wxFileDialog (this, _prompt, wxEmptyString, wxEmptyString, _wildcard);
	if (d->ShowModal () == wxID_OK) {
		SetPath (d->GetPath ());
	}
	d->Destroy ();
}
