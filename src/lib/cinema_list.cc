/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#include "cinema.h"
#include "cinema_list.h"
#include "config.h"
#include "dcpomatic_assert.h"
#include "exceptions.h"
#include "screen.h"
#include "sqlite_statement.h"
#include "sqlite_transaction.h"
#include "util.h"
#include <dcp/certificate.h>
#include <sqlite3.h>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <numeric>


using std::function;
using std::pair;
using std::make_pair;
using std::string;
using std::vector;
using boost::optional;


CinemaList::CinemaList()
	: _cinemas("cinemas")
	, _screens("screens")
	, _trusted_devices("trusted_devices")
{
	setup_tables();
	setup(Config::instance()->cinemas_file());
}


CinemaList::CinemaList(boost::filesystem::path db_file)
	: _cinemas("cinemas")
	, _screens("screens")
	, _trusted_devices("trusted_devices")
{
	setup_tables();
	setup(db_file);
}


void
CinemaList::setup_tables()
{
	_cinemas.add_column("name", "TEXT");
	_cinemas.add_column("emails", "TEXT");
	_cinemas.add_column("notes", "TEXT");
	_cinemas.add_column("utc_offset_hour", "INTEGER");
	_cinemas.add_column("utc_offset_minute", "INTEGER");

	_screens.add_column("cinema", "INTEGER");
	_screens.add_column("name", "TEXT");
	_screens.add_column("notes", "TEXT");
	_screens.add_column("recipient", "TEXT");
	_screens.add_column("recipient_file", "TEXT");

	_trusted_devices.add_column("screen", "INTEGER");
	_trusted_devices.add_column("certificate_or_thumbprint", "TEXT");
}


void
CinemaList::read_legacy_file(boost::filesystem::path xml_file)
{
	cxml::Document doc("Cinemas");
	doc.read_file(xml_file);
	read_legacy_document(doc);
}


void
CinemaList::read_legacy_string(std::string const& xml)
{
	cxml::Document doc("Cinemas");
	doc.read_string(xml);
	read_legacy_document(doc);
}


void
CinemaList::read_legacy_document(cxml::Document const& doc)
{
	for (auto cinema_node: doc.node_children("Cinema")) {
		vector<string> emails;
		for (auto email_node: cinema_node->node_children("Email")) {
			emails.push_back(email_node->content());
		}

		int hour = 0;
		if (cinema_node->optional_number_child<int>("UTCOffset")) {
			hour = cinema_node->number_child<int>("UTCOffset");
		} else {
			hour = cinema_node->optional_number_child<int>("UTCOffsetHour").get_value_or(0);
		}

		int minute = cinema_node->optional_number_child<int>("UTCOffsetMinute").get_value_or(0);

		Cinema cinema(
			cinema_node->string_child("Name"),
			emails,
			cinema_node->string_child("Notes"),
			dcp::UTCOffset(hour, minute)
			);

		auto cinema_id = add_cinema(cinema);

		for (auto screen_node: cinema_node->node_children("Screen")) {
			optional<dcp::Certificate> recipient;
			if (auto recipient_string = screen_node->optional_string_child("Recipient")) {
				recipient = dcp::Certificate(*recipient_string);
			}
			vector<TrustedDevice> trusted_devices;
			for (auto trusted_device_node: screen_node->node_children("TrustedDevice")) {
				trusted_devices.push_back(TrustedDevice(trusted_device_node->content()));
			}
			dcpomatic::Screen screen(
				screen_node->string_child("Name"),
				screen_node->string_child("Notes"),
				recipient,
				screen_node->optional_string_child("RecipientFile"),
				trusted_devices
				);
			add_screen(cinema_id, screen);
		}
	}
}


void
CinemaList::clear()
{
	for (auto table: { "cinemas", "screens", "trusted_devices" }) {
		SQLiteStatement sql(_db, String::compose("DELETE FROM %1", table));
		sql.execute();
	}
}


void
CinemaList::setup(boost::filesystem::path db_file)
{
#ifdef DCPOMATIC_WINDOWS
	auto rc = sqlite3_open16(db_file.c_str(), &_db);
#else
	auto rc = sqlite3_open(db_file.c_str(), &_db);
#endif
	if (rc != SQLITE_OK) {
		throw FileError("Could not open SQLite database", db_file);
	}

	sqlite3_busy_timeout(_db, 500);

	SQLiteStatement cinemas(_db, _cinemas.create());
	cinemas.execute();

	SQLiteStatement screens(_db, _screens.create());
	screens.execute();

	SQLiteStatement devices(_db, _trusted_devices.create());
	devices.execute();
}


CinemaList::CinemaList(CinemaList&& other)
	: _db(other._db)
	, _cinemas(std::move(other._cinemas))
	, _screens(std::move(other._screens))
	, _trusted_devices(std::move(other._trusted_devices))
{
	other._db = nullptr;
}


