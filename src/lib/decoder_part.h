/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_DECODER_PART_H
#define DCPOMATIC_DECODER_PART_H

#include "dcpomatic_time.h"
#include <boost/optional.hpp>

class Decoder;
class Log;

class DecoderPart
{
public:
	DecoderPart (Decoder* parent, boost::shared_ptr<Log> log);
	virtual ~DecoderPart () {}

	virtual ContentTime position () const = 0;
	virtual void seek () = 0;

	void set_ignore (bool i) {
		_ignore = i;
	}

	bool ignore () const {
		return _ignore;
	}

protected:
	Decoder* _parent;
	boost::shared_ptr<Log> _log;

private:
	bool _ignore;
};

#endif
