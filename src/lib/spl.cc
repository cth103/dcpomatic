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


#include "content_store.h"
#include "spl.h"
#include <libcxml/cxml.h>
#include <dcp/filesystem.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <iostream>


using std::cout;
using std::string;
using std::shared_ptr;


void
SPL::read (boost::filesystem::path path, ContentStore* store)
{
	_spl.clear ();
	_missing = false;
	cxml::Document doc ("SPL");
	doc.read_file(dcp::filesystem::fix_long_path(path));
	_id = doc.string_child("Id");
	_name = doc.string_child("Name");
	for (auto i: doc.node_children("Entry")) {
		if (auto cpl = i->optional_string_child("CPL")) {
			if (auto c = store->get_by_cpl_id(*cpl)) {
				add(SPLEntry(c, i));
			} else {
				_missing = true;
			}
		} else {
			if (auto c = store->get_by_digest(i->string_child("Digest"))) {
				add(SPLEntry(c, i));
			} else {
				_missing = true;
			}
		}
	}
}


void
SPL::write (boost::filesystem::path path) const
{
	xmlpp::Document doc;
	auto root = doc.create_root_node ("SPL");
	cxml::add_text_child(root, "Id", _id);
	cxml::add_text_child(root, "Name", _name);
	for (auto i: _spl) {
		i.as_xml(cxml::add_child(root, "Entry"));
	}
	doc.write_to_file_formatted (path.string());
}
