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
#include "content_factory.h"
#include "cross.h"
#include "dcp_content.h"
#include "examine_content_job.h"
#include "job_manager.h"
#include "show_playlist_content_store.h"
#include "show_playlist_entry.h"
#include "util.h"
#include <dcp/cpl.h>
#include <dcp/exceptions.h>
#include <dcp/filesystem.h>
#include <dcp/search.h>


using std::dynamic_pointer_cast;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;


ShowPlaylistContentStore* ShowPlaylistContentStore::_instance = nullptr;


vector<pair<string, string>>
ShowPlaylistContentStore::update(std::function<bool()> pulse)
{
	_content.clear();
	auto dir = Config::instance()->player_content_directory();
	if (!dir || !dcp::filesystem::is_directory(*dir)) {
		return {};
	}

	auto jm = JobManager::instance();

	vector<shared_ptr<ExamineContentJob>> jobs;

	auto examine = [&](shared_ptr<Content> content) {
		auto job = make_shared<ExamineContentJob>(vector<shared_ptr<Content>>{content}, true);
		jm->add(job);
		jobs.push_back(job);
	};

	for (auto i: boost::filesystem::directory_iterator(*dir)) {
		try {
			pulse();

			if (is_directory(i) && contains_assetmap(i)) {
				auto dcp = make_shared<DCPContent>(i);
				examine(dcp);
				/* Add a Content for each CPL in this DCP, so we can choose CPLs to play
				 * rather than DCPs.
				 */
				for (auto cpl: dcp->cpls()) {
					auto copy = dynamic_pointer_cast<DCPContent>(dcp->clone());
					copy->set_cpl(cpl);
					examine(copy);
				}
			} else if (i.path().extension() == ".mp4") {
				auto all_content = content_factory(i);
				if (!all_content.empty()) {
					examine(all_content[0]);
				}
			}
		} catch (boost::filesystem::filesystem_error& e) {
			/* Never mind */
		} catch (dcp::ReadError& e) {
			/* Never mind */
		}
	}

	while (jm->work_to_do()) {
		if (!pulse()) {
			/* user pressed cancel */
			for (auto i: jm->get()) {
				i->cancel();
			}
			return {};
		}
		dcpomatic_sleep_seconds(1);
	}

	/* Add content from successful jobs and report errors */
	vector<pair<string, string>> errors;
	for (auto i: jobs) {
		if (i->finished_in_error()) {
			errors.push_back({fmt::format("{}.\n", i->error_summary()), i->error_details()});
		} else {
			_content.push_back(i->content().front());
		}
	}

	return errors;
}


shared_ptr<Content>
ShowPlaylistContentStore::get(string const& uuid) const
{
	auto iter = std::find_if(_content.begin(), _content.end(), [uuid](shared_ptr<const Content> content) {
		if (auto dcp = dynamic_pointer_cast<const DCPContent>(content)) {
			for (auto cpl: dcp->cpls()) {
				if (cpl == uuid) {
					return true;
				}
			}
		}
		return false;
	});

	if (iter == _content.end()) {
		iter = std::find_if(_content.begin(), _content.end(), [uuid](shared_ptr<const Content> content) {
			return content->digest() == uuid;
		});
	}

	if (iter == _content.end()) {
		return {};
	}

	return *iter;
}


shared_ptr<Content>
ShowPlaylistContentStore::get(ShowPlaylistEntry const& entry) const
{
	return get(entry.uuid());
}


ShowPlaylistContentStore*
ShowPlaylistContentStore::instance()
{
	if (!_instance) {
		_instance = new ShowPlaylistContentStore();
	}

	return _instance;
}
