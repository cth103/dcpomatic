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

#include "table_dialog.h"
#include "wx_util.h"
#include "lib/font_files.h"

class FontFilesDialog : public TableDialog
{
public:
	FontFilesDialog (wxWindow* parent, FontFiles files);

	FontFiles get () const {
		return _files;
	}

private:
	void set_from_file_clicked (FontFiles::Variant variant);
#ifdef DCPOMATIC_WINDOWS
	void set_from_system_clicked (FontFiles::Variant variant);
#endif
	void set (FontFiles::Variant variant, boost::filesystem::path path);

	FontFiles _files;

	wxStaticText* _name[FontFiles::VARIANTS];
	wxButton* _set_file[FontFiles::VARIANTS];

#ifdef DCPOMATIC_WINDOWS
	wxButton* _set_system[FontFiles::VARIANTS];
#endif
};
