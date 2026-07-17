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


#include "dcpomatic_time.h"
#include "http_server.h"
#include "response.h"
#include "signaller.h"
LIBDCP_DISABLE_WARNINGS
#include <boost/signals2.hpp>
LIBDCP_ENABLE_WARNINGS


class PlayerHTTPServer : public HTTPServer, public Signaller
{
public:
	explicit PlayerHTTPServer(int port, int timeout = 30);

	boost::signals2::signal<void ()> Play;
	boost::signals2::signal<void ()> Stop;
	boost::signals2::signal<void (std::vector<std::pair<std::string, boost::optional<float>>>)> LoadPlaylist;

	void set_playing(bool playing) {
		boost::mutex::scoped_lock lm(_mutex);
		_playing = playing;
	}

	void set_position(dcpomatic::DCPTime position) {
		boost::mutex::scoped_lock lm(_mutex);
		_position = position;
	}

	void set_dcp_name(std::string name) {
		boost::mutex::scoped_lock lm(_mutex);
		_dcp_name = name;
	}

	void set_current_playlist(std::vector<std::string> playlist) {
		boost::mutex::scoped_lock lm(_mutex);
		_current_playlist = playlist;
	}

private:
	void substitute(std::string& page) const;
	Response get_request(std::string const& url) override;
	Response post_request(std::string const& url, std::string const& body) override;
	Response delete_request(std::string const& url) override;

	boost::mutex _mutex;
	bool _playing = false;
	dcpomatic::DCPTime _position;
	std::string _dcp_name;
	std::vector<std::string> _current_playlist;
};

