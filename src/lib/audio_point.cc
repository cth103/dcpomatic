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

#include "audio_point.h"
#include <dcp/raw_convert.h>
#include <libxml++/libxml++.h>

using std::string;
using dcp::raw_convert;

AudioPoint::AudioPoint ()
{
	for (int i = 0; i < COUNT; ++i) {
		_data[i] = 0;
	}
}

AudioPoint::AudioPoint (cxml::ConstNodePtr node)
{
	_data[PEAK] = node->number_child<float> ("Peak");
	_data[RMS] = node->number_child<float> ("RMS");
}

AudioPoint::AudioPoint (AudioPoint const & other)
{
	for (int i = 0; i < COUNT; ++i) {
		_data[i] = other._data[i];
	}
}

AudioPoint &
AudioPoint::operator= (AudioPoint const & other)
{
	if (this == &other) {
		return *this;
	}

	for (int i = 0; i < COUNT; ++i) {
		_data[i] = other._data[i];
	}

	return *this;
}

void
AudioPoint::as_xml (xmlpp::Element* parent) const
{
	parent->add_child ("Peak")->add_child_text (raw_convert<string> (_data[PEAK]));
	parent->add_child ("RMS")->add_child_text (raw_convert<string> (_data[RMS]));
}
