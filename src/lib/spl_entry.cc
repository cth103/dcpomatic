/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#include "dcp_content.h"
#include "dcpomatic_assert.h"
#include "spl_entry.h"
#include <dcp/warnings.h>
#include <fmt/format.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS


using std::dynamic_pointer_cast;
using std::shared_ptr;


SPLEntry::SPLEntry(shared_ptr<Content> c, cxml::ConstNodePtr node)
	: content(c)
	, digest(content->digest())
{
	if (auto dcp = dynamic_pointer_cast<DCPContent>(content)) {
		name = dcp->name();
		DCPOMATIC_ASSERT(dcp->cpl());
		id = dcp->cpl();
		kind = dcp->content_kind().get_value_or(dcp::ContentKind::FEATURE);
		encrypted = dcp->encrypted();
	} else {
		name = content->path(0).filename().string();
		kind = dcp::ContentKind::FEATURE;
	}

	if (node) {
		if (auto crop = node->optional_number_child<float>("CropToRatio")) {
			crop_to_ratio = *crop;
		}
	}
}


void
SPLEntry::as_xml(xmlpp::Element* e) const
{
	if (id) {
		cxml::add_text_child(e, "CPL", *id);
	} else {
		cxml::add_text_child(e, "Digest", digest);
	}
	if (crop_to_ratio) {
		cxml::add_text_child(e, "CropToRatio", fmt::to_string(*crop_to_ratio));
	}
}
