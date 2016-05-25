/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "uploader.h"
#include "dcpomatic_assert.h"
#include "compose.hpp"

#include "i18n.h"

using std::string;
using boost::shared_ptr;
using boost::function;

Uploader::Uploader (function<void (string)> set_status, function<void (float)> set_progress)
	: _set_progress (set_progress)
	, _set_status (set_status)
{
	_set_status (_("connecting"));
}

boost::uintmax_t
Uploader::count_file_sizes (boost::filesystem::path directory) const
{
	using namespace boost::filesystem;

	boost::uintmax_t size = 0;

	for (directory_iterator i = directory_iterator (directory); i != directory_iterator (); ++i) {
		if (is_directory (i->path ())) {
			size += count_file_sizes (i->path ());
		} else {
			size += file_size (*i);
		}
	}

	return size;
}

void
Uploader::upload (boost::filesystem::path directory)
{
	boost::uintmax_t transferred = 0;
	upload_directory (directory.parent_path (), directory, transferred, count_file_sizes (directory));
}

void
Uploader::upload_directory (boost::filesystem::path base, boost::filesystem::path directory, boost::uintmax_t& transferred, boost::uintmax_t total_size)
{
	using namespace boost::filesystem;

	create_directory (remove_prefix (base, directory));
	for (directory_iterator i = directory_iterator (directory); i != directory_iterator (); ++i) {
		if (is_directory (i->path ())) {
			upload_directory (base, i->path (), transferred, total_size);
		} else {
			_set_status (String::compose (_("copying %1"), i->path().leaf ()));
			upload_file (i->path (), remove_prefix (base, i->path ()), transferred, total_size);
		}
	}
}

boost::filesystem::path
Uploader::remove_prefix (boost::filesystem::path prefix, boost::filesystem::path target) const
{
	using namespace boost::filesystem;

	path result;

	path::iterator i = target.begin ();
	for (path::iterator j = prefix.begin (); j != prefix.end(); ++j) {
		DCPOMATIC_ASSERT (*i == *j);
		++i;
	}

	for (; i != target.end(); ++i) {
		result /= *i;
	}

	return result;
}
