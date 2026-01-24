/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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
#include "show_playlist.h"
#include "show_playlist_content_store.h"
#include "show_playlist_entry.h"
#include "show_playlist_list.h"
#include "sqlite_statement.h"
#include "sqlite_transaction.h"
#include <dcp/filesystem.h>


using std::make_pair;
using std::pair;
using std::string;
using std::vector;
using boost::optional;


ShowPlaylistList::ShowPlaylistList()
	: _show_playlists("show_playlists")
	, _entries("entries")
	, _db(Config::instance()->show_playlists_file())
{
	setup_tables();
	setup();
}


ShowPlaylistList::ShowPlaylistList(boost::filesystem::path db_file)
	: _show_playlists("show_playlists")
	, _entries("entries")
	, _db(db_file)
{
	setup_tables();
	setup();
}


void
ShowPlaylistList::setup_tables()
{
	_show_playlists.add_column("uuid", "TEXT");
	_show_playlists.add_column("name", "TEXT");

	_entries.add_column("show_playlist", "INTEGER");
	_entries.add_column("uuid", "TEXT");
	_entries.add_column("name", "TEXT");
	_entries.add_column("kind", "TEXT");
	_entries.add_column("approximate_length", "TEXT");
	_entries.add_column("encrypted", "INTEGER");
	_entries.add_column("crop_to_ratio", "REAL");
	_entries.add_column("sort_index", "INTEGER");
}


void
ShowPlaylistList::setup()
{
	SQLiteStatement show_playlists(_db, _show_playlists.create());
	show_playlists.execute();

	SQLiteStatement entries(_db, _entries.create());
	entries.execute();
}


ShowPlaylistID
ShowPlaylistList::add_show_playlist(ShowPlaylist const& playlist)
{
	SQLiteStatement statement(_db, _show_playlists.insert());

	statement.bind_text(1, playlist.uuid());
	statement.bind_text(2, playlist.name());

	statement.execute();

	return ShowPlaylistID(sqlite3_last_insert_rowid(_db.db()));
}


void
ShowPlaylistList::update_show_playlist(ShowPlaylistID id, ShowPlaylist const& playlist)
{
	SQLiteStatement statement(_db, _show_playlists.update("WHERE id=?"));

	statement.bind_text(1, playlist.uuid());
	statement.bind_text(2, playlist.name());
	statement.bind_int64(3, id.get());

	statement.execute();
}


void
ShowPlaylistList::remove_show_playlist(ShowPlaylistID id)
{
	SQLiteStatement statement(_db, "DELETE FROM show_playlists WHERE ID=?");
	statement.bind_int64(1, id.get());
	statement.execute();
}


static
vector<pair<ShowPlaylistID, ShowPlaylist>>
show_playlists_from_result(SQLiteStatement& statement)
{
	vector<pair<ShowPlaylistID, ShowPlaylist>> output;

	statement.execute([&output](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 3);
		ShowPlaylistID const id(statement.column_int64(0));
		auto const uuid = statement.column_text(1);
		auto const name = statement.column_text(2);
		output.push_back(make_pair(id, ShowPlaylist(uuid, name)));
	});

	return output;
}


vector<pair<ShowPlaylistID, ShowPlaylist>>
ShowPlaylistList::show_playlists() const
{
	SQLiteStatement statement(_db, _show_playlists.select("ORDER BY name COLLATE unicode ASC"));
	return show_playlists_from_result(statement);
}


optional<ShowPlaylist>
ShowPlaylistList::show_playlist(ShowPlaylistID id) const
{
	SQLiteStatement statement(_db, _show_playlists.select("WHERE id=?"));
	statement.bind_int64(1, id.get());
	auto results = show_playlists_from_result(statement);
	if (results.empty()) {
		return {};
	}

	return results[0].second;
}



vector<ShowPlaylistEntry>
ShowPlaylistList::entries(std::string const& where, std::function<void (SQLiteStatement&)> bind) const
{
	SQLiteStatement statement(
		_db,
		fmt::format(
			"SELECT entries.uuid,entries.name,entries.kind,entries.approximate_length,entries.encrypted,entries.crop_to_ratio "
			"FROM entries "
			"JOIN show_playlists ON entries.show_playlist=show_playlists.id "
			"{} ORDER BY entries.sort_index", where
		)
	);

	bind(statement);

	vector<ShowPlaylistEntry> output;

	statement.execute([&output](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 6);
		output.push_back(
			ShowPlaylistEntry(
				statement.column_text(0),
				statement.column_text(1),
				dcp::ContentKind::from_name(statement.column_text(2)),
				statement.column_text(3),
				statement.column_int64(4),
				statement.column_double(5) > 0 ? optional<float>(statement.column_double(5)) : optional<float>()
			)
		);
	});

	return output;
}