CinemaList&
CinemaList::operator=(CinemaList&& other)
{
	if (this != &other) {
		_db = other._db;
		other._db = nullptr;
	}
	return *this;
}


CinemaID
CinemaList::add_cinema(Cinema const& cinema)
{
	SQLiteStatement statement(_db, _cinemas.insert());

	statement.bind_text(1, cinema.name);
	statement.bind_text(2, join_strings(cinema.emails));
	statement.bind_text(3, cinema.notes);
	statement.bind_int64(4, cinema.utc_offset.hour());
	statement.bind_int64(5, cinema.utc_offset.minute());

	statement.execute();

	return sqlite3_last_insert_rowid(_db);
}


void
CinemaList::update_cinema(CinemaID id, Cinema const& cinema)
{
	SQLiteStatement statement(_db, _cinemas.update("WHERE id=?"));

	statement.bind_text(1, cinema.name);
	statement.bind_text(2, join_strings(cinema.emails));
	statement.bind_text(3, cinema.notes);
	statement.bind_int64(4, cinema.utc_offset.hour());
	statement.bind_int64(5, cinema.utc_offset.minute());
	statement.bind_int64(6, id.get());

	statement.execute();
}


void
CinemaList::remove_cinema(CinemaID id)
{
	SQLiteStatement statement(_db, "DELETE FROM cinemas WHERE ID=?");
	statement.bind_int64(1, id.get());
	statement.execute();
}


CinemaList::~CinemaList()
{
	if (_db) {
		sqlite3_close(_db);
	}
}


static
vector<pair<CinemaID, Cinema>>
cinemas_from_result(SQLiteStatement& statement)
{
	vector<pair<CinemaID, Cinema>> output;

	statement.execute([&output](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 6);
		CinemaID const id = statement.column_int64(0);
		auto const name = statement.column_text(1);
		auto const join_strings = statement.column_text(2);
		vector<string> emails;
		boost::algorithm::split(emails, join_strings, boost::is_any_of(" "));
		auto const notes = statement.column_text(3);
		auto const utc_offset_hour = static_cast<int>(statement.column_int64(4));
		auto const utc_offset_minute = static_cast<int>(statement.column_int64(5));
		output.push_back(make_pair(id, Cinema(name, { emails }, notes, dcp::UTCOffset{utc_offset_hour, utc_offset_minute})));
	});

	return output;
}


vector<pair<CinemaID, Cinema>>
CinemaList::cinemas() const
{
	SQLiteStatement statement(_db, _cinemas.select("ORDER BY name ASC"));
	return cinemas_from_result(statement);
}


optional<Cinema>
CinemaList::cinema(CinemaID id) const
{
	SQLiteStatement statement(_db, _cinemas.select("WHERE id=?"));
	statement.bind_int64(1, id.get());
	auto result = cinemas_from_result(statement);
	if (result.empty()) {
		return {};
	}
	return result[0].second;
}

ScreenID
CinemaList::add_screen(CinemaID cinema_id, dcpomatic::Screen const& screen)
{
	SQLiteTransaction transaction(_db);

	SQLiteStatement add_screen(_db, _screens.insert());

	add_screen.bind_int64(1, cinema_id.get());
	add_screen.bind_text(2, screen.name);
	add_screen.bind_text(3, screen.notes);
	add_screen.bind_text(4, screen.recipient()->certificate(true));
	add_screen.bind_text(5, screen.recipient_file.get_value_or(""));

	add_screen.execute();

	auto const screen_id = sqlite3_last_insert_rowid(_db);

	for (auto device: screen.trusted_devices) {
		SQLiteStatement add_device(_db, _trusted_devices.insert());
		add_device.bind_int64(1, screen_id);
		add_device.bind_text(2, device.as_string());
		add_device.execute();
	}

	transaction.commit();

	return screen_id;
}


dcpomatic::Screen
CinemaList::screen_from_result(SQLiteStatement& statement, ScreenID screen_id, bool with_trusted_devices) const
{
	auto certificate_string = statement.column_text(4);
	optional<string> certificate = certificate_string.empty() ? optional<string>() : certificate_string;
	auto recipient_file_string = statement.column_text(5);
	optional<string> recipient_file = recipient_file_string.empty() ? optional<string>() : recipient_file_string;

	vector<TrustedDevice> trusted_devices;
	if (with_trusted_devices) {
		SQLiteStatement trusted_devices_statement(_db, _trusted_devices.select("WHERE screen=?"));
		trusted_devices_statement.bind_int64(1, screen_id.get());
		trusted_devices_statement.execute([&trusted_devices](SQLiteStatement& statement) {
			DCPOMATIC_ASSERT(statement.data_count() == 3);
			auto description = statement.column_text(2);
			if (boost::algorithm::starts_with(description, "-----BEGIN CERTIFICATE")) {
				trusted_devices.push_back(TrustedDevice(dcp::Certificate(description)));
			} else {
				trusted_devices.push_back(TrustedDevice(description));
			}
		});
	}

	return dcpomatic::Screen(statement.column_text(2), statement.column_text(3), certificate, recipient_file, trusted_devices);
}


