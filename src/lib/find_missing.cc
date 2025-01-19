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
#include <dcp/filesystem.h>


using std::map;
using std::shared_ptr;
using std::vector;


typedef map<shared_ptr<const Content>, vector<boost::filesystem::path>> Replacements;


static
void
search_by_name(Replacements& replacement_paths, boost::filesystem::path directory, int depth = 0)
{
	boost::system::error_code ec;
	for (auto candidate: dcp::filesystem::directory_iterator(directory, ec)) {
		if (dcp::filesystem::is_regular_file(candidate.path())) {
			for (auto& replacement: replacement_paths) {
				for (auto& path: replacement.second) {
					if (!dcp::filesystem::exists(path) && path.filename() == candidate.path().filename()) {
						path = candidate.path();
					}
				}
			}
		} else if (dcp::filesystem::is_directory(candidate.path()) && depth <= 2) {
			search_by_name(replacement_paths, candidate, depth + 1);
		}
	}

	/* Just ignore errors when creating the directory_iterator; they can be triggered by things like
	 * macOS' love of creating random directories (see #2291).
	 */
}


static
void
search_by_digest(Replacements& replacement_paths, boost::filesystem::path directory, int depth = 0)
{
	boost::system::error_code ec;
	for (auto candidate: dcp::filesystem::directory_iterator(directory, ec)) {
		if (dcp::filesystem::is_regular_file(candidate.path())) {
			auto const candidate_digest = simple_digest({candidate.path()});
			for (auto& replacement: replacement_paths) {
				DCPOMATIC_ASSERT(replacement.first->number_of_paths() == 1)
				if (replacement.first->digest() == candidate_digest) {
					replacement.second = { candidate.path() };
				}
			}
		} else if (dcp::filesystem::is_directory(candidate.path()) && depth <= 2) {
			search_by_digest(replacement_paths, candidate, depth + 1);
		}
	}
}


void
dcpomatic::find_missing (vector<shared_ptr<Content>> content_to_fix, boost::filesystem::path clue)
{
	using namespace boost::filesystem;

	Replacements name_replacement_paths;
	for (auto content: content_to_fix) {
		name_replacement_paths[content] = content->paths();
	}

	/* Look for replacements with the same filename */
	search_by_name(name_replacement_paths, is_directory(clue) ? clue : clue.parent_path());

	/* Fix any content that can be fixed with those, making a note of those that cannot */
	Replacements digest_replacement_paths;
	for (auto content: content_to_fix) {
		auto const& repl = name_replacement_paths[content];
		bool const replacements_exist = std::find_if(repl.begin(), repl.end(), [](path p) { return !exists(p); }) == repl.end();
		if (replacements_exist && simple_digest(name_replacement_paths[content]) == content->digest()) {
			content->set_paths (repl);
		} else {
			/* Put it on the list to look for by digest, if possible */
			if (content->number_of_paths() == 1) {
				digest_replacement_paths[content] = name_replacement_paths[content];
			}
		}
	}

	if (!digest_replacement_paths.empty()) {
		/* Search for content with just one path by digest */
		search_by_digest(digest_replacement_paths, is_directory(clue) ? clue : clue.parent_path());

		for (auto content: content_to_fix) {
			auto iter = digest_replacement_paths.find(content);
			if (iter != digest_replacement_paths.end()) {
				auto const& repl = iter->second;
				bool const replacements_exist = std::find_if(repl.begin(), repl.end(), [](path p) { return !exists(p); }) == repl.end();
				if (replacements_exist) {
					content->set_paths(repl);
				}
			}
		}
	}
}
