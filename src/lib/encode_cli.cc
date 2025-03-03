/*
    Copyright (C) 2012-2022 Carl Hetherington <cth@carlh.net>

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


#include "ansi.h"
#include "audio_content.h"
#include "config.h"
#include "cross.h"
#include "dcpomatic_log.h"
#include "encode_server_finder.h"
#include "ffmpeg_film_encoder.h"
#include "film.h"
#include "filter.h"
#ifdef DCPOMATIC_GROK
#include "grok/context.h"
#include "grok/util.h"
#endif
#include "hints.h"
#include "job_manager.h"
#include "json_server.h"
#include "log.h"
#include "make_dcp.h"
#include "ratio.h"
#include "transcode_job.h"
#include "util.h"
#include "variant.h"
#include "version.h"
#include "video_content.h"
#include <dcp/filesystem.h>
#include <dcp/version.h>
#include <fmt/format.h>
#include <getopt.h>
#include <iostream>
#include <iomanip>


using std::cout;
using std::dynamic_pointer_cast;
using std::function;
using std::list;
using std::make_shared;
using std::pair;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;


static void
help(function <void (string)> out)
{
	out(fmt::format("Syntax: {} [OPTION] [COMMAND] [<PARAMETER>]\n", program_name));

	out("\nCommands:\n\n");
	out("  make-dcp <FILM>              make DCP from the given film; default if no other command is specified\n");
	out(variant::insert_dcpomatic("  list-servers                 display a list of encoding servers that %1 can use (until Ctrl-C)\n"));
	out("  dump <FILM>                  show a summary of the film's settings\n");
#ifdef DCPOMATIC_GROK
	out("  config-params                list the parameters that can be set with `config`\n");
	out("  config <PARAMETER> <VALUE>   set a DCP-o-matic configuration value\n");
	out("  list-gpus                    list available GPUs\n");
#endif

	out("\nOptions:\n\n");
	out(variant::insert_dcpomatic("  -v, --version                     show %1 version\n"));
	out("  -h, --help                        show this help\n");
	out("  -f, --flags                       show flags passed to C++ compiler on build\n");
	out("  -n, --no-progress                 do not print progress to stdout\n");
	out("  -r, --no-remote                   do not use any remote servers\n");
	out("  -t, --threads                     specify number of local encoding threads (overriding configuration)\n");
	out("  -j, --json <port>                 run a JSON server on the specified port\n");
	out("  -k, --keep-going                  keep running even when the job is complete\n");
	out("  -s, --servers <file>              specify servers to use in a text file\n");
	out(variant::insert_dcpomatic("  -l, --list-servers                just display a list of encoding servers that %1 is configured to use; don't encode\n"));
	out("                                      (deprecated - use the list-servers command instead)\n");
	out("  -d, --dcp-path                    echo DCP's path to stdout on successful completion (implies -n)\n");
	out("  -c, --config <dir>                directory containing config.xml and cinemas.xml\n");
	out("      --dump                        just dump a summary of the film's settings; don't encode\n");
	out("                                      (deprecated - use the dump command instead)\n");
	out("      --no-check                    don't check project's content files for changes before making the DCP\n");
	out("      --export-format <format>      export project to a file, rather than making a DCP: specify mov or mp4\n");
	out("      --export-filename <filename>  filename to export to with --export-format\n");
	out("      --hints                       analyze film for hints before encoding and abort if any are found\n");
	out("\ne.g.\n");
	out(fmt::format("\n  {} -t 4 make-dcp my_great_movie\n", program_name));
	out(fmt::format("\n  {} config grok-licence 12345ABCD\n", program_name));
	out("\n");
}


static void
print_dump(function<void (string)> out, shared_ptr<Film> film)
{
	out(fmt::format("{}\n", film->dcp_name(true)));
	out(fmt::format("{} at {}\n", film->container()->container_nickname(), film->resolution() == Resolution::TWO_K ? "2K" : "4K"));
	out(fmt::format("{}Mbit/s\n", film->video_bit_rate(film->video_encoding()) / 1000000));
	out(fmt::format("Duration {}\n", film->length().timecode(film->video_frame_rate())));
	out(fmt::format("Output {}fps {} {}kHz\n", film->video_frame_rate(), film->three_d() ? "3D" : "2D", film->audio_frame_rate() / 1000));
	out(fmt::format("{} {}\n", film->interop() ? "Inter-Op" : "SMPTE", film->encrypted() ? "encrypted" : "unencrypted"));

	for (auto c: film->content()) {
		out(fmt::format("\n{}\n", c->path(0).string()));
		out(fmt::format("\tat {} length {} start trim {} end trim {}\n", c->position().seconds(), c->full_length(film).seconds(), c->trim_start().seconds(), c->trim_end().seconds()));

		if (c->video && c->video->size()) {
			out(fmt::format("\t{}x{}\n", c->video->size()->width, c->video->size()->height));
			out(fmt::format("\t{}fps\n", c->active_video_frame_rate(film)));
			out(fmt::format("\tcrop left {} right {} top {} bottom {}\n", c->video->requested_left_crop(), c->video->requested_right_crop(), c->video->requested_top_crop(), c->video->requested_bottom_crop()));
			if (c->video->custom_ratio()) {
				out(fmt::format("\tscale to custom ratio {}:1\n", *c->video->custom_ratio()));
			}
			if (c->video->colour_conversion()) {
				if (c->video->colour_conversion().get().preset()) {
					out(fmt::format("\tcolour conversion {}\n", PresetColourConversion::all()[c->video->colour_conversion().get().preset().get()].name));
				} else {
					out("\tcustom colour conversion\n");
				}
			} else {
				out("\tno colour conversion\n");
			}

		}

		if (c->audio) {
			out(fmt::format("\t{} delay\n", c->audio->delay()));
			out(fmt::format("\t{} gain\n", c->audio->gain()));
		}
	}
}


static void
list_servers(function <void (string)> out)
{
	while (true) {
		int N = 0;
		auto servers = EncodeServerFinder::instance()->servers();

		/* This is a bit fiddly because we want to list configured servers that are down as well
		   as all those (configured and found by broadcast) that are up.
		*/

		if (servers.empty() && Config::instance()->servers().empty()) {
			out("No encoding servers found or configured.\n");
			++N;
		} else {
			out(fmt::format("{:24} Status Threads\n", "Host"));
			++N;

			/* Report the state of configured servers */
			for (auto i: Config::instance()->servers()) {
				/* See if this server is on the active list; if so, remove it and note
				   the number of threads it is offering.
				*/
				optional<int> threads;
				auto j = servers.begin();
				while (j != servers.end()) {
					if (i == j->host_name() && j->current_link_version()) {
						threads = j->threads();
						auto tmp = j;
						++tmp;
						servers.erase(j);
						j = tmp;
					} else {
						++j;
					}
				}
				if (static_cast<bool>(threads)) {
					out(fmt::format("{:24} UP     {}\n", i, threads.get()));
				} else {
					out(fmt::format("{:24} DOWN\n", i));
				}
				++N;
			}

			/* Now report any left that have been found by broadcast */
			for (auto const& i: servers) {
				if (i.current_link_version()) {
					out(fmt::format("{:24} UP     {}\n", i.host_name(), i.threads()));
				} else {
					out(fmt::format("{:24} bad version\n", i.host_name()));
				}
				++N;
			}
		}

		dcpomatic_sleep_seconds(1);

		for (int i = 0; i < N; ++i) {
			out("\033[1A\033[2K");
		}
	}
}


