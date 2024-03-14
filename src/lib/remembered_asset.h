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


#ifndef DCPOMATIC_REMEMBERED_ASSET_H
#define DCPOMATIC_REMEMBERED_ASSET_H


#include "dcpomatic_time.h"
#include <libcxml/cxml.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>


class RememberedAsset
{
public:
	explicit RememberedAsset(cxml::ConstNodePtr node);

	RememberedAsset(boost::filesystem::path filename, dcpomatic::DCPTimePeriod period, std::string identifier)
		: _filename(filename)
		, _period(period)
		, _identifier(std::move(identifier))
	{}

	void as_xml(xmlpp::Element* parent) const;

	boost::filesystem::path filename() const {
		return _filename;
	}

	dcpomatic::DCPTimePeriod period() const {
		return _period;
	}

	std::string identifier() const {
		return _identifier;
	}

private:
	boost::filesystem::path _filename;
	dcpomatic::DCPTimePeriod _period;
	std::string _identifier;
};


boost::optional<boost::filesystem::path> find_asset(
	std::vector<RememberedAsset> const& assets, boost::filesystem::path directory, dcpomatic::DCPTimePeriod period, std::string identifier
	);


void clean_up_asset_directory(boost::filesystem::path directory);


void preserve_assets(boost::filesystem::path search, boost::filesystem::path assets_path);


#endif

