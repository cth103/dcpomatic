/*
    Copyright (C) 2013-2019 Carl Hetherington <cth@carlh.net>

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


#include "lib/create_cli.h"
#include "lib/cross.h"
#include "lib/film.h"
#include "lib/signal_manager.h"
#include "lib/state.h"
#include "lib/util.h"
#include "lib/version.h"
#include <dcp/exceptions.h>
#include <dcp/filesystem.h>
#include <libxml++/libxml++.h>
#include <boost/filesystem.hpp>
#include <getopt.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>


using std::cerr;
using std::cout;
using std::dynamic_pointer_cast;
using std::exception;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;


class SimpleSignalManager : public SignalManager
{
public:
	/* Do nothing in this method so that UI events happen in our thread
	   when we call SignalManager::ui_idle().
	*/
	void wake_ui () override {}
};

int
main (int argc, char* argv[])
{
	ArgFixer fixer(argc, argv);

	dcpomatic_setup_path_encoding ();
	dcpomatic_setup ();

	CreateCLI cc(fixer.argc(), fixer.argv());
	if (cc.error) {
		cerr << *cc.error << "\n";
		exit (1);
	}

	if (cc.version) {
		cout << "dcpomatic version " << dcpomatic_version << " " << dcpomatic_git_commit << "\n";
		exit (EXIT_SUCCESS);
	}

	if (cc.config_dir) {
		State::override_path = *cc.config_dir;
	}

	signal_manager = new SimpleSignalManager ();

	try {
		auto film = cc.make_film([](string s) { cerr << s; });
		if (!film) {
			exit(EXIT_FAILURE);
		}

		if (cc.output_dir) {
			film->write_metadata ();
		} else {
			film->metadata()->write_to_stream_formatted(cout, "UTF-8");
		}
	} catch (exception& e) {
		cerr << argv[0] << ": " << e.what() << "\n";
		exit (EXIT_FAILURE);
	}

	return 0;
}
