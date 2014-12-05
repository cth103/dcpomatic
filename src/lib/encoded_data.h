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

#include "types.h"
#include <dcp/picture_mxf_writer.h>
#include <boost/noncopyable.hpp>
#include <boost/filesystem.hpp>

class Socket;
class Film;

/** @class EncodedData
 *  @brief Container for J2K-encoded data.
 */
class EncodedData : public boost::noncopyable
{
public:
	/** @param s Size of data, in bytes */
	EncodedData (int s);
	EncodedData (uint8_t const * d, int s);

	EncodedData (boost::filesystem::path);

	virtual ~EncodedData ();

	void send (boost::shared_ptr<Socket> socket);
	void write (boost::shared_ptr<const Film>, int, Eyes) const;
	void write_info (boost::shared_ptr<const Film>, int, Eyes, dcp::FrameInfo) const;

	/** @return data */
	uint8_t* data () const {
		return _data;
	}

	/** @return data size, in bytes */
	int size () const {
		return _size;
	}

protected:
	uint8_t* _data; ///< data
	int _size;	///< data size in bytes
};

/** @class LocallyEncodedData
 *  @brief EncodedData that was encoded locally; this class
 *  just keeps a pointer to the data, but does no memory
 *  management.
 */
class LocallyEncodedData : public EncodedData
{
public:
	/** @param d Data (which will be copied by this class)
	 *  @param s Size of data, in bytes.
	 */
	LocallyEncodedData (uint8_t* d, int s);
};

/** @class RemotelyEncodedData
 *  @brief EncodedData that is being read from a remote server;
 *  this class allocates and manages memory for the data.
 */
class RemotelyEncodedData : public EncodedData
{
public:
	RemotelyEncodedData (int s);
};
