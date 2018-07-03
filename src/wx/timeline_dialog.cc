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
#include <wx/graphics.h>
#include <iostream>
#include <list>

using std::list;
using std::cout;
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

	wxBoxSizer* controls = new wxBoxSizer (wxHORIZONTAL);

#ifdef DCPOMATIC_LINUX
	wxBitmap select (wxString::Format (wxT ("%s/select.png"), std_to_wx (shared_path().string())), wxBITMAP_TYPE_PNG);
	wxBitmap zoom (wxString::Format (wxT ("%s/zoom.png"), std_to_wx (shared_path().string())), wxBITMAP_TYPE_PNG);
	wxBitmap zoom_all (wxString::Format (wxT ("%s/zoom_all.png"), std_to_wx (shared_path().string())), wxBITMAP_TYPE_PNG);
#endif
	wxToolBar* toolbar = new wxToolBar (this, wxID_ANY);
	toolbar->AddRadioTool ((int) Timeline::SELECT, _("Select"), select);
	toolbar->AddRadioTool ((int) Timeline::ZOOM, _("Zoom"), zoom);
	toolbar->AddTool ((int) Timeline::ZOOM_ALL, _("Zoom to whole project"), zoom_all);
	controls->Add (toolbar);
	toolbar->Bind (wxEVT_TOOL, bind (&TimelineDialog::tool_clicked, this, _1));

	_snap = new wxCheckBox (this, wxID_ANY, _("Snap"));
	controls->Add (_snap);
	_sequence = new wxCheckBox (this, wxID_ANY, _("Keep video and subtitles in sequence"));
	controls->Add (_sequence, 1, wxLEFT, 12);

	sizer->Add (controls, 0, wxALL, 12);
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

	_snap->SetValue (_timeline.snap ());
	_snap->Bind (wxEVT_CHECKBOX, boost::bind (&TimelineDialog::snap_toggled, this));
	film_changed (Film::SEQUENCE);
	_sequence->Bind (wxEVT_CHECKBOX, boost::bind (&TimelineDialog::sequence_toggled, this));

	_film_changed_connection = film->Changed.connect (bind (&TimelineDialog::film_changed, this, _1));
}

void
TimelineDialog::snap_toggled ()
{
	_timeline.set_snap (_snap->GetValue ());
}

void
TimelineDialog::sequence_toggled ()
{
	shared_ptr<Film> film = _film.lock ();
	if (!film) {
		return;
	}

	film->set_sequence (_sequence->GetValue ());
}

void
TimelineDialog::film_changed (Film::Property p)
{
	shared_ptr<Film> film = _film.lock ();
	if (!film) {
		return;
	}

	if (p == Film::SEQUENCE) {
		_sequence->SetValue (film->sequence ());
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
	_timeline.tool_clicked ((Timeline::Tool) ev.GetId());
}
