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

#include <vector>
#include <map>
#include <boost/optional.hpp>
#include <libdcp/types.h>

class AudioMapping
{
public:
	virtual boost::optional<libdcp::Channel> source_to_dcp (int c) const = 0;
	virtual boost::optional<int> dcp_to_source (libdcp::Channel c) const = 0;
};

class AutomaticAudioMapping : public AudioMapping
{
public:
	AutomaticAudioMapping (int);

	boost::optional<libdcp::Channel> source_to_dcp (int c) const;
	boost::optional<int> dcp_to_source (libdcp::Channel c) const;
	int dcp_channels () const;
	
private:
	int _source_channels;
};

class ConfiguredAudioMapping : public AudioMapping
{
public:
	boost::optional<libdcp::Channel> source_to_dcp (int c) const;
	boost::optional<int> dcp_to_source (libdcp::Channel c) const;

private:
	std::map<int, libdcp::Channel> _source_to_dcp;
};
