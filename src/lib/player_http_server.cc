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


#include "cross.h"
#include "dcpomatic_log.h"
#include "dcpomatic_socket.h"
#include "player_http_server.h"
#include "show_playlist.h"
#include "show_playlist_content_store.h"
#include "show_playlist_list.h"
#include "util.h"
#include "variant.h"
#include <dcp/raw_convert.h>
#include <nlohmann/json.hpp>
#include <boost/algorithm/string.hpp>


using std::pair;
using std::string;
using std::vector;
using boost::optional;


PlayerHTTPServer::PlayerHTTPServer(int port, int timeout)
	: HTTPServer(port, timeout)
{

}



void
PlayerHTTPServer::substitute(string& page) const
{
	boost::algorithm::replace_all(page, "TITLE", variant::dcpomatic_player());
	boost::algorithm::replace_all(page, "SIDEBAR", dcp::file_to_string(resources_path() / "web" / "sidebar.html"));
}


Response
PlayerHTTPServer::get_request(string const& url)
{
	if (url == "/") {
		auto page = dcp::file_to_string(resources_path() / "web" / "index.html");
		substitute(page);
		return Response(200, page);
	} else if (url == "/playlists") {
		auto page = dcp::file_to_string(resources_path() / "web" / "playlists.html");
		substitute(page);
		return Response(200, page);
	} else if (url == "/common.css") {
		auto page = dcp::file_to_string(resources_path() / "web" / "common.css");
		return Response(200, page, Response::Type::CSS);
	} else if (url == "/dom.png") {
		auto page = dcp::file_to_string(resources_path() / "web" / "dom.png");
		return Response(200, page, Response::Type::PNG);
	} else if (url == "/api/v1/status") {
		nlohmann::json json;
		{
			boost::mutex::scoped_lock lm(_mutex);
			json["playing"] = _playing;
			json["position"] = seconds_to_hms(_position.seconds());
			json["dcp_name"] = _dcp_name;
		}
		return Response(200, json.dump(), Response::Type::JSON);
	} else if (url == "/api/v1/playlists") {
		ShowPlaylistList list;
		nlohmann::json json;
		for (auto spl: list.show_playlists()) {
			json.push_back(spl.second.as_json());
		}
		return Response(200, json.dump(), Response::Type::JSON);
	} else if (url == "/api/v1/current-playlist") {
		nlohmann::json json;
		boost::mutex::scoped_lock lm(_mutex);
		for (auto entry: _current_playlist) {
			json.push_back(entry);
		}
		return Response(200, json.dump(), Response::Type::JSON);
	} else if (boost::algorithm::starts_with(url, "/api/v1/content/")) {
		vector<string> parts;
		boost::algorithm::split(parts, url, boost::is_any_of("/"));
		if (parts.size() != 5) {
			return Response::ERROR_404;
		}
		auto content = ShowPlaylistContentStore::instance()->get(parts[4]);
		if (!content) {
			return Response::ERROR_404;
		}
		/* XXX: converting to JSON this way feels a bit grotty */
		auto json = ShowPlaylistEntry(content, {}).as_json();
		return Response(200, json.dump(), Response::Type::JSON);
	} else if (boost::algorithm::starts_with(url, "/api/v1/playlist/")) {
		vector<string> parts;
		boost::algorithm::split(parts, url, boost::is_any_of("/"));
		if (parts.size() != 5) {
			return Response::ERROR_404;
		}
		ShowPlaylistList list;
		auto playlist_id = list.get_show_playlist_id(parts[4]);
		if (!playlist_id) {
			return Response::ERROR_404;
		}
		auto playlist = list.show_playlist(*playlist_id);
		if (!playlist) {
			return Response::ERROR_404;
		}
		nlohmann::json json = playlist->as_json();
		json["content"] = nlohmann::json::array();
		auto entries = list.entries(parts[4]);
		for (auto entry: list.entries(parts[4])) {
			json["content"].push_back(entry.as_json());
		}
		return Response(200, json.dump(), Response::Type::JSON);
	} else if (url == "/api/v1/content") {
		nlohmann::json json;
		for (auto i: ShowPlaylistContentStore::instance()->all()) {
			/* XXX: converting to JSON this way feels a bit grotty */
			json.push_back(ShowPlaylistEntry(i, {}).as_json());
		}
		return Response(200, json.dump(), Response::Type::JSON);
	} else if (boost::algorithm::starts_with(url, "/api/v1/content/")) {
		vector<string> parts;
		boost::algorithm::split(parts, url, boost::is_any_of("/"));
		if (parts.size() != 5) {
			return Response::ERROR_404;
		}
		auto content = ShowPlaylistContentStore::instance()->get(parts[4]);
		if (!content) {
			return Response::ERROR_404;
		}
		/* XXX: converting to JSON this way feels a bit grotty */
		auto json = ShowPlaylistEntry(content, {}).as_json();
		return Response(200, json.dump(), Response::Type::JSON);
	} else {
		LOG_HTTP("404 {}", url);
		return Response::ERROR_404;
	}
}