bool
show_jobs_on_console(function<void (string)> out, function<void ()> flush, bool progress)
{
	bool first = true;
	bool error = false;
	while (true) {

		dcpomatic_sleep_seconds(5);

		auto jobs = JobManager::instance()->get();

		if (!first && progress) {
			for (size_t i = 0; i < jobs.size(); ++i) {
				out(UP_ONE_LINE_AND_ERASE);
			}
			flush();
		}

		first = false;

		for (auto i: jobs) {
			if (progress) {
				out(i->name());
				if (!i->sub_name().empty()) {
					out(fmt::format("; {}", i->sub_name()));
				}
				out(": ");

				if (i->progress()) {
					out(fmt::format("{}			    \n", i->status()));
				} else {
					out(": Running	     \n");
				}
			}

			if (!progress && i->finished_in_error()) {
				/* We won't see this error if we haven't been showing progress,
				   so show it now.
				*/
				out(fmt::format("{}\n", i->status()));
			}

			if (i->finished_in_error()) {
				error = true;
			}
		}

		if (!JobManager::instance()->work_to_do()) {
			break;
		}
	}

	return error;
}


optional<string>
encode_cli(int argc, char* argv[], function<void (string)> out, function<void ()> flush)
{
	program_name = argv[0];

	boost::filesystem::path film_dir;
	bool progress = true;
	bool no_remote = false;
	optional<int> threads;
	optional<int> json_port;
	bool keep_going = false;
	bool dump = false;
	optional<boost::filesystem::path> servers;
	bool list_servers_ = false;
	bool dcp_path = false;
	optional<boost::filesystem::path> config;
	bool check = true;
	optional<string> export_format;
	optional<boost::filesystem::path> export_filename;
	bool hints = false;
	string command = "make-dcp";

	/* This makes it possible to call getopt several times in the same executable, for tests */
	optind = 0;

	int option_index = 0;
	while (true) {
		static struct option long_options[] = {
			{ "version", no_argument, 0, 'v'},
			{ "help", no_argument, 0, 'h'},
			{ "flags", no_argument, 0, 'f'},
			{ "no-progress", no_argument, 0, 'n'},
			{ "no-remote", no_argument, 0, 'r'},
			{ "threads", required_argument, 0, 't'},
			{ "json", required_argument, 0, 'j'},
			{ "keep-going", no_argument, 0, 'k' },
			{ "servers", required_argument, 0, 's' },
			{ "list-servers", no_argument, 0, 'l' },
			{ "dcp-path", no_argument, 0, 'd' },
			{ "config", required_argument, 0, 'c' },
			/* Just using A, B, C ... from here on */
			{ "dump", no_argument, 0, 'A' },
			{ "no-check", no_argument, 0, 'B' },
			{ "export-format", required_argument, 0, 'C' },
			{ "export-filename", required_argument, 0, 'D' },
			{ "hints", no_argument, 0, 'E' },
			{ 0, 0, 0, 0 }
		};

		int c = getopt_long(argc, argv, "vhfnrt:j:kAs:ldc:BC:D:E", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'v':
			out(fmt::format("dcpomatic version {} {}\n", dcpomatic_version, dcpomatic_git_commit));
			return {};
		case 'h':
			help(out);
			return {};
		case 'f':
			out(fmt::format("{}\n", dcpomatic_cxx_flags));
			return {};
		case 'n':
			progress = false;
			break;
		case 'r':
			no_remote = true;
			break;
		case 't':
			threads = atoi(optarg);
			break;
		case 'j':
			json_port = atoi(optarg);
			break;
		case 'k':
			keep_going = true;
			break;
		case 'A':
			dump = true;
			break;
		case 's':
			servers = optarg;
			break;
		case 'l':
			list_servers_ = true;
			break;
		case 'd':
			dcp_path = true;
			progress = false;
			break;
		case 'c':
			config = optarg;
			break;
		case 'B':
			check = false;
			break;
		case 'C':
			export_format = optarg;
			break;
		case 'D':
			export_filename = optarg;
			break;
		case 'E':
			hints = true;
			break;
		}
	}

	vector<string> commands = {
		"make-dcp",
		"list-servers",
#ifdef DCPOMATIC_GROK
		"dump",
		"config-params",
		"config",
		"list-gpus"
#else
		"dump"
#endif
	};

	if (optind < argc - 1) {
		/* Command with a film specified afterwards */
		command = argv[optind++];
	} else if (optind < argc) {
		/* Look for a valid command, hoping that it's not the name of a film */
		if (std::find(commands.begin(), commands.end(), argv[optind]) != commands.end()) {
			command = argv[optind++];
		}
	}


#ifdef DCPOMATIC_GROK
	if (command == "config-params") {
		out("Configurable parameters:\n\n");
		out("  grok-licence           licence string for using the Grok JPEG2000 encoder\n");
		out("  grok-enable            1 to enable the Grok encoder, 0 to disable it\n");
		out("  grok-binary-location   directory containing Grok binaries\n");
		return {};
	}

	if (command == "config") {
		if (optind < argc - 1) {
			string const parameter = argv[optind++];
			string const value = argv[optind++];
			auto grok = Config::instance()->grok();
			if (parameter == "grok-licence") {
				grok.licence = value;
			} else if (parameter == "grok-enable") {
				if (value == "1") {
					grok.enable = true;
				} else if (value == "0") {
					grok.enable = false;
				} else {
					return fmt::format("Invalid value {} for grok-enable (use 1 to enable, 0 to disable)", value);
				}
			} else if (parameter == "grok-binary-location") {
				grok.binary_location = value;
			} else {
				return fmt::format("Unrecognised configuration parameter `{}'", parameter);
			}
			Config::instance()->set_grok(grok);
			Config::instance()->write();
		} else {
			return fmt::format("Missing configuration parameter: use {} config <parameter> <value>", program_name);
		}
		return {};
	} else if (command == "list-gpus") {
		int N = 0;
		for (auto gpu: get_gpu_names()) {
			out(fmt::format("{}: {}\n", N++, gpu));
		}
		return {};
	}
#endif

	if (config) {
		State::override_path = *config;
	}

	if (servers) {
		dcp::File f(*servers, "r");
		if (!f) {
			return fmt::format("Could not open servers list file {}\n", servers->string());
		}
		vector<string> servers;
		while (!f.eof()) {
			char buffer[128];
			if (fscanf(f.get(), "%s.127", buffer) == 1) {
				servers.push_back(buffer);
			}
		}
		Config::instance()->set_servers(servers);
	}

	if (command == "list-servers" || list_servers_) {
		list_servers(out);
		return {};
	}

	if (optind >= argc) {
		help(out);
		return {};
	}

	if (export_format && !export_filename) {
		return string{"Argument --export-filename is required with --export-format\n"};
	}

	if (!export_format && export_filename) {
		return string{"Argument --export-format is required with --export-filename\n"};
	}

	if (export_format && *export_format != "mp4" && *export_format != "mov") {
		return string{"Unrecognised export format: must be mp4 or mov\n"};
	}

	film_dir = argv[optind];

	if (no_remote || export_format) {
		EncodeServerFinder::drop();
	}

	if (json_port) {
		new JSONServer(json_port.get());
	}

	if (threads) {
		Config::instance()->set_master_encoding_threads(threads.get());
	}

	shared_ptr<Film> film;
	try {
		film = make_shared<Film>(film_dir);
		film->read_metadata();
	} catch (std::exception& e) {
		return fmt::format("{}: error reading film `{}' ({})\n", program_name, film_dir.string(), e.what());
	}

	if (command == "dump" || dump) {
		print_dump(out, film);
		return {};
	}

	dcpomatic_log = film->log();

	for (auto i: film->content()) {
		auto paths = i->paths();
		for (auto j: paths) {
			if (!dcp::filesystem::exists(j)) {
				return fmt::format("{}: content file {} not found.\n", program_name, j.string());
			}
		}
	}

	if (!export_format && hints) {
		string const prefix = "Checking project for hints";
		bool pulse_phase = false;
		vector<string> hints;
		bool finished = false;

		Hints hint_finder(film);
		hint_finder.Progress.connect([prefix, &out, &flush](string progress) {
			out(fmt::format("{}{}: {}\n", UP_ONE_LINE_AND_ERASE, prefix, progress));
			flush();
		});
		hint_finder.Pulse.connect([prefix, &pulse_phase, &out, &flush]() {
			out(fmt::format("{}{}: {}\n", UP_ONE_LINE_AND_ERASE, prefix, pulse_phase ? "X" : "x"));
			flush();
			pulse_phase = !pulse_phase;
		});
		hint_finder.Hint.connect([&hints](string hint) {
			hints.push_back(hint);
		});
		hint_finder.Finished.connect([&finished]() {
			finished = true;
		});

		out(fmt::format("{}:\n", prefix));
		flush();

		hint_finder.start();
		while (!finished) {
			signal_manager->ui_idle();
			dcpomatic_sleep_milliseconds(200);
		}

		out(UP_ONE_LINE_AND_ERASE);

		if (!hints.empty()) {
			string error = "Hints:\n\n";
			for (auto hint: hints) {
				error += word_wrap("* " + hint, 70) + "\n";
			}
			error += "*** Encoding aborted because hints were found ***\n\n";
			error += "Modify your settings and run the command again, or run without\n";
			error += "the `--hints' option to ignore these hints and encode anyway.\n";
			return error;
		}
	}

#ifdef DCPOMATIC_GROK
	grk_plugin::setMessengerLogger(new grk_plugin::GrokLogger("[GROK] "));
	setup_grok_library_path();
#endif

	if (progress) {
		if (export_format) {
			out(fmt::format("Exporting {}\n", film->name()));
		} else {
			out(fmt::format("Making DCP for {}\n", film->name()));
		}
	}

	TranscodeJob::ChangedBehaviour const behaviour = check ? TranscodeJob::ChangedBehaviour::STOP : TranscodeJob::ChangedBehaviour::IGNORE;

	if (export_format) {
		auto job = std::make_shared<TranscodeJob>(film, behaviour);
		job->set_encoder(
			std::make_shared<FFmpegFilmEncoder>(
				film, job, *export_filename, *export_format == "mp4" ? ExportFormat::H264_AAC : ExportFormat::PRORES_HQ, false, false, false, 23
				)
			);
		JobManager::instance()->add(job);
	} else {
		try {
			make_dcp(film, behaviour);
		} catch (runtime_error& e) {
			return fmt::format("Could not make DCP: {}\n", e.what());
		}
	}

	bool const error = show_jobs_on_console(out, flush, progress);

	if (keep_going) {
		while (true) {
			dcpomatic_sleep_seconds(3600);
		}
	}

	/* This is just to stop valgrind reporting leaks due to JobManager
	   indirectly holding onto codecs.
	*/
	JobManager::drop();

	EncodeServerFinder::drop();

	if (dcp_path && !error) {
		out(fmt::format("{}\n", film->dir(film->dcp_name(false)).string()));
	}

	if (error) {
		return string{"Error during encoding"};
	}

	return {};
}
