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


#include <dcp/encrypted_kdm.h>
#include <libcxml/cxml.h>
#include <memory>


namespace xmlpp {
	class Element;
}


class DKDMGroup;


class DKDMBase : public std::enable_shared_from_this<DKDMBase>
{
public:
	virtual ~DKDMBase() {}
	virtual std::string name() const = 0;
	virtual void as_xml(xmlpp::Element *) const = 0;
	/** @return true if this thing is, or contains, any actual DKDM */
	virtual bool contains_dkdm() const = 0;
	virtual std::vector<dcp::EncryptedKDM> all_dkdms() const = 0;

	static std::shared_ptr<DKDMBase> read(cxml::ConstNodePtr node);

	std::shared_ptr<DKDMGroup> parent() const {
		return _parent;
	}

	void set_parent(std::shared_ptr<DKDMGroup> parent) {
		_parent = parent;
	}

private:
	std::shared_ptr<DKDMGroup> _parent;
};


class DKDM : public DKDMBase
{
public:
	explicit DKDM(dcp::EncryptedKDM k)
		: _dkdm(k)
	{}

	std::string name() const override;
	void as_xml(xmlpp::Element *) const override;
	bool contains_dkdm() const override {
		return true;
	}
	std::vector<dcp::EncryptedKDM> all_dkdms() const override {
		return { _dkdm };
	}

	dcp::EncryptedKDM dkdm() const {
		return _dkdm;
	}

private:
	dcp::EncryptedKDM _dkdm;
};


class DKDMGroup : public DKDMBase
{
public:
	explicit DKDMGroup(std::string name)
		: _name(name)
	{}

	std::string name() const override {
		return _name;
	}

	void as_xml(xmlpp::Element *) const override;

	bool contains_dkdm() const override;

	std::vector<dcp::EncryptedKDM> all_dkdms() const override;

	std::list<std::shared_ptr<DKDMBase>> children() const {
		return _children;
	}

	void add(std::shared_ptr<DKDMBase> child, std::shared_ptr<DKDM> previous = std::shared_ptr<DKDM>());
        void remove(std::shared_ptr<DKDMBase> child);

	bool contains(std::string dkdm_id) const;

private:
	std::string _name;
	std::list<std::shared_ptr<DKDMBase>> _children;
};
