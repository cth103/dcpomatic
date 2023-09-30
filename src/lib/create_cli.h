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


#include "video_frame_type.h"
#include <dcp/types.h>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <vector>


class DCPContentType;
class Film;
class Ratio;

struct create_cli_test;


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
		boost::optional<std::string> cpl;
	};

	bool version;
	boost::optional<int> dcp_frame_rate;
	boost::optional<int> still_length;
	boost::optional<boost::filesystem::path> config_dir;
	boost::optional<boost::filesystem::path> output_dir;
	boost::optional<std::string> error;
	std::vector<Content> content;

	std::shared_ptr<Film> make_film() const;

private:
	friend struct ::create_cli_test;

	boost::optional<std::string> _template_name;
	std::string _name;
	Ratio const* _container_ratio = nullptr;
	bool _encrypt = false;
	bool _twod = false;
	bool _threed = false;
	DCPContentType const* _dcp_content_type = nullptr;
	dcp::Standard _standard = dcp::Standard::SMPTE;
	bool _no_use_isdcf_name = false;
	bool _twok = false;
	bool _fourk = false;
	boost::optional<int> _j2k_bandwidth;

	static std::string _help;
};