optional<dcpomatic::Screen>
CinemaList::screen(ScreenID screen_id) const
{
	SQLiteStatement statement(_db, _screens.select("WHERE id=?"));
	statement.bind_int64(1, screen_id.get());

	optional<dcpomatic::Screen> output;

	statement.execute([this, &output, screen_id](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 6);
		output = screen_from_result(statement, screen_id, true);
	});

	return output;
}



vector<pair<ScreenID, dcpomatic::Screen>>
CinemaList::screens_from_result(SQLiteStatement& statement) const
{
	vector<pair<ScreenID, dcpomatic::Screen>> output;

	statement.execute([this, &output](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 6);
		ScreenID const screen_id = statement.column_int64(0);
		output.push_back({screen_id, screen_from_result(statement, screen_id, true)});
	});

	return output;
}


vector<pair<ScreenID, dcpomatic::Screen>>
CinemaList::screens(CinemaID cinema_id) const
{
	SQLiteStatement statement(_db, _screens.select("WHERE cinema=?"));
	statement.bind_int64(1, cinema_id.get());
	return screens_from_result(statement);
}


vector<pair<ScreenID, dcpomatic::Screen>>
CinemaList::screens_by_cinema_and_name(CinemaID id, std::string const& name) const
{
	SQLiteStatement statement(_db, _screens.select("WHERE cinema=? AND name=?"));
	statement.bind_int64(1, id.get());
	statement.bind_text(2, name);
	return screens_from_result(statement);
}


optional<std::pair<CinemaID, Cinema>>
CinemaList::cinema_by_name_or_email(std::string const& text) const
{
	SQLiteStatement statement(_db, _cinemas.select("WHERE name LIKE ? OR EMAILS LIKE ?"));
	auto const wildcard = string("%") + text + "%";
	statement.bind_text(1, wildcard);
	statement.bind_text(2, wildcard);

	auto all = cinemas_from_result(statement);
	if (all.empty()) {
		return {};
	}
	return all[0];
}


void
CinemaList::update_screen(CinemaID cinema_id, ScreenID screen_id, dcpomatic::Screen const& screen)
{
	SQLiteTransaction transaction(_db);

	SQLiteStatement statement(_db, _screens.update("WHERE id=?"));

	statement.bind_int64(1, cinema_id.get());
	statement.bind_text(2, screen.name);
	statement.bind_text(3, screen.notes);
	statement.bind_text(4, screen.recipient()->certificate(true));
	statement.bind_text(5, screen.recipient_file.get_value_or(""));
	statement.bind_int64(6, screen_id.get());
	statement.execute();

	SQLiteStatement remove_existing(_db, "DELETE FROM trusted_devices WHERE screen=?");
	remove_existing.bind_int64(1, screen_id.get());
	remove_existing.execute();

	for (auto device: screen.trusted_devices) {
		SQLiteStatement add_device(_db, _trusted_devices.insert());
		add_device.bind_int64(1, screen_id.get());
		add_device.bind_text(2, device.as_string());
		add_device.execute();
	}

	transaction.commit();
}


void
CinemaList::remove_screen(ScreenID id)
{
	SQLiteStatement statement(_db, "DELETE FROM screens WHERE ID=?");
	statement.bind_int64(1, id.get());
	statement.execute();
}


optional<dcp::UTCOffset>
CinemaList::unique_utc_offset(std::set<CinemaID> const& cinemas_to_check)
{
	optional<dcp::UTCOffset> offset;

	for (auto const& cinema: cinemas()) {
		if (cinemas_to_check.find(cinema.first) == cinemas_to_check.end()) {
			continue;
		}

		if (!offset) {
			offset = cinema.second.utc_offset;
		} else if (cinema.second.utc_offset != *offset) {
			return dcp::UTCOffset();
		}
	}

	return offset;
}


void
CinemaList::screens(function<void (CinemaID, ScreenID, dcpomatic::Screen const& screen)> callback, bool with_trusted_devices) const
{
	SQLiteStatement statement(_db, _screens.select(""));
	statement.execute([this, &callback, with_trusted_devices](SQLiteStatement& statement) {
		auto const screen_id = statement.column_int64(0);
		callback(statement.column_int64(1), screen_id, screen_from_result(statement, screen_id, with_trusted_devices));
	});

}

