/*
    Copyright (C) 2012-2013 Carl Hetherington <cth@carlh.net>

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

#include <wx/notebook.h>
#include "film_editor_panel.h"
#include "film_editor.h"

using boost::shared_ptr;

FilmEditorPanel::FilmEditorPanel (FilmEditor* e, wxString name)
	: wxPanel (e->content_notebook (), wxID_ANY)
	, _editor (e)
	, _sizer (new wxBoxSizer (wxVERTICAL))
{
	e->content_notebook()->AddPage (this, name, false);
	SetSizer (_sizer);
}