vector<ShowPlaylistEntry>
ShowPlaylistList::entries(ShowPlaylistID show_playlist_id) const
{
	return entries("WHERE show_playlists.id=?", [show_playlist_id](SQLiteStatement& statement) { statement.bind_int64(1, show_playlist_id.get()); });
}


vector<ShowPlaylistEntry>
ShowPlaylistList::entries(string const& show_playlist_uuid) const
{
	return entries("WHERE show_playlists.uuid=?", [show_playlist_uuid](SQLiteStatement& statement) { statement.bind_text(1, show_playlist_uuid); });
}


void
ShowPlaylistList::add_entry(ShowPlaylistID playlist_id, ShowPlaylistEntry const& entry)
{
	SQLiteTransaction transaction(_db);

	SQLiteStatement find_last_entry(_db, "SELECT MAX(sort_index) FROM entries WHERE show_playlist=?");
	find_last_entry.bind_int64(1, playlist_id.get());

	int highest_index = 0;
	find_last_entry.execute([&highest_index](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 1);
		highest_index = statement.column_int64(0);
	});

	SQLiteStatement count_entries(_db, "SELECT COUNT(id) FROM entries WHERE show_playlist=?");
	count_entries.bind_int64(1, playlist_id.get());

	count_entries.execute([&highest_index](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 1);
		if (statement.column_int64(0) == 0) {
			highest_index = -1;
		}
	});

	SQLiteStatement add_entry(_db, _entries.insert());

	add_entry.bind_int64(1, playlist_id.get());
	add_entry.bind_text(2, entry.uuid());
	add_entry.bind_text(3, entry.name());
	add_entry.bind_text(4, entry.kind().name());
	add_entry.bind_text(5, entry.approximate_length());
	add_entry.bind_int64(6, entry.encrypted());
	add_entry.bind_double(7, entry.crop_to_ratio().get_value_or(0));
	add_entry.bind_int64(8, highest_index + 1);

	add_entry.execute();

	transaction.commit();
}


void
ShowPlaylistList::insert_entry(ShowPlaylistID playlist_id, ShowPlaylistEntry const& entry, int index)
{
	SQLiteTransaction transaction(_db);

	SQLiteStatement update(_db, "UPDATE entries SET sort_index=sort_index+1 WHERE show_playlist=? AND sort_index>=?");
	update.bind_int64(1, playlist_id.get());
	update.bind_int64(2, index);
	update.execute();

	SQLiteStatement insert_entry(_db, _entries.insert());
	insert_entry.bind_int64(1, playlist_id.get());
	insert_entry.bind_text(2, entry.uuid());
	insert_entry.bind_text(3, entry.name());
	insert_entry.bind_text(4, entry.kind().name());
	insert_entry.bind_text(5, entry.approximate_length());
	insert_entry.bind_int64(6, entry.encrypted());
	insert_entry.bind_double(7, entry.crop_to_ratio().get_value_or(0));
	insert_entry.bind_int64(8, index);
	insert_entry.execute();

	transaction.commit();
}


void
ShowPlaylistList::move_entry(ShowPlaylistID playlist_id, int old_index, int new_index)
{
	SQLiteTransaction transaction(_db);

	if (old_index == new_index) {
		return;
	}

	SQLiteStatement find_id(_db, "SELECT id FROM entries WHERE show_playlist=? AND sort_index=?");
	find_id.bind_int64(1, playlist_id.get());
	find_id.bind_int64(2, old_index);

	optional<int> moving_id;
	find_id.execute([&moving_id](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 1);
		moving_id = statement.column_int64(0);
	});
	DCPOMATIC_ASSERT(moving_id);

	auto const lower = old_index < new_index ? (old_index + 1) : new_index;
	auto const upper = old_index < new_index ? new_index : (old_index - 1);
	auto const direction = old_index < new_index ? "-1" : "+1";

	SQLiteStatement update_others(_db, fmt::format("UPDATE entries SET sort_index=sort_index{} WHERE show_playlist=? AND sort_index>=? AND sort_index<=?", direction));
	update_others.bind_int64(1, playlist_id.get());
	update_others.bind_int64(2, lower);
	update_others.bind_int64(3, upper);
	update_others.execute();

	SQLiteStatement update(_db, "UPDATE entries SET sort_index=? WHERE show_playlist=? AND id=?");
	update.bind_int64(1, new_index);
	update.bind_int64(2, playlist_id.get());
	update.bind_int64(3, *moving_id);
	update.execute();

	transaction.commit();
}


