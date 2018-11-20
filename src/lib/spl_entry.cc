/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "spl_entry.h"
#include "dcp_content.h"
#include "dcpomatic_assert.h"
#include <libxml++/libxml++.h>

using boost::shared_ptr;
using boost::dynamic_pointer_cast;

SPLEntry::SPLEntry (shared_ptr<Content> content)
	: skippable (false)
	, disable_timeline (false)
	, stop_after_play (false)
{
	construct (content);
}

SPLEntry::SPLEntry (shared_ptr<Content> content, cxml::ConstNodePtr node)
	: skippable (node->bool_child("Skippable"))
	, disable_timeline (node->bool_child("DisableTimeline"))
	, stop_after_play (node->bool_child("StopAfterPlay"))
{
	construct (content);
}

void
SPLEntry::construct (shared_ptr<Content> c)
{
	content = c;
	shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent> (content);
	digest = content->digest ();
	if (dcp) {
		name = dcp->name ();
		DCPOMATIC_ASSERT (dcp->cpl());
		id = *dcp->cpl();
		kind = dcp->content_kind().get_value_or(dcp::FEATURE);
		type = DCP;
		encrypted = dcp->encrypted ();
	} else {
		name = content->path(0).filename().string();
		type = ECINEMA;
		kind = dcp::FEATURE;
	}
}

void
SPLEntry::as_xml (xmlpp::Element* e)
{
	e->add_child("Digest")->add_child_text(digest);
	e->add_child("Skippable")->add_child_text(skippable ? "1" : "0");
	e->add_child("DisableTimeline")->add_child_text(disable_timeline ? "1" : "0");
	e->add_child("StopAfterPlay")->add_child_text(stop_after_play ? "1" : "0");
}
