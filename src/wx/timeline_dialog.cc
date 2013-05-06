/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include <list>
#include <wx/graphics.h>
#include "timeline_dialog.h"
#include "wx_util.h"
#include "playlist.h"

using std::list;
using std::cout;
using boost::shared_ptr;

TimelineDialog::TimelineDialog (wxWindow* parent, shared_ptr<Playlist> pl)
	: wxDialog (parent, wxID_ANY, _("Timeline"), wxDefaultPosition, wxSize (640, 512), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxFULL_REPAINT_ON_RESIZE)
	, _timeline (this, pl)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	
	sizer->Add (&_timeline, 1, wxEXPAND | wxALL, 12);

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);
}
