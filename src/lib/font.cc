/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_assert.h"
#include "font.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS


using std::string;
using boost::optional;
using namespace dcpomatic;


Font::Font (cxml::NodePtr node)
	: _id (node->string_child("Id"))
{
	for (auto i: node->node_children("File")) {
		string variant = i->optional_string_attribute("Variant").get_value_or("Normal");
		if (variant == "Normal") {
			_content.file = i->content();
		}
	}
}


Font::Font(Font const& other)
	: _id(other._id)
	, _content(other._content)
{

}


Font& Font::operator=(Font const& other)
{
	if (&other != this) {
		_id = other._id;
		_content = other._content;
	}
	return *this;
}


void
Font::as_xml(xmlpp::Element* element)
{
	cxml::add_text_child(element, "Id", _id);
	if (_content.file) {
		cxml::add_text_child(element, "File", _content.file->string());
	}
}


bool
dcpomatic::operator== (Font const & a, Font const & b)
{
	if (a.id() != b.id()) {
		return false;
	}

	/* XXX: it's dubious that this ignores _data, though I think it's OK for the cases
	 * where operator== is used.  Perhaps we should remove operator== and have a more
	 * specific comparator.
	 */

	return a.file() == b.file();
}


bool
dcpomatic::operator!= (Font const & a, Font const & b)
{
	return !(a == b);
}


optional<dcp::ArrayData>
Font::data () const
{
	if (_content.data) {
		return _content.data;
	}

	if (_content.file) {
		return dcp::ArrayData(*_content.file);
	}

	return {};
}

