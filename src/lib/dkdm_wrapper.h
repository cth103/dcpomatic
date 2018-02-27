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

#include <dcp/encrypted_kdm.h>
#include <libcxml/cxml.h>
#include <boost/enable_shared_from_this.hpp>

namespace xmlpp {
	class Element;
}

class DKDMGroup;

class DKDMBase : public boost::enable_shared_from_this<DKDMBase>
{
public:
	virtual ~DKDMBase () {}
	virtual std::string name () const = 0;
	virtual void as_xml (xmlpp::Element *) const = 0;

	static boost::shared_ptr<DKDMBase> read (cxml::ConstNodePtr node);

	boost::shared_ptr<DKDMGroup> parent () const {
		return _parent;
	}

	void set_parent (boost::shared_ptr<DKDMGroup> parent) {
		_parent = parent;
	}

private:
	boost::shared_ptr<DKDMGroup> _parent;
};

class DKDM : public DKDMBase
{
public:
	explicit DKDM (dcp::EncryptedKDM k)
		: _dkdm (k)
	{}

	std::string name () const;
	void as_xml (xmlpp::Element *) const;

	dcp::EncryptedKDM dkdm () const {
		return _dkdm;
	}

private:
	dcp::EncryptedKDM _dkdm;
};

class DKDMGroup : public DKDMBase
{
public:
	explicit DKDMGroup (std::string name)
		: _name (name)
	{}

	std::string name () const {
		return _name;
	}

	void as_xml (xmlpp::Element *) const;

	std::list<boost::shared_ptr<DKDMBase> > children () const {
		return _children;
	}

	void add (boost::shared_ptr<DKDMBase> child, boost::shared_ptr<DKDM> previous = boost::shared_ptr<DKDM> ());
        void remove (boost::shared_ptr<DKDMBase> child);

private:
	std::string _name;
	std::list<boost::shared_ptr<DKDMBase> > _children;
};
