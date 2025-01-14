/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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
#include "content.h"
#include "dcp_content.h"
#include "film.h"
#include "film_util.h"


using std::dynamic_pointer_cast;
using std::shared_ptr;
using boost::optional;


bool
dcpomatic::film::channel_is_mapped(shared_ptr<const Film> film, dcp::Channel channel)
{
	auto const mapped = film->mapped_audio_channels();
	return std::find(mapped.begin(), mapped.end(), static_cast<int>(channel)) != mapped.end();
}


optional<boost::filesystem::path>
dcpomatic::film::add_files_override_path(shared_ptr<const Film> film)
{
	film->directory();
	return Config::instance()->default_add_file_location() == Config::DefaultAddFileLocation::SAME_AS_PROJECT
		? film->directory()->parent_path()
		: boost::optional<boost::filesystem::path>();

}


bool
dcpomatic::film::is_vf(shared_ptr<const Film> film)
{
	for (auto content: film->content()) {
		auto dcp = dynamic_pointer_cast<const DCPContent>(content);
		if (!dcp) {
			continue;
		}

		bool any_text = false;
		for (int i = 0; i < static_cast<int>(TextType::COUNT); ++i) {
			if (dcp->reference_text(static_cast<TextType>(i))) {
				any_text = true;
			}
		}
		if (dcp->reference_video() || dcp->reference_audio() || any_text) {
			return true;
		}
	}

	return false;
}

