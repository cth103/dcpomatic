/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace boost;

string
find_dvd ()
{
	ifstream f ("/etc/mtab");
	while (f.good ()) {
		string s;
		getline (f, s);
		vector<string> b;
		split (b, s, is_any_of (" "));
		if (b.size() >= 3 && b[2] == "udf") {
			replace_all (b[1], "\\040", " ");
			return b[1];
		}
	}

	return "";
}

vector<uint64_t>
dvd_titles (string dvd)
{
	filesystem::path video (dvd);
	video /= "VIDEO_TS";

	vector<uint64_t> sizes;
	
	for (filesystem::directory_iterator i = filesystem::directory_iterator (video); i != filesystem::directory_iterator(); ++i) {
#if BOOST_FILESYSTEM_VERSION == 3		
		string const n = filesystem::path(*i).filename().generic_string();
#else		
		string const n = filesystem::path(*i).filename();
#endif
		if (starts_with (n, "VTS_") && ends_with (n, ".VOB")) {
			uint64_t const size = filesystem::file_size (filesystem::path (*i));
			vector<string> p;
			split (p, n, is_any_of ("_."));
			if (p.size() == 4) {
				int const a = atoi (p[1].c_str ());
				int const b = atoi (p[2].c_str ());
				while (a >= int (sizes.size())) {
					sizes.push_back (0);
				}

				if (b > 0) {
					sizes[a] += size;
				}
			}
		}
	}
					
	return sizes;		
}
