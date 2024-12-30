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


#include "dcpomatic_assert.h"
#include "remembered_asset.h"
#include <dcp/filesystem.h>
#include <libcxml/cxml.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <fmt/format.h>


using std::string;
using std::vector;


RememberedAsset::RememberedAsset(cxml::ConstNodePtr node)
{
	_filename = node->string_child("Filename");
	auto period_node = node->node_child("Period");
	DCPOMATIC_ASSERT(period_node);

	_period = {
		dcpomatic::DCPTime(period_node->number_child<int64_t>("From")),
		dcpomatic::DCPTime(period_node->number_child<int64_t>("To"))
	};

	_identifier = node->string_child("Identifier");
}


void
RememberedAsset::as_xml(xmlpp::Element* parent) const
{
	cxml::add_text_child(parent, "Filename", _filename.string());
	auto period_node = cxml::add_child(parent, "Period");
	cxml::add_text_child(period_node, "From", fmt::to_string(_period.from.get()));
	cxml::add_text_child(period_node, "To", fmt::to_string(_period.to.get()));
	cxml::add_text_child(parent, "Identifier", _identifier);
}


boost::optional<boost::filesystem::path>
find_asset(vector<RememberedAsset> const& assets, boost::filesystem::path directory, dcpomatic::DCPTimePeriod period, string identifier)
{
	for (auto path: dcp::filesystem::recursive_directory_iterator(directory)) {
		auto iter = std::find_if(assets.begin(), assets.end(), [period, identifier, path](RememberedAsset const& asset) {
			return asset.filename() == path.path().filename() && asset.period() == period && asset.identifier() == identifier;
		});
		if (iter != assets.end()) {
			return path.path();
		}
	}

	return {};
}


void
clean_up_asset_directory(boost::filesystem::path directory)
{
	/* We could do something more advanced here (e.g. keep the last N assets) but for now
	 * let's just clean the whole thing out.
	 */
	boost::system::error_code ec;
	dcp::filesystem::remove_all(directory, ec);
}


void
preserve_assets(boost::filesystem::path search, boost::filesystem::path assets_path)
{
	for (auto const& path: boost::filesystem::directory_iterator(search)) {
		if (path.path().extension() == ".mxf") {
			dcp::filesystem::rename(path.path(), assets_path / path.path().filename());
		}
	}
}
