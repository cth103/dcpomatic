/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#include "encoded_data.h"
#include "cross.h"
#include "exceptions.h"
#include "film.h"
#include "dcpomatic_socket.h"

#include "i18n.h"

using boost::shared_ptr;

EncodedData::EncodedData (int s)
	: _data (new uint8_t[s])
	, _size (s)
{

}

EncodedData::EncodedData (uint8_t const * d, int s)
	: _data (new uint8_t[s])
	, _size (s)
{
	memcpy (_data, d, s);
}

EncodedData::EncodedData (boost::filesystem::path file)
{
	_size = boost::filesystem::file_size (file);
	_data = new uint8_t[_size];

	FILE* f = fopen_boost (file, "rb");
	if (!f) {
		throw FileError (_("could not open file for reading"), file);
	}
	
	size_t const r = fread (_data, 1, _size, f);
	if (r != size_t (_size)) {
		fclose (f);
		throw FileError (_("could not read encoded data"), file);
	}
		
	fclose (f);
}


EncodedData::~EncodedData ()
{
	delete[] _data;
}

/** Write this data to a J2K file.
 *  @param Film Film.
 *  @param frame DCP frame index.
 */
void
EncodedData::write (shared_ptr<const Film> film, int frame, Eyes eyes) const
{
	boost::filesystem::path const tmp_j2c = film->j2c_path (frame, eyes, true);

	FILE* f = fopen_boost (tmp_j2c, "wb");
	
	if (!f) {
		throw WriteFileError (tmp_j2c, errno);
	}

	fwrite (_data, 1, _size, f);
	fclose (f);

	boost::filesystem::path const real_j2c = film->j2c_path (frame, eyes, false);

	/* Rename the file from foo.j2c.tmp to foo.j2c now that it is complete */
	boost::filesystem::rename (tmp_j2c, real_j2c);
}

void
EncodedData::write_info (shared_ptr<const Film> film, int frame, Eyes eyes, dcp::FrameInfo fin) const
{
	boost::filesystem::path const info = film->info_path (frame, eyes);
	FILE* h = fopen_boost (info, "w");
	fin.write (h);
	fclose (h);
}

/** Send this data to a socket.
 *  @param socket Socket
 */
void
EncodedData::send (shared_ptr<Socket> socket)
{
	socket->write (_size);
	socket->write (_data, _size);
}

LocallyEncodedData::LocallyEncodedData (uint8_t* d, int s)
	: EncodedData (s)
{
	memcpy (_data, d, s);
}

/** @param s Size of data in bytes */
RemotelyEncodedData::RemotelyEncodedData (int s)
	: EncodedData (s)
{

}
