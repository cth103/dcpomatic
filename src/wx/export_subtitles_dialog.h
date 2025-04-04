/*
    Copyright (C) 2017-2020 Carl Hetherington <cth@carlh.net>

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


#include "dir_picker_ctrl.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>


class CheckBox;
class FilePickerCtrl;


class ExportSubtitlesDialog : public wxDialog
{
public:
	ExportSubtitlesDialog (wxWindow* parent, int reels, bool interop);

	boost::filesystem::path path () const;
	bool split_reels () const;
	bool include_font () const;

private:
	void setup_sensitivity ();

	bool _interop;
	CheckBox* _split_reels;
	CheckBox* _include_font = nullptr;
	wxStaticText* _file_label;
	FilePickerCtrl* _file;
	DirPickerCtrl* _dir;
	wxStaticText* _dir_label;
};
