/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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

#include "compose.hpp"
#include "dkdm_wrapper.h"
#include "dcpomatic_assert.h"
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>

using std::string;
using std::list;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

shared_ptr<DKDMBase>
DKDMBase::read (cxml::ConstNodePtr node)
{
	if (node->name() == "DKDM") {
		return shared_ptr<DKDM> (new DKDM (dcp::EncryptedKDM (node->content ())));
	} else if (node->name() == "DKDMGroup") {
		shared_ptr<DKDMGroup> group (new DKDMGroup (node->string_attribute ("Name")));
		BOOST_FOREACH (cxml::ConstNodePtr i, node->node_children()) {
			shared_ptr<DKDMBase> c = read (i);
			if (c) {
				group->add (c);
			}
		}
		return group;
	}

	return shared_ptr<DKDMBase> ();
}

string
DKDM::name () const
{
	return String::compose ("%1 (%2)", _dkdm.content_title_text(), _dkdm.cpl_id());
}

void
DKDM::as_xml (xmlpp::Element* node) const
{
	node->add_child("DKDM")->add_child_text (_dkdm.as_xml ());
}

void
DKDMGroup::as_xml (xmlpp::Element* node) const
{
	xmlpp::Element* f = node->add_child("DKDMGroup");
	f->set_attribute ("Name", _name);
	BOOST_FOREACH (shared_ptr<DKDMBase> i, _children) {
		i->as_xml (f);
	}
}

void
DKDMGroup::add (shared_ptr<DKDMBase> child)
{
	DCPOMATIC_ASSERT (child);
	_children.push_back (child);
}

void
DKDMGroup::remove (shared_ptr<DKDMBase> child)
{
	for (list<shared_ptr<DKDMBase> >::iterator i = _children.begin(); i != _children.end(); ++i) {
		if (*i == child) {
			_children.erase (i);
			return;
		}
		shared_ptr<DKDMGroup> g = dynamic_pointer_cast<DKDMGroup> (*i);
		if (g) {
			g->remove (child);
		}
	}
}
