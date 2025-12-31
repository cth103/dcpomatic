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


#include "video_encoder.h"


using std::shared_ptr;
using boost::optional;


VideoEncoder::VideoEncoder(shared_ptr<const Film> film, Writer& writer)
	: _film(film)
	, _writer(writer)
	, _history(200)
{

}


int
VideoEncoder::video_frames_encoded() const
{
	return _history.events();
}


/** @return an estimate of the current number of frames we are encoding per second,
 *  if known.
 */
optional<float>
VideoEncoder::current_encoding_rate() const
{
	return _history.rate();
}
