/*
    Copyright (C) 2017-2021 Carl Hetherington <cth@carlh.net>

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
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS


using std::string;
using std::list;
using std::shared_ptr;
using std::make_shared;
using std::dynamic_pointer_cast;


shared_ptr<DKDMBase>
DKDMBase::read (cxml::ConstNodePtr node)
{
	if (node->name() == "DKDM") {
		return make_shared<DKDM>(dcp::EncryptedKDM(node->content()));
	} else if (node->name() == "DKDMGroup") {
		auto group = make_shared<DKDMGroup>(node->string_attribute("Name"));
		for (auto i: node->node_children()) {
			if (auto c = read(i)) {
				group->add (c);
			}
		}
		return group;
	}

	return {};
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
	auto f = node->add_child("DKDMGroup");
	f->set_attribute ("Name", _name);
	for (auto i: _children) {
		i->as_xml (f);
	}
}


void
DKDMGroup::add (shared_ptr<DKDMBase> child, shared_ptr<DKDM> previous)
{
	DCPOMATIC_ASSERT (child);
	if (previous) {
		auto i = find (_children.begin(), _children.end(), previous);
		if (i != _children.end()) {
			++i;
		}
		_children.insert (i, child);
	} else {
		_children.push_back (child);
	}
	child->set_parent (dynamic_pointer_cast<DKDMGroup>(shared_from_this()));
}


void
DKDMGroup::remove (shared_ptr<DKDMBase> child)
{
	for (auto i = _children.begin(); i != _children.end(); ++i) {
		if (*i == child) {
			_children.erase (i);
			child->set_parent (shared_ptr<DKDMGroup>());
			return;
		}
		auto g = dynamic_pointer_cast<DKDMGroup> (*i);
		if (g) {
			g->remove (child);
		}
	}
}


bool
DKDMGroup::contains(string dkdm_id) const
{
	for (auto child: _children) {
		if (auto child_group = dynamic_pointer_cast<DKDMGroup>(child)) {
			if (child_group->contains(dkdm_id)) {
				return true;
			}
		} else if (auto child_dkdm = dynamic_pointer_cast<DKDM>(child)) {
			if (child_dkdm->dkdm().id() == dkdm_id) {
				return true;
			}
		}
	}

	return false;
}

