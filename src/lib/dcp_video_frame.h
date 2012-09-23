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
#include "util.h"

/** @file  src/dcp_video_frame.h
 *  @brief A single frame of video destined for a DCP.
 */

class FilmState;
class Options;
class ServerDescription;
class Scaler;
class Image;
class Log;

/** @class EncodedData
 *  @brief Container for J2K-encoded data.
 */
class EncodedData
{
public:
	/** @param d Data (will not be freed by this class, but may be by subclasses)
	 *  @param s Size of data, in bytes.
	 */
	EncodedData (uint8_t* d, int s)
		: _data (d)
		, _size (s)
	{}

	virtual ~EncodedData () {}

	void send (boost::shared_ptr<DeadlineWrapper> wrapper);
	void write (boost::shared_ptr<const Options>, int);

#ifdef DEBUG_HASH
	void hash (std::string) const;
#endif	

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
};

/** @class LocallyEncodedData
 *  @brief EncodedData that was encoded locally; this class
 *  just keeps a pointer to the data, but does no memory
 *  management.
 */
class LocallyEncodedData : public EncodedData
{
public:
	/** @param d Data (which will not be freed by this class)
	 *  @param s Size of data, in bytes.
	 */
	LocallyEncodedData (uint8_t* d, int s)
		: EncodedData (d, s)
	{}
};

/** @class RemotelyEncodedData
 *  @brief EncodedData that is being read from a remote server;
 *  this class allocates and manages memory for the data.
 */
class RemotelyEncodedData : public EncodedData
{
public:
	RemotelyEncodedData (int s);
	~RemotelyEncodedData ();
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
	DCPVideoFrame (boost::shared_ptr<Image>, Size, int, Scaler const *, int, float, std::string, int, int, Log *);
	virtual ~DCPVideoFrame ();

	boost::shared_ptr<EncodedData> encode_locally ();
	boost::shared_ptr<EncodedData> encode_remotely (ServerDescription const *);

	int frame () const {
		return _frame;
	}
	
private:
	void create_openjpeg_container ();
	void write_encoded (boost::shared_ptr<const Options>, uint8_t *, int);

	boost::shared_ptr<Image> _input; ///< the input image
	Size _out_size;                  ///< the required size of the output, in pixels
	int _padding;
	Scaler const * _scaler;          ///< scaler to use
	int _frame;                      ///< frame index within the Film
	int _frames_per_second;          ///< Frames per second that we will use for the DCP (rounded)
	std::string _post_process;       ///< FFmpeg post-processing string to use
	int _colour_lut_index;           ///< Colour look-up table to use (see Config::colour_lut_index ())
	int _j2k_bandwidth;              ///< J2K bandwidth to use (see Config::j2k_bandwidth ())

	Log* _log; ///< log

	opj_image_cmptparm_t _cmptparm[3]; ///< libopenjpeg's opj_image_cmptparm_t
	opj_image* _image;                 ///< libopenjpeg's image container 
	opj_cparameters_t* _parameters;    ///< libopenjpeg's parameters
	opj_cinfo_t* _cinfo;               ///< libopenjpeg's opj_cinfo_t
	opj_cio_t* _cio;                   ///< libopenjpeg's opj_cio_t
};
