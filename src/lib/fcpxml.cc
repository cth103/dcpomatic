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


#include "fcpxml.h"
#include <dcp/raw_convert.h>
#include <libcxml/cxml.h>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>


#include "i18n.h"


using std::map;
using std::string;
using std::vector;


static dcpomatic::ContentTime
convert_time(string const& time)
{
	vector<string> parts;
	boost::algorithm::split(parts, time, boost::is_any_of("/"));

	if (parts.size() != 2 || parts[1].empty() || parts[1][parts[1].length() - 1] != 's') {
		throw FCPXMLError(String::compose("Unexpected time format %1", time));
	}

	return dcpomatic::ContentTime{dcp::raw_convert<int64_t>(parts[0]) * dcpomatic::ContentTime::HZ / dcp::raw_convert<int64_t>(parts[1])};
}


dcpomatic::fcpxml::Sequence
dcpomatic::fcpxml::load(boost::filesystem::path xml_file)
{
	cxml::Document doc("fcpxml");
	doc.read_file(xml_file);

	auto project = doc.node_child("project");

	map<string, boost::filesystem::path> assets;
	for (auto asset: project->node_child("resources")->node_children("asset")) {
		assets[asset->string_attribute("name")] = asset->string_attribute("src");
	}

	auto sequence = Sequence{xml_file.parent_path()};
	for (auto video: project->node_child("sequence")->node_child("spine")->node_children("video")) {
		auto name = video->string_attribute("name");
		auto iter = assets.find(name);
		if (iter == assets.end()) {
			throw FCPXMLError(String::compose(_("Video refers to missing asset %1"), name));
		}

		auto start = convert_time(video->string_attribute("offset"));
		sequence.video.push_back(
			{
				iter->second,
				dcpomatic::ContentTimePeriod(start, start + convert_time(video->string_attribute("duration")))
			}
		);
	}

	return sequence;
}
