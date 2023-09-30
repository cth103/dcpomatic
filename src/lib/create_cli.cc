/*
    Copyright (C) 2019-2022 Carl Hetherington <cth@carlh.net>

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


#include "compose.hpp"
#include "config.h"
#include "create_cli.h"
#include "dcp_content_type.h"
#include "dcpomatic_log.h"
#include "film.h"
#include "ratio.h"
#include <dcp/raw_convert.h>
#include <iostream>
#include <string>


using std::cout;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
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
	"  -s, --still-length <n>        number of seconds that still content should last\n"
	"      --standard <standard>     SMPTE or interop (default SMPTE)\n"
	"      --no-use-isdcf-name       do not use an ISDCF name; use the specified name unmodified\n"
	"      --config <dir>            directory containing config.xml and cinemas.xml\n"
	"      --twok                    make a 2K DCP instead of choosing a resolution based on the content\n"
	"      --fourk                   make a 4K DCP instead of choosing a resolution based on the content\n"
	"  -o, --output <dir>            output directory\n"
	"      --threed                  make a 3D DCP\n"
	"      --j2k-bandwidth <Mbit/s>  J2K bandwidth in Mbit/s\n"
	"      --left-eye                next piece of content is for the left eye\n"
	"      --right-eye               next piece of content is for the right eye\n"
	"      --channel <channel>       next piece of content should be mapped to audio channel L, R, C, Lfe, Ls, Rs, BsL, BsR, HI, VI\n"
	"      --gain                    next piece of content should have the given audio gain (in dB)\n"
	"      --cpl <id>                CPL ID to use from the next piece of content (which is a DCP)\n"
	"      --kdm <file>              KDM for next piece of content\n";


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


template <class T>
void
argument_option (
	int& n,
	int argc,
	char* argv[],
	string short_name,
	string long_name,
	bool* claimed,
	optional<string>* error,
	boost::optional<T>* out,
	std::function<boost::optional<T> (string s)> convert = [](string s) { return dcp::raw_convert<T>(s); }
	)
{
	string const a = argv[n];
	if (a != short_name && a != long_name) {
		return;
	}

	if ((n + 1) >= argc) {
		**error = String::compose("%1: option %2 requires an argument", argv[0], long_name);
		return;
	}

	auto const arg = argv[++n];
	auto const value = convert(arg);
	if (!value) {
		*error = String::compose("%1: %2 is not valid for %3", argv[0], arg, long_name);
		*claimed = true;
		return;
	}

	*out = value;
	*claimed = true;
}


CreateCLI::CreateCLI (int argc, char* argv[])
	: version (false)
	, encrypt (false)
	, threed (false)
	, dcp_content_type (nullptr)
	, container_ratio (nullptr)
	, standard (dcp::Standard::SMPTE)
	, no_use_isdcf_name (false)
	, twok (false)
	, fourk (false)
{
	string dcp_content_type_string = "TST";
	string container_ratio_string;
	string standard_string = "SMPTE";
	int dcp_frame_rate_int = 0;
	string template_name_string;
	int j2k_bandwidth_int = 0;
	auto next_frame_type = VideoFrameType::TWO_D;
	optional<dcp::Channel> channel;
	optional<float> gain;
	optional<boost::filesystem::path> kdm;
	optional<std::string> cpl;

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
		} else if (a == "--threed") {
			threed = claimed = true;
		} else if (a == "--left-eye") {
			next_frame_type = VideoFrameType::THREE_D_LEFT;
			claimed = true;
		} else if (a == "--right-eye") {
			next_frame_type = VideoFrameType::THREE_D_RIGHT;
			claimed = true;
		} else if (a == "--twok") {
			twok = true;
			claimed = true;
		} else if (a == "--fourk") {
			fourk = true;
			claimed = true;
		}

		std::function<optional<string> (string s)> string_to_string = [](string s) {
			return s;
		};

		std::function<optional<boost::filesystem::path> (string s)> string_to_path = [](string s) {
			return boost::filesystem::path(s);
		};

		std::function<optional<int> (string s)> string_to_nonzero_int = [](string s) {
			auto const value = dcp::raw_convert<int>(s);
			if (value == 0) {
				return boost::optional<int>();
			}
			return boost::optional<int>(value);
		};

		argument_option(i, argc, argv, "-n", "--name",             &claimed, &error, &name);
		argument_option(i, argc, argv, "-t", "--template",         &claimed, &error, &template_name_string);
		argument_option(i, argc, argv, "-c", "--dcp-content-type", &claimed, &error, &dcp_content_type_string);
		argument_option(i, argc, argv, "-f", "--dcp-frame-rate",   &claimed, &error, &dcp_frame_rate_int);
		argument_option(i, argc, argv, "",   "--container-ratio",  &claimed, &error, &container_ratio_string);
		argument_option(i, argc, argv, "-s", "--still-length",     &claimed, &error, &still_length, string_to_nonzero_int);
		argument_option(i, argc, argv, "",   "--standard",         &claimed, &error, &standard_string);
		argument_option(i, argc, argv, "",   "--config",           &claimed, &error, &config_dir, string_to_path);
		argument_option(i, argc, argv, "-o", "--output",           &claimed, &error, &output_dir, string_to_path);
		argument_option(i, argc, argv, "",   "--j2k-bandwidth",    &claimed, &error, &j2k_bandwidth_int);

		std::function<optional<dcp::Channel> (string)> convert_channel = [](string channel) -> optional<dcp::Channel>{
			if (channel == "L") {
				return dcp::Channel::LEFT;
			} else if (channel == "R") {
				return dcp::Channel::RIGHT;
			} else if (channel == "C") {
				return dcp::Channel::CENTRE;
			} else if (channel == "Lfe") {
				return dcp::Channel::LFE;
			} else if (channel == "Ls") {
				return dcp::Channel::LS;
			} else if (channel == "Rs") {
				return dcp::Channel::RS;
			} else if (channel == "BsL") {
				return dcp::Channel::BSL;
			} else if (channel == "BsR") {
				return dcp::Channel::BSR;
			} else if (channel == "HI") {
				return dcp::Channel::HI;
			} else if (channel == "VI") {
				return dcp::Channel::VI;
			} else {
				return {};
			}
		};

		argument_option(i, argc, argv, "", "--channel", &claimed, &error, &channel, convert_channel);
		argument_option(i, argc, argv, "", "--gain", &claimed, &error, &gain);
		argument_option(i, argc, argv, "", "--kdm", &claimed, &error, &kdm, string_to_path);
		/* It shouldn't be necessary to use this string_to_string here, but using the other argument_option()
		 * causes an odd compile error on Ubuntu 18.04.
		 */
		argument_option(i, argc, argv, "", "--cpl", &claimed, &error, &cpl, string_to_string);

		if (!claimed) {
			if (a.length() > 2 && a.substr(0, 2) == "--") {
				error = String::compose("%1: unrecognised option '%2'", argv[0], a) + String::compose(_help, argv[0]);
				return;
			} else {
				Content c;
				c.path = a;
				c.frame_type = next_frame_type;
				c.channel = channel;
				c.gain = gain;
				c.kdm = kdm;
				c.cpl = cpl;
				content.push_back (c);
				next_frame_type = VideoFrameType::TWO_D;
				channel = {};
				gain = {};
			}
		}

		++i;
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

	if (!container_ratio_string.empty()) {
		container_ratio = Ratio::from_id (container_ratio_string);
		if (!container_ratio) {
			error = String::compose("%1: unrecognised container ratio %2", argv[0], container_ratio_string);
			return;
		}
	}

	if (standard_string != "SMPTE" && standard_string != "interop") {
		error = String::compose("%1: standard must be SMPTE or interop", argv[0]);
		return;
	}

	if (standard_string == "interop") {
		standard = dcp::Standard::INTEROP;
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


shared_ptr<Film>
CreateCLI::make_film() const
{
	auto film = std::make_shared<Film>(output_dir);
	dcpomatic_log = film->log();
	dcpomatic_log->set_types(Config::instance()->log_types());
	if (template_name) {
		film->use_template(template_name.get());
	}
	film->set_name(name);

	if (container_ratio) {
		film->set_container(container_ratio);
	}
	film->set_dcp_content_type(dcp_content_type);
	film->set_interop(standard == dcp::Standard::INTEROP);
	film->set_use_isdcf_name(!no_use_isdcf_name);
	film->set_encrypted(encrypt);
	film->set_three_d(threed);
	if (twok) {
		film->set_resolution(Resolution::TWO_K);
	}
	if (fourk) {
		film->set_resolution(Resolution::FOUR_K);
	}
	if (j2k_bandwidth) {
		film->set_j2k_bandwidth(*j2k_bandwidth);
	}

	int channels = 6;
	for (auto cli_content: content) {
		if (cli_content.channel) {
			channels = std::max(channels, static_cast<int>(*cli_content.channel) + 1);
		}
	}
	if (channels % 1) {
		++channels;
	}

	film->set_audio_channels(channels);

	return film;
}

