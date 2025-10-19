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


#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/show_playlist.h"
#include "lib/show_playlist_content_store.h"
#include "lib/show_playlist_entry.h"
#include "lib/show_playlist_list.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::make_shared;


BOOST_AUTO_TEST_CASE(test_create_show_playlist_entry_from_dcp)
{
	auto dcp = make_shared<DCPContent>("test/data/burnt_subtitle_test_dcp");
	auto film = new_test_film("test_create_show_playlist_entry_from_dcp", { dcp });
	ShowPlaylistEntry entry(dcp, 1.85);

	BOOST_CHECK_EQUAL(entry.uuid(), "808090c2-6dc8-4336-a112-2b0c3512334c");
	BOOST_CHECK_EQUAL(entry.name(), "Frobozz_TLR-1_F_XX-XX_MOS_2K_20240610_SMPTE_OV");
	BOOST_CHECK(entry.kind() == dcp::ContentKind::TRAILER);
	BOOST_CHECK_EQUAL(entry.approximate_length(), "00:00:02");
	BOOST_CHECK(!entry.encrypted());
	BOOST_CHECK_CLOSE(entry.crop_to_ratio().get_value_or(1), 1.85, 0.1);

	BOOST_CHECK_EQUAL(
		entry.as_json().dump(4),
"{\n"
"    \"approximate_length\": \"00:00:02\",\n"
"    \"crop_to_ratio\": 185,\n"
"    \"encrypted\": false,\n"
"    \"kind\": \"trailer\",\n"
"    \"name\": \"Frobozz_TLR-1_F_XX-XX_MOS_2K_20240610_SMPTE_OV\",\n"
"    \"uuid\": \"808090c2-6dc8-4336-a112-2b0c3512334c\"\n"
"}"
	);
}


BOOST_AUTO_TEST_CASE(test_create_show_playlist_entry_from_other)
{
	auto content = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film("test_create_show_playlist_entry_from_other", { content });
	ShowPlaylistEntry entry(content, {});

	BOOST_CHECK_EQUAL(entry.uuid(), "819f4022268af00e926516e04fc195908778");
	BOOST_CHECK_EQUAL(entry.name(), "flat_red.png");
	BOOST_CHECK(entry.kind() == dcp::ContentKind::FEATURE);
	BOOST_CHECK_EQUAL(entry.approximate_length(), "00:00:10");
	BOOST_CHECK(!entry.encrypted());
	BOOST_CHECK(!static_cast<bool>(entry.crop_to_ratio()));

	BOOST_CHECK_EQUAL(
		entry.as_json().dump(4),
"{\n"
"    \"approximate_length\": \"00:00:10\",\n"
"    \"encrypted\": false,\n"
"    \"kind\": \"feature\",\n"
"    \"name\": \"flat_red.png\",\n"
"    \"uuid\": \"819f4022268af00e926516e04fc195908778\"\n"
"}"
	);
}


BOOST_AUTO_TEST_CASE(test_show_playlist_content_store)
{
	ConfigRestorer cr;

	Config::instance()->set_player_content_directory("test/data");
	auto store = ShowPlaylistContentStore::instance();

	int pulses = 0;
	auto pulse = [&pulses]() { ++pulses; return true; };
	store->update(pulse);

	BOOST_CHECK(pulses > 0);

	BOOST_CHECK(store->get("e781b9d108a555b0fa12bfbaf308f0202058"));
	BOOST_CHECK(store->get("70eb015a-6328-468e-b53d-0211faaca64f"));
}


