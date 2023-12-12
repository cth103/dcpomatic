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


#include "dcp_panel.h"
#include "dcp_timeline_dialog.h"
#include "film_editor.h"
#include "wx_util.h"
#include "lib/compose.hpp"
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


DCPTimelineDialog::DCPTimelineDialog(wxWindow* parent, shared_ptr<Film> film)
	: wxDialog(
		parent,
		wxID_ANY,
		_("Reels"),
		wxDefaultPosition,
		wxSize(640, 512),
#ifdef DCPOMATIC_OSX
		/* I can't get wxFRAME_FLOAT_ON_PARENT to work on OS X, and although wxSTAY_ON_TOP keeps
		   the window above all others (and not just our own) it's better than nothing for now.
		*/
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxFULL_REPAINT_ON_RESIZE | wxSTAY_ON_TOP
#else
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxFULL_REPAINT_ON_RESIZE | wxFRAME_FLOAT_ON_PARENT
#endif
		)
	, _timeline(this, film)
{
	auto sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add (&_timeline, 1, wxEXPAND | wxALL, 12);

#ifdef DCPOMATIC_LINUX
	auto buttons = CreateSeparatedButtonSizer (wxCLOSE);
	if (buttons) {
		sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}
#endif

	SetSizer(sizer);
	sizer->Layout();
	sizer->SetSizeHints(this);
}

