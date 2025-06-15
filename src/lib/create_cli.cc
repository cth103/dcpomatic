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


#include "audio_content.h"
#include "compose.hpp"
#include "config.h"
#include "content_factory.h"
#include "create_cli.h"
#include "cross.h"
#include "dcp_content.h"
#include "dcp_content_type.h"
#include "dcpomatic_log.h"
#include "film.h"
#include "image_content.h"
#include "guess_crop.h"
#include "job_manager.h"
#include "ratio.h"
#include "variant.h"
#include "video_content.h"
#include <dcp/raw_convert.h>
#include <fmt/format.h>
#include <iostream>
#include <string>


using std::dynamic_pointer_cast;
using std::function;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;


static
string
help()
{
	string colour_conversions = "";
	for (auto const& conversion: PresetColourConversion::all()) {
		colour_conversions += conversion.id + ", ";
	}
	DCPOMATIC_ASSERT(colour_conversions.length() > 2);
	colour_conversions = colour_conversions.substr(0, colour_conversions.length() - 2);

	return string("\nSyntax: %1 [OPTION] <CONTENT> [OPTION] [<CONTENT> ...]\n") +
		variant::insert_dcpomatic("  -v, --version                 show %1 version\n") +
		"  -h, --help                    show this help\n"
		"  -n, --name <name>             film name\n"
		"  -t, --template <name>         template name\n"
		"      --no-encrypt              make an unencrypted DCP\n"
		"  -e, --encrypt                 make an encrypted DCP\n"
		"  -c, --dcp-content-type <type> FTR, SHR, TLR, TST, XSN, RTG, TSR, POL, PSA or ADV\n"
		"  -f, --dcp-frame-rate <rate>   set DCP video frame rate (otherwise guessed from content)\n"
		"      --container-ratio <ratio> 119, 133, 137, 138, 166, 178, 185 or 239\n"
		"  -s, --still-length <n>        number of seconds that still content should last\n"
		"      --auto-crop-threshold <n> threshold to use for 'black' when auto-cropping\n"
		"      --standard <standard>     SMPTE or interop (default SMPTE)\n"
		"      --no-use-isdcf-name       do not use an ISDCF name; use the specified name unmodified\n"
		"      --config <dir>            directory containing config.xml and cinemas.sqlite3\n"
		"      --twok                    make a 2K DCP instead of choosing a resolution based on the content\n"
		"      --fourk                   make a 4K DCP instead of choosing a resolution based on the content\n"
		"  -a, --audio-channels <n>      specify the number of audio channels in the DCP\n"
		"  -o, --output <dir>            output directory\n"
		"      --twod                    make a 2D DCP\n"
		"      --threed                  make a 3D DCP\n"
		"      --video-bit-rate <Mbit/s> J2K bandwidth in Mbit/s\n"
		"      --left-eye                next piece of content is for the left eye\n"
		"      --right-eye               next piece of content is for the right eye\n"
		"      --auto-crop               next piece of content should be auto-cropped\n"
		"      --colourspace             next piece of content is in the given colourspace: " + colour_conversions + "\n"
		"      --colorspace              same as --colourspace\n"
		"      --channel <channel>       next piece of content should be mapped to audio channel L, R, C, Lfe, Ls, Rs, BsL, BsR, HI, VI\n"
		"      --gain                    next piece of content should have the given audio gain (in dB)\n"
		"      --fade-in <seconds>       next piece of content should have the given fade-in (in seconds)\n"
		"      --fade-out <seconds>      next piece of content should have the given fade-out (in seconds)\n"
		"      --cpl <id>                CPL ID to use from the next piece of content (which is a DCP)\n"
		"      --kdm <file>              KDM for next piece of content\n";
}


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


template <>
void
argument_option(int& n, int argc, char* argv[], string short_name, string long_name, bool* claimed, optional<string>* error, string* out)
{
	string const a = argv[n];
	if (a != short_name && a != long_name) {
		return;
	}

	if ((n + 1) >= argc) {
		**error = String::compose("%1: option %2 requires an argument", argv[0], long_name);
		return;
	}

	*out = string(argv[++n]);
	*claimed = true;
}