Response
PlayerHTTPServer::post_request(string const& url, string const& body)
{
	if (url == "/api/v1/play") {
		emit(boost::bind(boost::ref(Play)));
		auto response = Response(303);
		response.add_header("Location", "/");
		return response;
	} else if (url == "/api/v1/stop") {
		emit(boost::bind(boost::ref(Stop)));
		auto response = Response(303);
		response.add_header("Location", "/");
		return response;
	} else if (url == "/api/v1/load-playlist") {
		nlohmann::json details = nlohmann::json::parse(body);
		vector<pair<string, boost::optional<float>>> entries;
		for (auto const& entry: details["entries"]) {
			if (!entry.contains("uuid")) {
				continue;
			}
			boost::optional<float> crop;
			if (entry.contains("crop_to_ratio")) {
				crop = entry["crop_to_ratio"];
			}
			entries.push_back({entry["uuid"], crop});
		}
		emit(boost::bind(boost::ref(LoadPlaylist), entries));
		/* XXX: return a failure if LoadPlaylist fails */
		return Response(200);
	} else if (url == "/api/v1/playlists") {
		ShowPlaylist playlist("New Playlist");
		ShowPlaylistList list;
		list.add_show_playlist(playlist);
		return Response(200);
	} else if (boost::algorithm::starts_with(url, "/api/v1/playlist/")) {
		vector<string> parts;
		boost::algorithm::split(parts, url, boost::is_any_of("/"));
		if (parts.size() != 6) {
			return Response::ERROR_404;
		}
		ShowPlaylistList list;
		auto playlist_id = list.get_show_playlist_id(parts[4]);
		if (!playlist_id) {
			return Response::ERROR_404;
		}
		nlohmann::json details = nlohmann::json::parse(body);
		if (parts[5] == "insert") {
			auto content = ShowPlaylistContentStore::instance()->get(details["uuid"]);
			if (!content) {
				return Response::ERROR_404;
			}
			list.insert_entry(*playlist_id, ShowPlaylistEntry(content, {}), details["index"]);
		} else if (parts[5] == "move") {
			list.move_entry(*playlist_id, details["old_index"], details["new_index"]);
		} else if (parts[5] == "rename") {
			if (auto playlist = list.show_playlist(*playlist_id)) {
				playlist->set_name(details["name"]);
				list.update_show_playlist(*playlist_id, *playlist);
			}
		} else {
			return Response::ERROR_404;
		}
		return Response(200);
	} else {
		return Response::ERROR_404;
	}
}


Response
PlayerHTTPServer::delete_request(string const& url)
{
	if (boost::algorithm::starts_with(url, "/api/v1/playlist/")) {
		vector<string> parts;
		boost::algorithm::split(parts, url, boost::is_any_of("/"));
		if (parts.size() != 5) {
			return Response::ERROR_404;
		}
		ShowPlaylistList list;
		auto playlist_id = list.get_show_playlist_id(parts[4]);
		if (!playlist_id) {
			return Response::ERROR_404;
		}
		list.remove_show_playlist(*playlist_id);
		return Response(200);
	}

	return Response::ERROR_404;
}


