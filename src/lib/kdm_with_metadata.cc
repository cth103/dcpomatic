/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "kdm_with_metadata.h"
#include "cinema.h"
#include "screen.h"
#include "util.h"
#include <boost/foreach.hpp>

using std::string;
using std::cout;
using std::list;
using boost::shared_ptr;
using boost::optional;

int
write_files (
	list<KDMWithMetadataPtr> screen_kdms,
	boost::filesystem::path directory,
	dcp::NameFormat name_format,
	dcp::NameFormat::Map name_values,
	boost::function<bool (boost::filesystem::path)> confirm_overwrite
	)
{
	int written = 0;

	if (directory == "-") {
		/* Write KDMs to the stdout */
		BOOST_FOREACH (KDMWithMetadataPtr i, screen_kdms) {
			cout << i->kdm_as_xml ();
			++written;
		}

		return written;
	}

	if (!boost::filesystem::exists (directory)) {
		boost::filesystem::create_directories (directory);
	}

	/* Write KDMs to the specified directory */
	BOOST_FOREACH (KDMWithMetadataPtr i, screen_kdms) {
		name_values['i'] = i->kdm_id ();
		boost::filesystem::path out = directory / careful_string_filter(name_format.get(name_values, ".xml"));
		if (!boost::filesystem::exists (out) || confirm_overwrite (out)) {
			i->kdm_as_xml (out);
			++written;
		}
	}

	return written;
}


optional<string>
KDMWithMetadata::get (char k) const
{
	dcp::NameFormat::Map::const_iterator i = _name_values.find (k);
	if (i == _name_values.end()) {
		return optional<string>();
	}

	return i->second;
}