template <class T>
void
argument_option(
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


CreateCLI::CreateCLI(int argc, char* argv[])
	: version(false)
{
	optional<string> dcp_content_type_string;
	string container_ratio_string;
	optional<string> standard_string;
	int dcp_frame_rate_int = 0;
	string template_name_string;
	int64_t video_bit_rate_int = 0;
	optional<int> audio_channels;
	optional<string> next_colour_conversion;
	auto next_frame_type = VideoFrameType::TWO_D;
	auto next_auto_crop = false;
	optional<dcp::Channel> channel;
	optional<float> gain;
	optional<float> fade_in;
	optional<float> fade_out;
	optional<boost::filesystem::path> kdm;
	optional<string> cpl;

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
				"will be written to stdout.\n" + String::compose(help(), argv[0]);
			return;
		}

		if (a == "--no-encrypt") {
			_no_encrypt = claimed = true;
		} else if (a == "-e" || a == "--encrypt") {
			_encrypt = claimed = true;
		} else if (a == "--no-use-isdcf-name") {
			_no_use_isdcf_name = claimed = true;
		} else if (a == "--twod") {
			_twod = claimed = true;
		} else if (a == "--threed") {
			_threed = claimed = true;
		} else if (a == "--left-eye") {
			next_frame_type = VideoFrameType::THREE_D_LEFT;
			claimed = true;
		} else if (a == "--right-eye") {
			next_frame_type = VideoFrameType::THREE_D_RIGHT;
			claimed = true;
		} else if (a == "--auto-crop") {
			next_auto_crop = true;
			claimed = true;
		} else if (a == "--twok") {
			_twok = true;
			claimed = true;
		} else if (a == "--fourk") {
			_fourk = true;
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

		std::function<optional<int> (string s)> string_to_int = [](string s) {
			return boost::optional<int>(dcp::raw_convert<int>(s));
		};

		argument_option(i, argc, argv, "-n", "--name",             &claimed, &error, &_name);
		argument_option(i, argc, argv, "-t", "--template",         &claimed, &error, &template_name_string);
		/* See comment below about --cpl */
		argument_option(i, argc, argv, "-c", "--dcp-content-type", &claimed, &error, &dcp_content_type_string, string_to_string);
		argument_option(i, argc, argv, "-f", "--dcp-frame-rate",   &claimed, &error, &dcp_frame_rate_int);
		argument_option(i, argc, argv, "",   "--container-ratio",  &claimed, &error, &container_ratio_string);
		argument_option(i, argc, argv, "-s", "--still-length",     &claimed, &error, &still_length, string_to_nonzero_int);
		argument_option(i, argc, argv, "",   "--auto-crop-threshold", &claimed, &error, &auto_crop_threshold, string_to_int);
		/* See comment below about --cpl */
		argument_option(i, argc, argv, "",   "--standard",         &claimed, &error, &standard_string, string_to_string);
		argument_option(i, argc, argv, "",   "--config",           &claimed, &error, &config_dir, string_to_path);
		argument_option(i, argc, argv, "-o", "--output",           &claimed, &error, &output_dir, string_to_path);
		argument_option(i, argc, argv, "",   "--video-bit-rate",   &claimed, &error, &video_bit_rate_int);
		/* Similar to below, not using string_to_int here causes on add compile error on Ubuntu 1{6,8}.04 */
		argument_option(i, argc, argv, "-a", "--audio-channels",   &claimed, &error, &audio_channels, string_to_int);
		argument_option(i, argc, argv, ""  , "--colourspace",      &claimed, &error, &next_colour_conversion, string_to_string);
		argument_option(i, argc, argv, ""  , "--colorspace",       &claimed, &error, &next_colour_conversion, string_to_string);

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
			} else if (channel == "SLV") {
				return dcp::Channel::SIGN_LANGUAGE;
			} else {
				return {};
			}
		};

		argument_option(i, argc, argv, "", "--channel", &claimed, &error, &channel, convert_channel);
		argument_option(i, argc, argv, "", "--gain", &claimed, &error, &gain);
		argument_option(i, argc, argv, "", "--fade-in", &claimed, &error, &fade_in);
		argument_option(i, argc, argv, "", "--fade-out", &claimed, &error, &fade_out);
		argument_option(i, argc, argv, "", "--kdm", &claimed, &error, &kdm, string_to_path);
		/* It shouldn't be necessary to use this string_to_string here, but using the other argument_option()
		 * causes an odd compile error on Ubuntu 18.04.
		 */
		argument_option(i, argc, argv, "", "--cpl", &claimed, &error, &cpl, string_to_string);

		if (!claimed) {
			if (a.length() > 2 && a.substr(0, 2) == "--") {
				error = String::compose("%1: unrecognised option '%2'", argv[0], a) + String::compose(help(), argv[0]);
				return;
			} else {
				if (next_colour_conversion) {
					auto colour_conversions = PresetColourConversion::all();
					if (!std::any_of(
							colour_conversions.begin(),
							colour_conversions.end(),
							[next_colour_conversion](PresetColourConversion const& conversion) {
								return conversion.id == *next_colour_conversion;
							})) {
						error = fmt::format("{}: {} is not a recognised colourspace", argv[0], *next_colour_conversion);
						return;
					}
				}

				Content c;
				c.path = a;
				c.frame_type = next_frame_type;
				c.auto_crop = next_auto_crop;
				c.colour_conversion = next_colour_conversion;
				c.channel = channel;
				c.gain = gain;
				c.fade_in = fade_in;
				c.fade_out = fade_out;
				c.kdm = kdm;
				c.cpl = cpl;
				content.push_back(c);
				next_frame_type = VideoFrameType::TWO_D;
				next_auto_crop = false;
				next_colour_conversion = {};
				channel = {};
				gain = {};
				fade_in = {};
				fade_out = {};
			}
		}

		++i;
	}

	if (!template_name_string.empty()) {
		_template_name = template_name_string;
	}

	if (dcp_frame_rate_int) {
		dcp_frame_rate = dcp_frame_rate_int;
	}

	if (video_bit_rate_int) {
		_video_bit_rate = video_bit_rate_int * 1000000;
	}

	if (dcp_content_type_string) {
		_dcp_content_type = DCPContentType::from_isdcf_name(*dcp_content_type_string);
		if (!_dcp_content_type) {
			error = String::compose("%1: unrecognised DCP content type '%2'", argv[0], *dcp_content_type_string);
			return;
		}
	}

	if (!container_ratio_string.empty()) {
		_container_ratio = Ratio::from_id(container_ratio_string);
		if (!_container_ratio) {
			error = String::compose("%1: unrecognised container ratio %2", argv[0], container_ratio_string);
			return;
		}
	}

	if (standard_string) {
		if (*standard_string == "interop") {
			_standard = dcp::Standard::INTEROP;
		} else if (*standard_string == "SMPTE") {
			_standard = dcp::Standard::SMPTE;
		} else {
			error = String::compose("%1: standard must be SMPTE or interop", argv[0]);
			return;
		}
	}

	if (_twod && _threed) {
		error = String::compose("%1: specify one of --twod or --threed, not both", argv[0]);
	}

	if (_no_encrypt && _encrypt) {
		error = String::compose("%1: specify one of --no-encrypt or --encrypt, not both", argv[0]);
	}

	if (content.empty()) {
		error = String::compose("%1: no content specified", argv[0]);
		return;
	}

	if (_name.empty()) {
		_name = content[0].path.filename().string();
	}

	if (_video_bit_rate && (*_video_bit_rate < 10000000 || *_video_bit_rate > Config::instance()->maximum_video_bit_rate(VideoEncoding::JPEG2000))) {
		error = String::compose("%1: video-bit-rate must be between 10 and %2 Mbit/s", argv[0], (Config::instance()->maximum_video_bit_rate(VideoEncoding::JPEG2000) / 1000000));
		return;
	}

	/* See how many audio channels we would need to accommmodate the requested channel mappings */
	int channels_for_content_specs = 0;
	for (auto cli_content: content) {
		if (cli_content.channel) {
			channels_for_content_specs = std::max(channels_for_content_specs, static_cast<int>(*cli_content.channel) + 1);
		}
	}

	/* Complain if we asked for a smaller number */
	if (audio_channels && *audio_channels < channels_for_content_specs) {
		error = fmt::format("{}: cannot map audio as requested with only {} channels", argv[0], *audio_channels);
		return;
	}

	if (audio_channels && (*audio_channels % 2) != 0) {
		error = fmt::format("{}: audio channel count must be even", argv[0]);
		return;
	}

	if (audio_channels) {
		_audio_channels = *audio_channels;
	} else {
		_audio_channels = std::max(channels_for_content_specs, 6);
		if (_audio_channels % 2) {
			++_audio_channels;
		}
	}
}


