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


#ifndef DCPOMATIC_SPL_ENTRY_H
#define DCPOMATIC_SPL_ENTRY_H


#include <libcxml/cxml.h>
#include <dcp/content_kind.h>


namespace xmlpp {
	class Element;
}

class Content;


class SPLEntry
{
public:
	SPLEntry (std::shared_ptr<Content> c);

	void as_xml (xmlpp::Element* e);

	std::shared_ptr<Content> content;
	std::string name;
	/** Digest of this content */
	std::string digest;
	/** CPL ID */
	std::string id;
	boost::optional<dcp::ContentKind> kind;
	bool encrypted;

private:
	void construct (std::shared_ptr<Content> content);
};


#endif
