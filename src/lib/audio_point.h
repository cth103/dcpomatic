/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_AUDIO_POINT_H
#define DCPOMATIC_AUDIO_POINT_H

#include <libcxml/cxml.h>

namespace xmlpp {
	class Element;
}

class AudioPoint
{
public:
	enum Type {
		PEAK,
		RMS,
		COUNT
	};

	AudioPoint ();
	AudioPoint (cxml::ConstNodePtr node);
	AudioPoint (AudioPoint const &);
	AudioPoint& operator= (AudioPoint const &);

	void as_xml (xmlpp::Element *) const;

	inline float& operator[] (int t) {
		return _data[t];
	}

private:
	float _data[COUNT];
};

#endif