shared_ptr<Film>
CreateCLI::make_film(function<void (string)> error) const
{
	auto film = std::make_shared<Film>(output_dir);
	dcpomatic_log = film->log();
	dcpomatic_log->set_types(Config::instance()->log_types());
	if (_template_name) {
		film->use_template(_template_name.get());
	} else {
		/* No template: apply our own CLI tool defaults to override the ones in Config.
		 * Maybe one day there will be no defaults in Config any more (as they'll be in
		 * a default template) and we can decide whether to use the default template
		 * or not.
		 */
		film->set_interop(false);
		film->set_dcp_content_type(DCPContentType::from_isdcf_name("TST"));
	}
	film->set_name(_name);

	if (_container_ratio) {
		film->set_container(_container_ratio);
	}
	if (_dcp_content_type) {
		film->set_dcp_content_type(_dcp_content_type);
	}
	if (_standard) {
		film->set_interop(*_standard == dcp::Standard::INTEROP);
	}
	film->set_use_isdcf_name(!_no_use_isdcf_name);
	if (_no_encrypt) {
		film->set_encrypted(false);
	} else if (_encrypt) {
		film->set_encrypted(true);
	}
	if (_twod) {
		film->set_three_d(false);
	} else if (_threed) {
		film->set_three_d(true);
	}
	if (_twok) {
		film->set_resolution(Resolution::TWO_K);
	}
	if (_fourk) {
		film->set_resolution(Resolution::FOUR_K);
	}
	if (_video_bit_rate) {
		film->set_video_bit_rate(VideoEncoding::JPEG2000, *_video_bit_rate);
	}

	film->set_audio_channels(_audio_channels);

	auto jm = JobManager::instance();

	for (auto cli_content: content) {
		auto const can = dcp::filesystem::canonical(cli_content.path);
		vector<shared_ptr<::Content>> film_content_list;

		if (dcp::filesystem::exists(can / "ASSETMAP") || (dcp::filesystem::exists(can / "ASSETMAP.xml"))) {
			auto dcp = make_shared<DCPContent>(can);
			film_content_list.push_back(dcp);
			if (cli_content.kdm) {
				dcp->add_kdm(dcp::EncryptedKDM(dcp::file_to_string(*cli_content.kdm)));
			}
			if (cli_content.cpl) {
				dcp->set_cpl(*cli_content.cpl);
			}
		} else {
			/* I guess it's not a DCP */
			film_content_list = content_factory(can);
		}

		for (auto film_content: film_content_list) {
			film->examine_and_add_content(film_content);
		}

		while (jm->work_to_do()) {
			dcpomatic_sleep_seconds(1);
		}

		while (signal_manager->ui_idle() > 0) {}

		for (auto film_content: film_content_list) {
			if (auto video = film_content->video) {
				auto const video_frame_rate = film_content->video_frame_rate().get_value_or(24);
				video->set_frame_type(cli_content.frame_type);
				if (cli_content.auto_crop) {
					auto crop = guess_crop_by_brightness(
						film,
						film_content,
						auto_crop_threshold.get_value_or(Config::instance()->auto_crop_threshold()),
						std::min(
							dcpomatic::ContentTime::from_seconds(1),
							dcpomatic::ContentTime::from_frames(
								video->length(),
								video_frame_rate
							)
						)
					);

					error(fmt::format(
						"Cropped {} to {} left, {} right, {} top and {} bottom",
						film_content->path(0).string(),
						crop.left,
						crop.right,
						crop.top,
						crop.bottom
					));

					video->set_crop(crop);
				}
				if (cli_content.colour_conversion) {
					video->set_colour_conversion(PresetColourConversion::from_id(*cli_content.colour_conversion).conversion);
				}
				if (cli_content.fade_in) {
					video->set_fade_in(dcpomatic::ContentTime::from_seconds(*cli_content.fade_in).frames_round(video_frame_rate));
				}
				if (cli_content.fade_out) {
					video->set_fade_out(dcpomatic::ContentTime::from_seconds(*cli_content.fade_out).frames_round(video_frame_rate));
				}
			}
			if (auto audio = film_content->audio) {
				if (cli_content.channel) {
					for (auto stream: audio->streams()) {
						AudioMapping mapping(stream->channels(), film->audio_channels());
						for (int channel = 0; channel < stream->channels(); ++channel) {
							mapping.set(channel, *cli_content.channel, 1.0f);
						}
						stream->set_mapping(mapping);
					}
				}
				if (cli_content.gain) {
					audio->set_gain(*cli_content.gain);
				}
				if (cli_content.fade_in) {
					audio->set_fade_in(dcpomatic::ContentTime::from_seconds(*cli_content.fade_in));
				}
				if (cli_content.fade_out) {
					audio->set_fade_out(dcpomatic::ContentTime::from_seconds(*cli_content.fade_out));
				}
			}
		}
	}

	if (dcp_frame_rate) {
		film->set_video_frame_rate(*dcp_frame_rate);
	}

	for (auto i: film->content()) {
		auto ic = dynamic_pointer_cast<ImageContent>(i);
		if (ic && ic->still()) {
			ic->video->set_length(still_length.get_value_or(10) * 24);
		}
	}

	if (jm->errors()) {
		for (auto i: jm->get()) {
			if (i->finished_in_error()) {
				error(fmt::format("{}\n", i->error_summary()));
				if (!i->error_details().empty()) {
					error(fmt::format("{}\n", i->error_details()));
				}
			}
		}
		return {};
	}

	return film;
}

