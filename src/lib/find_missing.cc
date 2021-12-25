/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "content.h"
#include "find_missing.h"
#include "util.h"
#include <boost/filesystem.hpp>


using std::map;
using std::shared_ptr;
using std::vector;


typedef map<shared_ptr<Content>, vector<boost::filesystem::path>> Replacements;


static
void
search (Replacements& replacement_paths, boost::filesystem::path directory, int depth = 0)
{
	for (auto candidate: boost::filesystem::directory_iterator(directory)) {
		if (boost::filesystem::is_regular_file(candidate.path())) {
			for (auto& replacement: replacement_paths) {
				for (auto& path: replacement.second) {
					if (!boost::filesystem::exists(path) && path.filename() == candidate.path().filename()) {
						path = candidate.path();
					}
				}
			}
		} else if (boost::filesystem::is_directory(candidate.path()) && depth <= 2) {
			search (replacement_paths, candidate, depth + 1);
		}
	}
}


void
dcpomatic::find_missing (vector<shared_ptr<Content>> content_to_fix, boost::filesystem::path clue)
{
	using namespace boost::filesystem;

	Replacements replacement_paths;
	for (auto content: content_to_fix) {
		replacement_paths[content] = content->paths();
	}

	search (replacement_paths, is_directory(clue) ? clue : clue.parent_path());

	for (auto content: content_to_fix) {
		auto const& repl = replacement_paths[content];
		bool const replacements_exist = std::find_if(repl.begin(), repl.end(), [](path p) { return !exists(p); }) == repl.end();
		if (replacements_exist && simple_digest(replacement_paths[content]) == content->digest()) {
			content->set_paths (repl);
		}
	}
}
