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
	: wxPanel (p->notebook(), wxID_ANY)
	, _parent (p)
	, _sizer (new wxBoxSizer (wxVERTICAL))
{
	p->notebook()->AddPage (this, name, false);
	SetSizer (_sizer);
}

void
ContentSubPanel::setup_refer_button (wxCheckBox* button, shared_ptr<DCPContent> dcp, bool can_reference, list<string> why_not) const
{
	/* XXX: just for debugging */
	if (dcp) {
		dcp->film()->log()->log (
			String::compose (
				"setup_refer_button for %1; %2 %3",
				dcp->path(0).string(),
				int(can_reference),
				int(why_not.size())),
			LogEntry::TYPE_GENERAL
			);
	}

	button->Enable (can_reference);

	wxString s;
	if (!dcp) {
		s = _("No DCP selected.");
	} else if (!can_reference) {
		s = _("Cannot reference this DCP.  ");
		BOOST_FOREACH (string i, why_not) {
			s += std_to_wx(i) + wxT("  ");
		}
	}

	/* XXX: more debugging */
	if (dcp) {
		if (!s.IsEmpty()) {
			dcp->film()->log()->log (String::compose ("setting tooltip to %1", wx_to_std (s)), LogEntry::TYPE_GENERAL);
		} else {
			dcp->film()->log()->log ("clearing tooltip", LogEntry::TYPE_GENERAL);
		}
	}

	button->SetToolTip (s);
}
