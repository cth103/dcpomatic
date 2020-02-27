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

#ifndef DCPOMATIC_SWAROOP_SPL_ENTRY_H
#define DCPOMATIC_SWAROOP_SPL_ENTRY_H

#include <libcxml/cxml.h>
#include <dcp/types.h>
#include <boost/shared_ptr.hpp>

namespace xmlpp {
	class Element;
}

class Content;

class SPLEntry
{
public:
	SPLEntry (boost::shared_ptr<Content> content);
	SPLEntry (boost::shared_ptr<Content> content, cxml::ConstNodePtr node);

	void as_xml (xmlpp::Element* e);

	boost::shared_ptr<Content> content;
	std::string name;
	/** Digest of this content */
	std::string digest;
	/** CPL ID or something else for MP4 (?) */
	std::string id;
	dcp::ContentKind kind;
	enum Type {
		DCP,
		ECINEMA
	};
	Type type;
	bool encrypted;
	bool skippable;
	bool disable_timeline;
	bool stop_after_play;

private:
	void construct (boost::shared_ptr<Content> content);
};

#endif