BOOST_AUTO_TEST_CASE(test_show_playlist_list)
{
	boost::filesystem::remove_all("build/test/playlist.sqlite3");
	ShowPlaylistList list("build/test/playlist.sqlite3");

	auto spl1 = ShowPlaylist("The Life of Brian + Support");
	auto id1 = list.add_show_playlist(spl1);
	BOOST_CHECK_EQUAL(list.show_playlists().size(), 1U);
	BOOST_CHECK(list.show_playlists()[0].first == id1);
	BOOST_CHECK(list.show_playlists()[0].second == spl1);

	BOOST_CHECK(static_cast<bool>(list.show_playlist(id1)));
	BOOST_CHECK(list.show_playlist(id1).get() == spl1);

	Config::instance()->set_player_content_directory("test/data");
	auto store = ShowPlaylistContentStore::instance();
	auto pulse = []() { return true; };
	store->update(pulse);

	list.add_entry(id1, ShowPlaylistEntry(store->get("e781b9d108a555b0fa12bfbaf308f0202058"), {}));
	list.add_entry(id1, ShowPlaylistEntry(store->get("70eb015a-6328-468e-b53d-0211faaca64f"), {}));
	BOOST_CHECK_EQUAL(list.entries(id1).size(), 2U);
	BOOST_CHECK(list.entries(id1)[0] == ShowPlaylistEntry(store->get("e781b9d108a555b0fa12bfbaf308f0202058"), {}));
	BOOST_CHECK(list.entries(id1)[1] == ShowPlaylistEntry(store->get("70eb015a-6328-468e-b53d-0211faaca64f"), {}));

	list.move_entry_up(id1, 1);
	BOOST_CHECK(list.entries(id1)[0] == ShowPlaylistEntry(store->get("70eb015a-6328-468e-b53d-0211faaca64f"), {}));
	BOOST_CHECK(list.entries(id1)[1] == ShowPlaylistEntry(store->get("e781b9d108a555b0fa12bfbaf308f0202058"), {}));

	list.move_entry_down(id1, 0);
	BOOST_CHECK(list.entries(id1)[0] == ShowPlaylistEntry(store->get("e781b9d108a555b0fa12bfbaf308f0202058"), {}));
	BOOST_CHECK(list.entries(id1)[1] == ShowPlaylistEntry(store->get("70eb015a-6328-468e-b53d-0211faaca64f"), {}));

	list.remove_entry(id1, 0);
	BOOST_CHECK_EQUAL(list.entries(id1).size(), 1U);
	BOOST_CHECK(list.entries(id1)[0] == ShowPlaylistEntry(store->get("70eb015a-6328-468e-b53d-0211faaca64f"), {}));

	list.update_entry(id1, 0, ShowPlaylistEntry(store->get("7cc527259b64a49137c18c30967e74508457"), {}));
	BOOST_CHECK_EQUAL(list.entries(id1).size(), 1U);
	BOOST_CHECK(list.entries(id1)[0] == ShowPlaylistEntry(store->get("7cc527259b64a49137c18c30967e74508457"), {}));

	BOOST_CHECK(!list.missing(id1));

	auto spl2 = ShowPlaylist("The Holy Grail + Trailers");
	auto id2 = list.add_show_playlist(spl2);
	BOOST_CHECK_EQUAL(list.show_playlists().size(), 2U);
	BOOST_CHECK(list.show_playlists()[0].first == id2);
	BOOST_CHECK(list.show_playlists()[0].second == spl2);
	BOOST_CHECK(list.show_playlists()[1].first == id1);
	BOOST_CHECK(list.show_playlists()[1].second == spl1);

	list.remove_show_playlist(id1);
	BOOST_CHECK_EQUAL(list.show_playlists().size(), 1U);
	BOOST_CHECK(list.show_playlists()[0].first == id2);
	BOOST_CHECK(list.show_playlists()[0].second == spl2);

	auto spl3 = ShowPlaylist("And now for something completely different");
	list.update_show_playlist(id2, spl3);
	BOOST_CHECK_EQUAL(list.show_playlists().size(), 1U);
	BOOST_CHECK(list.show_playlists()[0].first == id2);
	BOOST_CHECK(list.show_playlists()[0].second == spl3);
}


BOOST_AUTO_TEST_CASE(test_legacy_show_playlist)
{
	boost::filesystem::remove_all("build/test/playlist.sqlite3");

	Config::instance()->set_player_content_directory("test/data");
	auto store = ShowPlaylistContentStore::instance();
	auto pulse = []() { return true; };
	store->update(pulse);

	ShowPlaylistList list("build/test/playlist.sqlite3");
	list.read_legacy("test/data/playlists");

	auto show_playlists = list.show_playlists();
	BOOST_REQUIRE_EQUAL(show_playlists.size(), 2U);
	BOOST_CHECK_EQUAL(show_playlists[0].second.uuid(), "ea1423e5-dc22-473c-81c8-41e40499502c");
	BOOST_CHECK_EQUAL(show_playlists[0].second.name(), "Lost In Translation + Support");
	BOOST_CHECK_EQUAL(show_playlists[1].second.uuid(), "17963124-6212-4f8f-b20f-092fe566396c");
	BOOST_CHECK_EQUAL(show_playlists[1].second.name(), "The Life Aquatic + Support");

	auto entries = list.entries(show_playlists[0].first);
	BOOST_REQUIRE_EQUAL(entries.size(), 3U);
	BOOST_CHECK_EQUAL(entries[0].uuid(), "2d190c86-8ea3-4728-b7f8-14c465abdf1f");
	BOOST_CHECK_EQUAL(entries[0].name(), "Trigger_FTR-1_F_XX-XX_MOS_2K_20240211_IOP_OV");
	BOOST_CHECK_EQUAL(entries[0].kind().name(), "feature");
	BOOST_CHECK_EQUAL(entries[0].approximate_length(), "00:00:06");
	BOOST_CHECK(!entries[0].encrypted());
	BOOST_CHECK(!static_cast<bool>(entries[0].crop_to_ratio()));
	BOOST_CHECK_EQUAL(entries[1].uuid(), "59f52c34553e2a3f271ae922e079cd2b2061");
	BOOST_CHECK_EQUAL(entries[1].name(), "8bit_full_420.mp4");
	BOOST_CHECK_EQUAL(entries[1].kind().name(), "feature");
	BOOST_CHECK_EQUAL(entries[1].approximate_length(), "00:00:00");
	BOOST_CHECK(!entries[1].encrypted());
	BOOST_CHECK(!static_cast<bool>(entries[1].crop_to_ratio()));
	BOOST_CHECK_EQUAL(entries[2].uuid(), "57b71a5b3239fe7a3db0da51a891505b2058");
	BOOST_CHECK_EQUAL(entries[2].name(), "10bit_video_420.mp4");
	BOOST_CHECK_EQUAL(entries[2].kind().name(), "feature");
	BOOST_CHECK_EQUAL(entries[2].approximate_length(), "00:00:00");
	BOOST_CHECK(!entries[2].encrypted());
	BOOST_CHECK(!static_cast<bool>(entries[2].crop_to_ratio()));
}

