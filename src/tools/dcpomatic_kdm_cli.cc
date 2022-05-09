/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  src/tools/dcpomatic_kdm_cli.cc
 *  @brief Command-line program to generate KDMs.
 */


#include "lib/cross.h"
#include "lib/kdm_cli.h"
#include "lib/util.h"
#include <iostream>


int
main (int argc, char* argv[])
{
	ArgFixer fixer(argc, argv);

	dcpomatic_setup_path_encoding ();
	dcpomatic_setup ();

	auto error = kdm_cli (fixer.argc(), fixer.argv(), [](std::string s) { std::cout << s << "\n"; });
	if (error) {
		std::cerr << *error << "\n";
		exit (EXIT_FAILURE);
	}

	return 0;
}

