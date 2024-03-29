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
#include "film.h"
#include "film_util.h"


using std::shared_ptr;
using boost::optional;


bool
channel_is_mapped(shared_ptr<const Film> film, dcp::Channel channel)
{
	auto const mapped = film->mapped_audio_channels();
	return std::find(mapped.begin(), mapped.end(), static_cast<int>(channel)) != mapped.end();
}


optional<boost::filesystem::path>
add_files_override_path(shared_ptr<const Film> film)
{
	film->directory();
	return Config::instance()->default_add_file_location() == Config::DefaultAddFileLocation::SAME_AS_PROJECT
		? film->directory()->parent_path()
		: boost::optional<boost::filesystem::path>();

}
