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


#include "config.h"
#include "dkdm_recipient.h"
#include "dkdm_recipient_list.h"
#include "sqlite_statement.h"
#include "sqlite_transaction.h"
#include "util.h"
#include <boost/algorithm/string.hpp>


using std::make_pair;
using std::pair;
using std::string;
using std::vector;
using boost::optional;


DKDMRecipientList::DKDMRecipientList()
	: _dkdm_recipients("dkdm_recipients")
	, _db(Config::instance()->dkdm_recipients_file())
{
	setup();
}


DKDMRecipientList::DKDMRecipientList(boost::filesystem::path db_file)
	: _dkdm_recipients("dkdm_recipients")
	, _db(db_file)
{
	setup();
}



void
DKDMRecipientList::read_legacy_file(boost::filesystem::path xml_file)
{
	cxml::Document doc("DKDMRecipients");
	doc.read_file(xml_file);

	read_legacy_document(doc);
}


void
DKDMRecipientList::read_legacy_string(string const& xml)
{
	cxml::Document doc("DKDMRecipients");
	doc.read_file(xml);

	read_legacy_document(doc);
}


void
DKDMRecipientList::read_legacy_document(cxml::Document const& doc)
{
	for (auto recipient_node: doc.node_children("DKDMRecipient")) {
		vector<string> emails;
		for (auto email_node: recipient_node->node_children("Email")) {
			emails.push_back(email_node->content());
		}

		optional<dcp::Certificate> certificate;
		if (auto certificate_string = recipient_node->optional_string_child("Recipient")) {
			certificate = dcp::Certificate(*certificate_string);
		}

		DKDMRecipient recipient(
			recipient_node->string_child("Name"),
			recipient_node->string_child("Notes"),
			certificate,
			emails
			);

		add_dkdm_recipient(recipient);
	}
}


void
DKDMRecipientList::setup()
{
	_dkdm_recipients.add_column("name", "TEXT");
	_dkdm_recipients.add_column("notes", "TEXT");
	_dkdm_recipients.add_column("recipient", "TEXT");
	_dkdm_recipients.add_column("emails", "TEXT");

	SQLiteStatement screens(_db, _dkdm_recipients.create());
	screens.execute();
}


DKDMRecipientID
DKDMRecipientList::add_dkdm_recipient(DKDMRecipient const& dkdm_recipient)
{
	SQLiteStatement add_dkdm_recipient(_db, _dkdm_recipients.insert());

	add_dkdm_recipient.bind_text(1, dkdm_recipient.name);
	add_dkdm_recipient.bind_text(2, dkdm_recipient.notes);
	add_dkdm_recipient.bind_text(3, dkdm_recipient.recipient() ? dkdm_recipient.recipient()->certificate(true) : "");
	add_dkdm_recipient.bind_text(4, join_strings(dkdm_recipient.emails));

	add_dkdm_recipient.execute();

	return sqlite3_last_insert_rowid(_db.db());
}


void
DKDMRecipientList::update_dkdm_recipient(DKDMRecipientID id, DKDMRecipient const& dkdm_recipient)
{
	SQLiteStatement add_dkdm_recipient(_db, _dkdm_recipients.update("WHERE id=?"));

	add_dkdm_recipient.bind_text(1, dkdm_recipient.name);
	add_dkdm_recipient.bind_text(2, dkdm_recipient.notes);
	add_dkdm_recipient.bind_text(3, dkdm_recipient.recipient() ? dkdm_recipient.recipient()->certificate(true) : "");
	add_dkdm_recipient.bind_text(4, join_strings(dkdm_recipient.emails));
	add_dkdm_recipient.bind_int64(5, id.get());

	add_dkdm_recipient.execute();
}


void
DKDMRecipientList::remove_dkdm_recipient(DKDMRecipientID id)
{
	SQLiteStatement statement(_db, "DELETE FROM dkdm_recipients WHERE ID=?");
	statement.bind_int64(1, id.get());
	statement.execute();
}


static
vector<pair<DKDMRecipientID, DKDMRecipient>>
dkdm_recipients_from_result(SQLiteStatement& statement)
{
	vector<pair<DKDMRecipientID, DKDMRecipient>> output;

	statement.execute([&output](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 5);
		DKDMRecipientID const id = statement.column_int64(0);
		auto const name = statement.column_text(1);
		auto const notes = statement.column_text(2);
		auto certificate_string = statement.column_text(3);
		optional<dcp::Certificate> certificate = certificate_string.empty() ? optional<dcp::Certificate>() : dcp::Certificate(certificate_string);
		auto const join_with_spaces = statement.column_text(4);
		vector<string> emails;
		boost::algorithm::split(emails, join_with_spaces, boost::is_any_of(" "));
		output.push_back(make_pair(id, DKDMRecipient(name, notes, certificate, { emails })));
	});

	return output;
}




vector<std::pair<DKDMRecipientID, DKDMRecipient>>
DKDMRecipientList::dkdm_recipients() const
{
	SQLiteStatement statement(_db, _dkdm_recipients.select("ORDER BY name COLLATE unicode ASC"));
	return dkdm_recipients_from_result(statement);
}


boost::optional<DKDMRecipient>
DKDMRecipientList::dkdm_recipient(DKDMRecipientID id) const
{
	SQLiteStatement statement(_db, _dkdm_recipients.select("WHERE id=?"));
	statement.bind_int64(1, id.get());
	auto result = dkdm_recipients_from_result(statement);
	if (result.empty()) {
		return {};
	}
	return result[0].second;
}


void
DKDMRecipientList::clear()
{
	SQLiteStatement sql(_db, "DELETE FROM dkdm_recipients");
	sql.execute();
}


