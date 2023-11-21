/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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
#include "zipper.h"
#include <dcp/filesystem.h>
#include <zip.h>
#include <boost/filesystem.hpp>
#include <stdexcept>


using std::runtime_error;
using std::shared_ptr;
using std::string;


Zipper::Zipper (boost::filesystem::path file)
{
	int error;
	_zip = zip_open(dcp::filesystem::fix_long_path(file).string().c_str(), ZIP_CREATE | ZIP_EXCL, &error);
	if (!_zip) {
		if (error == ZIP_ER_EXISTS) {
			throw FileError ("ZIP file already exists", file);
		}
		throw FileError ("could not create ZIP file", file);
	}
}


void
Zipper::add (string name, string content)
{
	shared_ptr<string> copy(new string(content));
	_store.push_back (copy);

	auto source = zip_source_buffer (_zip, copy->c_str(), copy->length(), 0);
	if (!source) {
		throw runtime_error ("could not create ZIP source");
	}

#ifdef DCPOMATIC_HAVE_ZIP_FILE_ADD
	if (zip_file_add(_zip, name.c_str(), source, ZIP_FL_ENC_GUESS) == -1) {
#else
	if (zip_add(_zip, name.c_str(), source) == -1) {
#endif
		throw runtime_error(String::compose("failed to add data to ZIP archive (%1)", zip_strerror(_zip)));
	}
}


void
Zipper::close ()
{
	if (zip_close(_zip) == -1) {
		throw runtime_error ("failed to close ZIP archive");
	}
	_zip = 0;
}


Zipper::~Zipper ()
{
	if (_zip) {
		zip_close(_zip);
	}
}
