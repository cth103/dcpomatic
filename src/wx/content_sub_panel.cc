/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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


#include "content_sub_panel.h"
#include "content_panel.h"
#include "wx_util.h"
#include "lib/dcp_content.h"
#include "lib/compose.hpp"
#include "lib/log.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/notebook.h>
LIBDCP_ENABLE_WARNINGS


using std::list;
using std::shared_ptr;
using std::string;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


ContentSubPanel::ContentSubPanel (ContentPanel* p, wxString name)
	: wxScrolledWindow (p->notebook(), wxID_ANY)
	, _parent (p)
	, _sizer (new wxBoxSizer (wxVERTICAL))
	, _name (name)
{
	SetScrollRate (8, 8);
	SetSizer (_sizer);

	_grid = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	_sizer->Add (_grid, 0, wxALL, 8);
}

void
ContentSubPanel::layout ()
{
	int x;
	int y;
	GetViewStart (&x, &y);
	Scroll (0, 0);
	_sizer->Layout ();
	_sizer->FitInside(this);
	Scroll (x, y);
}