void
ShowPlaylistList::update_entry(ShowPlaylistID playlist_id, int index, ShowPlaylistEntry const& entry)
{
	SQLiteStatement update_entry(_db, _entries.update("WHERE show_playlist=? AND sort_index=?"));

	update_entry.bind_int64(1, playlist_id.get());
	update_entry.bind_text(2, entry.uuid());
	update_entry.bind_text(3, entry.name());
	update_entry.bind_text(4, entry.kind().name());
	update_entry.bind_text(5, entry.approximate_length());
	update_entry.bind_int64(6, entry.encrypted());
	update_entry.bind_double(7, entry.crop_to_ratio().get_value_or(0));
	update_entry.bind_int64(8, index);
	update_entry.bind_int64(9, playlist_id.get());
	update_entry.bind_int64(10, index);

	update_entry.execute();
}


void
ShowPlaylistList::remove_entry(ShowPlaylistID playlist_id, int index)
{
	SQLiteTransaction transaction(_db);

	SQLiteStatement delete_entry(_db, _entries.remove("WHERE show_playlist=? AND sort_index=?"));
	delete_entry.bind_int64(1, playlist_id.get());
	delete_entry.bind_int64(2, index);

	delete_entry.execute();

	SQLiteStatement find(_db, "SELECT id FROM entries WHERE show_playlist=? ORDER BY sort_index");
	find.bind_int64(1, playlist_id.get());

	vector<sqlite3_int64> ids;
	find.execute([&ids](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 1);
		ids.push_back(statement.column_int64(0));
	});

	int new_index = 0;
	for (auto id: ids) {
		SQLiteStatement update(_db, "UPDATE entries SET sort_index=? WHERE id=?");
		update.bind_int64(1, new_index++);
		update.bind_int64(2, id);
		update.execute();
	}

	transaction.commit();
}


/** Swap the entries at index and index + 1 */
void
ShowPlaylistList::swap_entries(ShowPlaylistID playlist_id, int index)
{
	SQLiteTransaction transaction(_db);

	SQLiteStatement find(_db, "SELECT id,sort_index FROM entries WHERE show_playlist=? ORDER BY sort_index LIMIT 2 OFFSET ?");
	find.bind_int64(1, playlist_id.get());
	find.bind_int64(2, index);

	vector<pair<int64_t, int64_t>> rows;
	find.execute([&rows](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 2);
		rows.push_back({statement.column_int64(0), statement.column_int64(1)});
	});

	DCPOMATIC_ASSERT(rows.size() == 2);

	SQLiteStatement swap1(_db, "UPDATE entries SET sort_index=? WHERE id=?");
	swap1.bind_int64(1, rows[0].second);
	swap1.bind_int64(2, rows[1].first);
	swap1.execute();

	SQLiteStatement swap2(_db, "UPDATE entries SET sort_index=? WHERE id=?");
	swap2.bind_int64(1, rows[1].second);
	swap2.bind_int64(2, rows[0].first);
	swap2.execute();

	transaction.commit();
}


void
ShowPlaylistList::move_entry_up(ShowPlaylistID playlist_id, int index)
{
	DCPOMATIC_ASSERT(index >= 1);
	swap_entries(playlist_id, index - 1);
}


void
ShowPlaylistList::move_entry_down(ShowPlaylistID playlist_id, int index)
{
	swap_entries(playlist_id, index);
}


void
ShowPlaylistList::read_legacy(boost::filesystem::path dir)
{
	auto const store = ShowPlaylistContentStore::instance();

	for (auto playlist: boost::filesystem::directory_iterator(dir)) {
		cxml::Document doc("SPL");
		doc.read_file(dcp::filesystem::fix_long_path(playlist));
		auto const spl_id = add_show_playlist({ doc.string_child("Id"), doc.string_child("Name") });
		for (auto entry: doc.node_children("Entry")) {
			auto uuid = entry->optional_string_child("CPL");
			if (!uuid) {
				uuid = entry->string_child("Digest");
			}
			if (auto content = store->get(*uuid)) {
				add_entry(spl_id, ShowPlaylistEntry(content, entry->optional_number_child<float>("CropToRatio")));
			}
		}
	}
}


bool
ShowPlaylistList::missing(string const& playlist_uuid) const
{
	auto store = ShowPlaylistContentStore::instance();
	for (auto entry: entries(playlist_uuid)) {
		if (!store->get(entry)) {
			return true;
		}
	}

	return false;
}


bool
ShowPlaylistList::missing(ShowPlaylistID playlist_id) const
{
	auto store = ShowPlaylistContentStore::instance();
	for (auto entry: entries(playlist_id)) {
		if (!store->get(entry)) {
			return true;
		}
	}

	return false;
}


optional<ShowPlaylistID>
ShowPlaylistList::get_show_playlist_id(string const& playlist_uuid) const
{
	SQLiteStatement statement(_db, "SELECT id FROM show_playlists WHERE uuid=?");
	statement.bind_text(1, playlist_uuid);

	optional<ShowPlaylistID> id;
	statement.execute([&id](SQLiteStatement& statement) {
		DCPOMATIC_ASSERT(statement.data_count() == 1);
		id = ShowPlaylistID(statement.column_int64(0));
	});

	return id;
}

