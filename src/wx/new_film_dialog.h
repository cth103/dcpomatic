/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#include <wx/wx.h>
#include <wx/filepicker.h>
#include "wx_util.h"
#include "table_dialog.h"

class DirPickerCtrl;

class NewFilmDialog : public TableDialog
{
public:
	NewFilmDialog (wxWindow *);
	~NewFilmDialog ();

	boost::filesystem::path get_path () const;

private:
	wxTextCtrl* _name;
#ifdef DCPOMATIC_USE_OWN_DIR_PICKER
	DirPickerCtrl* _folder;
#else
	wxDirPickerCtrl* _folder;
#endif
	static boost::optional<boost::filesystem::path> _directory;
};
