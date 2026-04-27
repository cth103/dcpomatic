/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_time.h"
#include <boost/filesystem.hpp>


class DCPAsset
{
public:
	enum class Type {
		VIDEO,
		AUDIO,
		TEXT
	};

	DCPAsset(Type type, boost::filesystem::path file, dcpomatic::DCPTimePeriod period)
		: _type(type)
		, _file(file)
		, _period(period)
	{}

	Type type() const {
		return _type;
	}
	boost::filesystem::path file() const {
		return _file;
	}
	dcpomatic::DCPTimePeriod period() const {
		return _period;
	}

private:
	Type _type;
	boost::filesystem::path _file;
	dcpomatic::DCPTimePeriod _period;
};

