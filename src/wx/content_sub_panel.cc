/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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
#include <wx/notebook.h>
#include <boost/foreach.hpp>

using std::list;
using std::string;
using boost::shared_ptr;

ContentSubPanel::ContentSubPanel (ContentPanel* p, wxString name)
	: wxScrolledWindow (p->notebook(), wxID_ANY)
	, _parent (p)
	, _sizer (new wxBoxSizer (wxVERTICAL))
	, _name (name)
{
	p->notebook()->AddPage (this, _name, false);
	SetScrollRate (-1, 8);
	SetSizer (_sizer);
}

void
ContentSubPanel::setup_refer_button (wxCheckBox* button, wxStaticText* note, shared_ptr<DCPContent> dcp, bool can_reference, string why_not) const
{
	button->Enable (can_reference);

	wxString s;
	if (dcp && !can_reference) {
		if (why_not.empty()) {
			s = _("Cannot reference this DCP.");
		} else {
			s = _("Cannot reference this DCP: ") + std_to_wx(why_not);
		}
	}

	note->SetLabel (s);
	note->Wrap (400);

	if (s.IsEmpty ()) {
		note->Hide ();
	} else {
		note->Show ();
	}

	_sizer->Layout ();
}
