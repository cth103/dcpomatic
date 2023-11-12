/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_button.h"
#include "file_picker_ctrl.h"
#include "wx_util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/filepicker.h>
#include <wx/stdpaths.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>


using namespace std;
using namespace boost;


FilePickerCtrl::FilePickerCtrl (wxWindow* parent, wxString prompt, wxString wildcard, bool open, bool warn_overwrite)
	: wxPanel (parent)
	, _prompt (prompt)
	, _wildcard (wildcard)
	, _open (open)
	, _warn_overwrite (warn_overwrite)
{
	_sizer = new wxBoxSizer (wxHORIZONTAL);

        wxClientDC dc (parent);
        wxSize size = dc.GetTextExtent (wxT ("This is the length of the file label it should be quite long"));
        size.SetHeight (-1);

	_file = new Button (this, _("(None)"), wxDefaultPosition, size, wxBU_LEFT);
	_sizer->Add (_file, 1, wxEXPAND, 0);

	SetSizerAndFit (_sizer);
	_file->Bind (wxEVT_BUTTON, boost::bind (&FilePickerCtrl::browse_clicked, this));
}

void
FilePickerCtrl::SetPath (wxString p)
{
	_path = p;

	if (!_path.IsEmpty ()) {
		_file->SetLabel(std_to_wx(filesystem::path(wx_to_std(_path)).filename().string()));
	} else {
		_file->SetLabel (_("(None)"));
	}

	wxCommandEvent ev (wxEVT_FILEPICKER_CHANGED, wxID_ANY);
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
	long style = _open ? wxFD_OPEN : wxFD_SAVE;
	if (_warn_overwrite) {
		style |= wxFD_OVERWRITE_PROMPT;
	}
	wxFileDialog dialog(this, _prompt, wxEmptyString, wxEmptyString, _wildcard, style);
	dialog.SetPath(_path);
	if (dialog.ShowModal() == wxID_OK) {
		SetPath(dialog.GetPath());
	}
}

void
FilePickerCtrl::SetWildcard (wxString w)
{
	_wildcard = w;
}
