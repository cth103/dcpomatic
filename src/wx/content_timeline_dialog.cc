/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "content_panel.h"
#include "content_timeline_dialog.h"
#include "film_editor.h"
#include "wx_util.h"
#include "lib/cross.h"
#include "lib/film.h"
#include "lib/playlist.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/graphics.h>
LIBDCP_ENABLE_WARNINGS
#include <list>


using std::list;
using std::shared_ptr;
using std::string;
using std::weak_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


ContentTimelineDialog::ContentTimelineDialog(ContentPanel* cp, shared_ptr<Film> film, FilmViewer& viewer)
	: wxDialog (
		cp->window(),
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
	, _timeline (this, cp, film, viewer)
{
	auto sizer = new wxBoxSizer (wxVERTICAL);

	wxBitmap select(icon_path("select"), wxBITMAP_TYPE_PNG);
	wxBitmap zoom(icon_path("zoom"), wxBITMAP_TYPE_PNG);
	wxBitmap zoom_all(icon_path("zoom_all"), wxBITMAP_TYPE_PNG);
	wxBitmap snap(icon_path("snap"), wxBITMAP_TYPE_PNG);
	wxBitmap sequence(icon_path("sequence"), wxBITMAP_TYPE_PNG);

	_toolbar = new wxToolBar (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL);
	_toolbar->SetMargins (4, 4);
	_toolbar->SetToolBitmapSize (wxSize(32, 32));
	_toolbar->AddRadioTool(static_cast<int>(ContentTimeline::SELECT), _("Select"), select, wxNullBitmap, _("Select and move content"));
	_toolbar->AddRadioTool(static_cast<int>(ContentTimeline::ZOOM), _("Zoom"), zoom, wxNullBitmap, _("Zoom in / out"));
	_toolbar->AddTool(static_cast<int>(ContentTimeline::ZOOM_ALL), _("Zoom all"), zoom_all, _("Zoom out to whole film"));
	_toolbar->AddCheckTool(static_cast<int>(ContentTimeline::SNAP), _("Snap"), snap, wxNullBitmap, _("Snap"));
	_toolbar->AddCheckTool(static_cast<int>(ContentTimeline::SEQUENCE), _("Sequence"), sequence, wxNullBitmap, _("Keep video and subtitles in sequence"));
	_toolbar->Realize ();

	_toolbar->Bind(wxEVT_TOOL, bind(&ContentTimelineDialog::tool_clicked, this, _1));

	sizer->Add (_toolbar, 0, wxALL, 12);
	sizer->Add (&_timeline, 1, wxEXPAND | wxALL, 12);

#ifdef DCPOMATIC_LINUX
	auto buttons = CreateSeparatedButtonSizer (wxCLOSE);
	if (buttons) {
		sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}
#endif

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);

	Bind(wxEVT_CHAR_HOOK, boost::bind(&ContentTimelineDialog::keypress, this, _1));

        _toolbar->ToggleTool(static_cast<int>(ContentTimeline::SNAP), _timeline.snap ());
	film_change(ChangeType::DONE, FilmProperty::SEQUENCE);

	_film_changed_connection = film->Change.connect(bind(&ContentTimelineDialog::film_change, this, _1, _2));
}


void
ContentTimelineDialog::film_change(ChangeType type, FilmProperty p)
{
	if (type != ChangeType::DONE) {
		return;
	}

	auto film = _film.lock ();
	if (!film) {
		return;
	}

	if (p == FilmProperty::SEQUENCE) {
		_toolbar->ToggleTool(static_cast<int>(ContentTimeline::SEQUENCE), film->sequence());
	}
}


void
ContentTimelineDialog::set_selection(ContentList selection)
{
	_timeline.set_selection (selection);
}


void
ContentTimelineDialog::tool_clicked(wxCommandEvent& ev)
{
	auto t = static_cast<ContentTimeline::Tool>(ev.GetId());
	_timeline.tool_clicked (t);
	if (t == ContentTimeline::SNAP) {
		_timeline.set_snap (_toolbar->GetToolState(static_cast<int>(t)));
	} else if (t == ContentTimeline::SEQUENCE) {
		auto film = _film.lock ();
		if (film) {
			film->set_sequence (_toolbar->GetToolState(static_cast<int>(t)));
		}
	}
}


void
ContentTimelineDialog::keypress(wxKeyEvent const& event)
{
	_timeline.keypress(event);
}
