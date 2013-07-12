/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>
    Taken from code Copyright (C) 2010-2011 Terrence Meiczinger

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

#include <openjpeg.h>
#include <libdcp/picture_asset.h>
#include "util.h"

/** @file  src/dcp_video_frame.h
 *  @brief A single frame of video destined for a DCP.
 */

class Film;
class ServerDescription;
class Scaler;
class Image;
class Log;
class Subtitle;

/** @class EncodedData
 *  @brief Container for J2K-encoded data.
 */
class EncodedData
{
public:
	/** @param s Size of data, in bytes */
	EncodedData (int s);

	EncodedData (std::string f);

	virtual ~EncodedData ();

	void send (boost::shared_ptr<Socket> socket);
	void write (boost::shared_ptr<const Film>, int) const;
	void write_info (boost::shared_ptr<const Film>, int, libdcp::FrameInfo) const;

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
	int _size;      ///< data size in bytes

private:
	/* No copy construction */
	EncodedData (EncodedData const &);
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

/** @class DCPVideoFrame
 *  @brief A single frame of video destined for a DCP.
 *
 *  Given an Image and some settings, this class knows how to encode
 *  the image to J2K either on the local host or on a remote server.
 *
 *  Objects of this class are used for the queue that we keep
 *  of images that require encoding.
 */
class DCPVideoFrame
{
public:
	DCPVideoFrame (boost::shared_ptr<const Image>, int, int, int, int, boost::shared_ptr<Log>);
	~DCPVideoFrame ();

	boost::shared_ptr<EncodedData> encode_locally ();
	boost::shared_ptr<EncodedData> encode_remotely (ServerDescription const *);

	int frame () const {
		return _frame;
	}
	
private:
	boost::shared_ptr<const Image> _image;
	int _frame;                      ///< frame index within the DCP's intrinsic duration
	int _frames_per_second;          ///< Frames per second that we will use for the DCP
	int _colour_lut;                 ///< Colour look-up table to use
	int _j2k_bandwidth;              ///< J2K bandwidth to use

	boost::shared_ptr<Log> _log; ///< log

	opj_cparameters_t* _parameters;    ///< libopenjpeg's parameters
	opj_cinfo_t* _cinfo;               ///< libopenjpeg's opj_cinfo_t
	opj_cio_t* _cio;                   ///< libopenjpeg's opj_cio_t
};
