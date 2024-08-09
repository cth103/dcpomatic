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
#include "file_dialog.h"
#include "file_picker_ctrl.h"
#include "wx_util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/filepicker.h>
#include <wx/stdpaths.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>


using std::string;
using boost::optional;


FilePickerCtrl::FilePickerCtrl(
	wxWindow* parent,
	wxString prompt,
	wxString wildcard,
	bool open,
	bool warn_overwrite,
	std::string initial_path_key,
	boost::optional<std::string> initial_filename,
	boost::optional<boost::filesystem::path> override_path
	)
	: wxPanel (parent)
	, _prompt (prompt)
	, _wildcard (wildcard)
	, _open (open)
	, _warn_overwrite (warn_overwrite)
	, _initial_path_key(initial_path_key)
	, _initial_filename(initial_filename)
	, _override_path(override_path)
{
	_sizer = new wxBoxSizer (wxHORIZONTAL);

        wxClientDC dc (parent);
        wxSize size = dc.GetTextExtent(char_to_wx("This is the length of the file label it should be quite long"));
        size.SetHeight (-1);

	_file = new Button (this, _("(None)"), wxDefaultPosition, size, wxBU_LEFT);
	_sizer->Add (_file, 1, wxEXPAND, 0);

	SetSizerAndFit (_sizer);
	_file->Bind (wxEVT_BUTTON, boost::bind (&FilePickerCtrl::browse_clicked, this));

	set_filename(_initial_filename);
}


void
FilePickerCtrl::set_filename(boost::optional<string> filename)
{
	if (filename) {
		_file->SetLabel(std_to_wx(*filename));
	} else {
		_file->SetLabel(_("None"));
	}
}


void
FilePickerCtrl::set_path(boost::optional<boost::filesystem::path> path)
{
	_path = path;

	if (_path) {
		set_filename(_path->filename().string());
	} else {
		set_filename({});
	}

	wxCommandEvent ev (wxEVT_FILEPICKER_CHANGED, wxID_ANY);
	GetEventHandler()->ProcessEvent (ev);
}


boost::optional<boost::filesystem::path>
FilePickerCtrl::path() const
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
	FileDialog dialog(this, _prompt, _wildcard, style, _initial_path_key, _initial_filename, _path);
	if (dialog.show()) {
		set_path(dialog.path());
	}
}

void
FilePickerCtrl::set_wildcard(wxString w)
{
	_wildcard = w;
}
