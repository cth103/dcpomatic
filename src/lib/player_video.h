/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include <boost/shared_ptr.hpp>
#include "types.h"
#include "position.h"
#include "colour_conversion.h"
#include "position_image.h"

class Image;
class ImageProxy;
class Scaler;
class Socket;
class Log;

/** Everything needed to describe a video frame coming out of the player, but with the
 *  bits still their raw form.  We may want to combine the bits on a remote machine,
 *  or maybe not even bother to combine them at all.
 */
class PlayerVideo
{
public:
	PlayerVideo (boost::shared_ptr<const ImageProxy>, DCPTime, Crop, dcp::Size, dcp::Size, Scaler const *, Eyes, Part, ColourConversion);
	PlayerVideo (boost::shared_ptr<cxml::Node>, boost::shared_ptr<Socket>, boost::shared_ptr<Log>);

	void set_subtitle (PositionImage);
	
	boost::shared_ptr<Image> image () const;

	void add_metadata (xmlpp::Node* node) const;
	void send_binary (boost::shared_ptr<Socket> socket) const;

	DCPTime time () const {
		return _time;
	}

	Eyes eyes () const {
		return _eyes;
	}

	ColourConversion colour_conversion () const {
		return _colour_conversion;
	}

private:
	boost::shared_ptr<const ImageProxy> _in;
	DCPTime _time;
	Crop _crop;
	dcp::Size _inter_size;
	dcp::Size _out_size;
	Scaler const * _scaler;
	Eyes _eyes;
	Part _part;
	ColourConversion _colour_conversion;
	PositionImage _subtitle;
};
