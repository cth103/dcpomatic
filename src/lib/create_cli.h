/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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


#include "types.h"
#include <dcp/types.h>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <vector>


class DCPContentType;
class Ratio;


class CreateCLI
{
public:
	CreateCLI (int argc, char* argv[]);

	struct Content {
		boost::filesystem::path path;
		VideoFrameType frame_type;
		boost::optional<dcp::Channel> channel;
		boost::optional<float> gain;
		boost::optional<boost::filesystem::path> kdm;
	};

	bool version;
	std::string name;
	boost::optional<std::string> template_name;
	bool encrypt;
	bool threed;
	DCPContentType const * dcp_content_type;
	boost::optional<int> dcp_frame_rate;
	Ratio const * container_ratio;
	int still_length;
	dcp::Standard standard;
	bool no_use_isdcf_name;
	boost::optional<boost::filesystem::path> config_dir;
	boost::optional<boost::filesystem::path> output_dir;
	boost::optional<std::string> error;
	std::vector<Content> content;
	bool twok;
	bool fourk;
	boost::optional<int> j2k_bandwidth;

private:
	static std::string _help;
};
