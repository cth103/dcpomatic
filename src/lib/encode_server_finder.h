/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/encode_server_finder.h
 *  @brief EncodeServerFinder class.
 */

#include "signaller.h"
#include "encode_server_description.h"
#include "config.h"
#include "exception_store.h"
#include <boost/signals2.hpp>
#include <boost/thread/condition.hpp>

class Socket;

/** @class EncodeServerFinder
 *  @brief Locater of encoding servers.
 *
 *  This class finds active (i.e. responding) encode servers.  Depending on
 *  configuration it finds servers by:
 *
 *  1. broadcasting a request to the local subnet and
 *  2. checking to see if any of the configured server hosts are up.
 */
class EncodeServerFinder : public Signaller, public ExceptionStore
{
public:
	static EncodeServerFinder* instance ();
	static void drop ();

	void stop ();

	std::list<EncodeServerDescription> good_servers () const;
	std::list<EncodeServerDescription> bad_servers () const;

	/** Emitted whenever the list of servers changes */
	boost::signals2::signal<void ()> ServersListChanged;

private:
	EncodeServerFinder ();
	~EncodeServerFinder ();

	void start ();

	void search_thread ();
	void listen_thread ();

	bool server_found (std::string) const;
	void start_accept ();
	void handle_accept (boost::system::error_code ec, boost::shared_ptr<Socket> socket);

	void config_changed (Config::Property what);

	/** Thread to periodically issue broadcasts and requests to find encoding servers */
	boost::thread* _search_thread;
	/** Thread to listen to the responses from servers */
	boost::thread* _listen_thread;

	/** Available servers with the correct link version */
	std::list<EncodeServerDescription> _good_servers;
	/** Available servers with an incorrect link version */
	std::list<EncodeServerDescription> _bad_servers;
	/** Mutex for _good_servers and _bad_servers */
	mutable boost::mutex _servers_mutex;

	boost::asio::io_service _listen_io_service;
	boost::shared_ptr<boost::asio::ip::tcp::acceptor> _listen_acceptor;
	bool _stop;

	boost::condition _search_condition;
	boost::mutex _search_condition_mutex;

	static EncodeServerFinder* _instance;
};
