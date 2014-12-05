/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file src/scaler.h
 *  @brief A class to describe one of FFmpeg's software scalers.
 */

#ifndef DCPOMATIC_SCALER_H
#define DCPOMATIC_SCALER_H

#include <boost/utility.hpp>
#include <string>
#include <vector>

/** @class Scaler
 *  @brief Class to describe one of FFmpeg's software scalers
 */
class Scaler : public boost::noncopyable
{
public:
	Scaler (int f, std::string i, std::string n);

	/** @return id used for calls to FFmpeg's sws_getContext */
	int ffmpeg_id () const {
		return _ffmpeg_id;
	}

	/** @return id for our use */
	std::string id () const {
		return _id;
	}

	/** @return user-visible name for this scaler */
	std::string name () const {
		return _name;
	}
	
	static std::vector<Scaler const *> all ();
	static void setup_scalers ();
	static Scaler const * from_id (std::string id);
	static Scaler const * from_index (int);
	static int as_index (Scaler const *);

private:

	/** id used for calls to FFmpeg's pp_postprocess */
	int _ffmpeg_id;
	/** id for our use */
	std::string _id;
	/** user-visible name for this scaler */
	std::string _name;

	/** all available scalers */
	static std::vector<Scaler const *> _scalers;
};

#endif
