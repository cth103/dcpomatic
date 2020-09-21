/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#include "create_cli.h"
#include "dcp_content_type.h"
#include "ratio.h"
#include "config.h"
#include "compose.hpp"
#include <dcp/raw_convert.h>
#include <string>

#include <iostream>

using std::string;
using std::cout;
using boost::optional;

string CreateCLI::_help =
	"\nSyntax: %1 [OPTION] <CONTENT> [OPTION] [<CONTENT> ...]\n"
	"  -v, --version                 show DCP-o-matic version\n"
	"  -h, --help                    show this help\n"
	"  -n, --name <name>             film name\n"
	"  -t, --template <name>         template name\n"
	"  -e, --encrypt                 make an encrypted DCP\n"
	"  -c, --dcp-content-type <type> FTR, SHR, TLR, TST, XSN, RTG, TSR, POL, PSA or ADV\n"
	"  -f, --dcp-frame-rate <rate>   set DCP video frame rate (otherwise guessed from content)\n"
	"      --container-ratio <ratio> 119, 133, 137, 138, 166, 178, 185 or 239\n"
	"      --content-ratio <ratio>   119, 133, 137, 138, 166, 178, 185 or 239\n"
	"  -s, --still-length <n>        number of seconds that still content should last\n"
	"      --standard <standard>     SMPTE or interop (default SMPTE)\n"
	"      --no-use-isdcf-name       do not use an ISDCF name; use the specified name unmodified\n"
	"      --no-sign                 do not sign the DCP\n"
	"      --config <dir>            directory containing config.xml and cinemas.xml\n"
	"      --fourk                   make a 4K DCP rather than a 2K one\n"
	"  -o, --output <dir>            output directory\n"
	"      --threed                  make a 3D DCP\n"
	"      --j2k-bandwidth <Mbit/s>  J2K bandwidth in Mbit/s\n"
	"      --left-eye                next piece of content is for the left eye\n"
	"      --right-eye               next piece of content is for the right eye\n";

template <class T>
void
argument_option (int& n, int argc, char* argv[], string short_name, string long_name, bool* claimed, optional<string>* error, T* out)
{
	string const a = argv[n];
	if (a != short_name && a != long_name) {
		return;
	}

	if ((n + 1) >= argc) {
		**error = String::compose("%1: option %2 requires an argument", argv[0], long_name);
		return;
	}

	*out = dcp::raw_convert<T>(string(argv[++n]));
	*claimed = true;
}

