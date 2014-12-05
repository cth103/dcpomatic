/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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
#include "audio_content.h"
extern "C" {
#include <libswresample/swresample.h>
}
#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>

class AudioBuffers;

class Resampler : public boost::noncopyable
{
public:
	Resampler (int, int, int);
	~Resampler ();

	boost::shared_ptr<const AudioBuffers> run (boost::shared_ptr<const AudioBuffers>);
	boost::shared_ptr<const AudioBuffers> flush ();

private:	
	SwrContext* _swr_context;
	int _in_rate;
	int _out_rate;
	int _channels;
};
