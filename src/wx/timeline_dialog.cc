/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

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

#include "film_editor.h"
#include "timeline_dialog.h"
#include "wx_util.h"
#include "content_panel.h"
#include "lib/playlist.h"
#include "lib/cross.h"
#include "lib/compose.hpp"
#include <wx/graphics.h>
#include <iostream>
#include <list>

using std::list;
using std::cout;
using std::string;
using boost::shared_ptr;

TimelineDialog::TimelineDialog (ContentPanel* cp, shared_ptr<Film> film)
	: wxDialog (
		cp->panel(),
		wxID_ANY,
		_("Timeline"),
		wxDefaultPosition,
		wxSize (640, 512),
#ifdef DCPOMATIC_OSX
		/* I can't get wxFRAME_FLOAT_ON_PARENT to work on OS X, and although wxSTAY_ON_TOP keeps
		   the window above all others (and not just our own) it's better than nothing for now.
		*/
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxFULL_REPAINT_ON_RESIZE | wxSTAY_ON_TOP
#else
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxFULL_REPAINT_ON_RESIZE | wxFRAME_FLOAT_ON_PARENT
#endif
		)
	, _film (film)
	, _timeline (this, cp, film)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);

	wxBitmap select (bitmap_path("select"), wxBITMAP_TYPE_PNG);
	wxBitmap zoom (bitmap_path("zoom"), wxBITMAP_TYPE_PNG);
	wxBitmap zoom_all (bitmap_path("zoom_all"), wxBITMAP_TYPE_PNG);
	wxBitmap snap (bitmap_path("snap"), wxBITMAP_TYPE_PNG);
	wxBitmap sequence (bitmap_path("sequence"), wxBITMAP_TYPE_PNG);

	_toolbar = new wxToolBar (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_TEXT);
	_toolbar->SetMargins (4, 4);
	_toolbar->AddRadioTool ((int) Timeline::SELECT, _("Select"), select, wxNullBitmap, _("Select and move content"));
	_toolbar->AddRadioTool ((int) Timeline::ZOOM, _("Zoom"), zoom, wxNullBitmap, _("Zoom in / out"));
	_toolbar->AddTool ((int) Timeline::ZOOM_ALL, _("Zoom all"), zoom_all, _("Zoom out to whole film"));
	_toolbar->AddCheckTool ((int) Timeline::SNAP, _("Snap"), snap, wxNullBitmap, _("Snap"));
	_toolbar->AddCheckTool ((int) Timeline::SEQUENCE, _("Sequence"), sequence, wxNullBitmap, _("Keep video and subtitles in sequence"));
	_toolbar->Realize ();

	_toolbar->Bind (wxEVT_TOOL, bind (&TimelineDialog::tool_clicked, this, _1));

	sizer->Add (_toolbar, 0, wxALL, 12);
	sizer->Add (&_timeline, 1, wxEXPAND | wxALL, 12);

#ifdef DCPOMATIC_LINUX
	wxSizer* buttons = CreateSeparatedButtonSizer (wxCLOSE);
	if (buttons) {
		sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}
#endif

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);

        _toolbar->ToggleTool ((int) Timeline::SNAP, _timeline.snap ());
	film_changed (Film::SEQUENCE);

	_film_changed_connection = film->Changed.connect (bind (&TimelineDialog::film_changed, this, _1));
}

wxString
TimelineDialog::bitmap_path (string name)
{
	boost::filesystem::path p = shared_path() / String::compose("%1.png", name);
	cout << "Loading " << p.string() << "\n";
	return std_to_wx (p.string());
}

void
TimelineDialog::film_changed (Film::Property p)
{
	shared_ptr<Film> film = _film.lock ();
	if (!film) {
		return;
	}

	if (p == Film::SEQUENCE) {
		_toolbar->ToggleTool ((int) Timeline::SEQUENCE, film->sequence ());
	}
}

void
TimelineDialog::set_selection (ContentList selection)
{
	_timeline.set_selection (selection);
}

void
TimelineDialog::tool_clicked (wxCommandEvent& ev)
{
	Timeline::Tool t = (Timeline::Tool) ev.GetId();
	_timeline.tool_clicked (t);
	if (t == Timeline::SNAP) {
		_timeline.set_snap (_snap->IsToggled());
	} else if (t == Timeline::SEQUENCE) {
		shared_ptr<Film> film = _film.lock ();
		if (film) {
			film->set_sequence (_sequence->IsToggled());
		}
	}
}