CreateCLI::CreateCLI (int argc, char* argv[])
	: version (false)
	, encrypt (false)
	, threed (false)
	, dcp_content_type (0)
	, container_ratio (0)
	, content_ratio (0)
	, still_length (10)
	, standard (dcp::SMPTE)
	, no_use_isdcf_name (false)
	, no_sign (false)
	, fourk (false)
{
	string dcp_content_type_string = "TST";
	string content_ratio_string;
	string container_ratio_string;
	string standard_string = "SMPTE";
	int dcp_frame_rate_int = 0;
	string template_name_string;
	string config_dir_string;
	string output_dir_string;
	int j2k_bandwidth_int = 0;
	VideoFrameType next_frame_type = VIDEO_FRAME_TYPE_2D;

	int i = 1;
	while (i < argc) {
		string const a = argv[i];
		bool claimed = false;

		if (a == "-v" || a == "--version") {
			version = true;
			return;
		} else if (a == "-h" || a == "--help") {
			error = "Create a film directory (ready for making a DCP) or metadata file from some content files.\n"
				"A film directory will be created if -o or --output is specified, otherwise a metadata file\n"
				"will be written to stdout.\n" + String::compose(_help, argv[0]);
			return;
		}

		if (a == "-e" || a == "--encrypt") {
			encrypt = claimed = true;
		} else if (a == "--no-use-isdcf-name") {
			no_use_isdcf_name = claimed = true;
		} else if (a == "--no-sign") {
			no_sign = claimed = true;
		} else if (a == "--threed") {
			threed = claimed = true;
		} else if (a == "--left-eye") {
			next_frame_type = VIDEO_FRAME_TYPE_3D_LEFT;
			claimed = true;
		} else if (a == "--right-eye") {
			next_frame_type = VIDEO_FRAME_TYPE_3D_RIGHT;
			claimed = true;
		} else if (a == "--fourk") {
			fourk = true;
			claimed = true;
		}

		argument_option(i, argc, argv, "-n", "--name",             &claimed, &error, &name);
		argument_option(i, argc, argv, "-t", "--template",         &claimed, &error, &template_name_string);
		argument_option(i, argc, argv, "-c", "--dcp-content-type", &claimed, &error, &dcp_content_type_string);
		argument_option(i, argc, argv, "-f", "--dcp-frame-rate",   &claimed, &error, &dcp_frame_rate_int);
		argument_option(i, argc, argv, "",   "--container-ratio",  &claimed, &error, &container_ratio_string);
		argument_option(i, argc, argv, "",   "--content-ratio",    &claimed, &error, &content_ratio_string);
		argument_option(i, argc, argv, "-s", "--still-length",     &claimed, &error, &still_length);
		argument_option(i, argc, argv, "",   "--standard",         &claimed, &error, &standard_string);
		argument_option(i, argc, argv, "",   "--config",           &claimed, &error, &config_dir_string);
		argument_option(i, argc, argv, "-o", "--output",           &claimed, &error, &output_dir_string);
		argument_option(i, argc, argv, "",   "--j2k-bandwidth",    &claimed, &error, &j2k_bandwidth_int);

		if (!claimed) {
			if (a.length() > 2 && a.substr(0, 2) == "--") {
				error = String::compose("%1: unrecognised option '%2'", argv[0], a) + String::compose(_help, argv[0]);
				return;
			} else {
				Content c;
				c.path = a;
				c.frame_type = next_frame_type;
				content.push_back (c);
				next_frame_type = VIDEO_FRAME_TYPE_2D;
			}
		}

		++i;
	}

	if (!config_dir_string.empty()) {
		config_dir = config_dir_string;
	}

	if (!output_dir_string.empty()) {
		output_dir = output_dir_string;
	}

	if (!template_name_string.empty()) {
		template_name = template_name_string;
	}

	if (dcp_frame_rate_int) {
		dcp_frame_rate = dcp_frame_rate_int;
	}

	if (j2k_bandwidth_int) {
		j2k_bandwidth = j2k_bandwidth_int * 1000000;
	}

	dcp_content_type = DCPContentType::from_isdcf_name(dcp_content_type_string);
	if (!dcp_content_type) {
		error = String::compose("%1: unrecognised DCP content type '%2'", argv[0], dcp_content_type_string);
		return;
	}

	if (content_ratio_string.empty()) {
		error = String::compose("%1: missing required option --content-ratio", argv[0]);
		return;
	}

	content_ratio = Ratio::from_id (content_ratio_string);
	if (!content_ratio) {
		error = String::compose("%1: unrecognised content ratio %2", content_ratio_string);
		return;
	}

	if (container_ratio_string.empty()) {
		container_ratio_string = content_ratio_string;
	}

	container_ratio = Ratio::from_id (container_ratio_string);
	if (!container_ratio) {
		error = String::compose("%1: unrecognised container ratio %2", argv[0], container_ratio_string);
		return;
	}

	if (standard_string != "SMPTE" && standard_string != "interop") {
		error = String::compose("%1: standard must be SMPTE or interop", argv[0]);
		return;
	}

	if (standard_string == "interop") {
		standard = dcp::INTEROP;
	}

	if (content.empty()) {
		error = String::compose("%1: no content specified", argv[0]);
		return;
	}

	if (name.empty()) {
		name = content[0].path.leaf().string();
	}

	if (j2k_bandwidth && (*j2k_bandwidth < 10000000 || *j2k_bandwidth > Config::instance()->maximum_j2k_bandwidth())) {
		error = String::compose("%1: j2k-bandwidth must be between 10 and %2 Mbit/s", argv[0], (Config::instance()->maximum_j2k_bandwidth() / 1000000));
		return;
	}
}
