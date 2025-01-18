/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_DKDM_RECIPIENT_LIST_H
#define DCPOMATIC_DKDM_RECIPIENT_LIST_H


#include "id.h"
#include "sqlite_database.h"
#include "sqlite_table.h"
#include <libcxml/cxml.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>


class DKDMRecipient;


class DKDMRecipientID : public ID
{
public:
	DKDMRecipientID(sqlite3_int64 id)
		: ID(id) {}

	bool operator==(DKDMRecipientID const& other) const {
		return get() == other.get();
	}

	bool operator!=(DKDMRecipientID const& other) const {
		return get() != other.get();
	}

	bool operator<(DKDMRecipientID const& other) const {
		return get() < other.get();
	}
};


class DKDMRecipientList
{
public:
	DKDMRecipientList();
	DKDMRecipientList(boost::filesystem::path db_file);

	DKDMRecipientList(DKDMRecipientList const&) = delete;
	DKDMRecipientList& operator=(DKDMRecipientList const&) = delete;

	void read_legacy_file(boost::filesystem::path xml_file);
	void read_legacy_string(std::string const& xml);

	void clear();

	DKDMRecipientID add_dkdm_recipient(DKDMRecipient const& dkdm_recipient);
	void update_dkdm_recipient(DKDMRecipientID id, DKDMRecipient const& dkdm_recipient);
	void remove_dkdm_recipient(DKDMRecipientID id);
	std::vector<std::pair<DKDMRecipientID, DKDMRecipient>> dkdm_recipients() const;
	boost::optional<DKDMRecipient> dkdm_recipient(DKDMRecipientID id) const;

private:
	void setup();
	void read_legacy_document(cxml::Document const& doc);

	SQLiteTable _dkdm_recipients;
	mutable SQLiteDatabase _db;
};


#endif

