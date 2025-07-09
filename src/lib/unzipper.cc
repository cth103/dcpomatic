/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_assert.h"
#include "exceptions.h"
#include "unzipper.h"
#include <dcp/filesystem.h>
#include <dcp/scope_guard.h>
#include <zip.h>
#include <boost/filesystem.hpp>
#include <stdexcept>

#include "i18n.h"


using std::runtime_error;
using std::shared_ptr;
using std::string;


Unzipper::Unzipper(boost::filesystem::path file)
{
	int error;
#ifdef DCPOMATIC_HAVE_ZIP_RDONLY
	_zip = zip_open(dcp::filesystem::fix_long_path(file).string().c_str(), ZIP_RDONLY, &error);
#else
	_zip = zip_open(dcp::filesystem::fix_long_path(file).string().c_str(), 0, &error);
#endif
	if (!_zip) {
		throw FileError("could not open ZIP file", file);
	}
}


Unzipper::~Unzipper()
{
	zip_close(_zip);
}


bool
Unzipper::contains(string const& filename) const
{
	auto file = zip_fopen(_zip, filename.c_str(), 0);
	bool exists = file != nullptr;
	if (file) {
		zip_fclose(file);
	}
	return exists;
}


string
Unzipper::get(string const& filename) const
{
	auto file = zip_fopen(_zip, filename.c_str(), 0);
	if (!file) {
		throw runtime_error(fmt::format(_("Could not find file {} in ZIP file"), filename));
	}

	dcp::ScopeGuard sg = [file]() { zip_fclose(file); };

	int constexpr maximum = 65536;

	dcp::ArrayData data(maximum);
	int remaining = maximum;
	uint8_t* next = data.data();

	while (remaining > 0) {
		auto read = zip_fread(file, next, remaining);
		if (read == 0) {
			break;
		} else if (read == -1) {
			throw runtime_error("Could not read from ZIP file");
		}

		next += read;
		remaining -= read;
	}

	if (remaining == 0) {
		throw runtime_error("File from ZIP is too big");
	}

	return string(reinterpret_cast<char*>(data.data()), maximum - remaining);
}

