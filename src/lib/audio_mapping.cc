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

#include "audio_mapping.h"

using std::map;
using boost::optional;

AutomaticAudioMapping::AutomaticAudioMapping (int c)
	: _source_channels (c)
{

}

optional<libdcp::Channel>
AutomaticAudioMapping::source_to_dcp (int c) const
{
	if (c >= _source_channels) {
		return optional<libdcp::Channel> ();
	}

	if (_source_channels == 1) {
		/* mono sources to centre */
		return libdcp::CENTRE;
	}
	
	return static_cast<libdcp::Channel> (c);
}

optional<int>
AutomaticAudioMapping::dcp_to_source (libdcp::Channel c) const
{
	if (_source_channels == 1) {
		if (c == libdcp::CENTRE) {
			return 0;
		} else {
			return optional<int> ();
		}
	}

	if (static_cast<int> (c) >= _source_channels) {
		return optional<int> ();
	}
	
	return static_cast<int> (c);
}

int
AutomaticAudioMapping::dcp_channels () const
{
	if (_source_channels == 1) {
		/* The source is mono, so to put the mono channel into
		   the centre we need to generate a 5.1 soundtrack.
		*/
		return 6;
	}

	return _source_channels;
}

optional<int>
ConfiguredAudioMapping::dcp_to_source (libdcp::Channel c) const
{
	map<int, libdcp::Channel>::const_iterator i = _source_to_dcp.begin ();
	while (i != _source_to_dcp.end() && i->second != c) {
		++i;
	}

	if (i == _source_to_dcp.end ()) {
		return boost::none;
	}

	return i->first;
}

optional<libdcp::Channel>
ConfiguredAudioMapping::source_to_dcp (int c) const
{
	map<int, libdcp::Channel>::const_iterator i = _source_to_dcp.find (c);
	if (i == _source_to_dcp.end ()) {
		return boost::none;
	}

	return i->second;
}

	
