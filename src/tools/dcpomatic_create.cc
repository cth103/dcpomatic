/*
    Copyright (C) 2013-2019 Carl Hetherington <cth@carlh.net>

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

#include "lib/audio_content.h"
#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/create_cli.h"
#include "lib/cross.h"
#include "lib/dcp_content.h"
#include "lib/dcp_content_type.h"
#include "lib/dcpomatic_log.h"
#include "lib/film.h"
#include "lib/image_content.h"
#include "lib/job.h"
#include "lib/job_manager.h"
#include "lib/ratio.h"
#include "lib/signal_manager.h"
#include "lib/util.h"
#include "lib/version.h"
#include "lib/version.h"
#include "lib/video_content.h"
#include <dcp/exceptions.h>
#include <libxml++/libxml++.h>
#include <boost/filesystem.hpp>
#include <getopt.h>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

using std::cerr;
using std::cout;
using std::dynamic_pointer_cast;
using std::exception;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using boost::optional;

class SimpleSignalManager : public SignalManager
{
public:
	/* Do nothing in this method so that UI events happen in our thread
	   when we call SignalManager::ui_idle().
	*/
	void wake_ui () {}
};

int
main (int argc, char* argv[])
{
	dcpomatic_setup_path_encoding ();
	dcpomatic_setup ();

	CreateCLI cc (argc, argv);
	if (cc.error) {
		cerr << *cc.error << "\n";
		exit (1);
	}

	if (cc.version) {
		cout << "dcpomatic version " << dcpomatic_version << " " << dcpomatic_git_commit << "\n";
		exit (EXIT_SUCCESS);
	}

	if (cc.config_dir) {
		State::override_path = *cc.config_dir;
	}

	signal_manager = new SimpleSignalManager ();
	auto jm = JobManager::instance ();

	try {
		auto film = std::make_shared<Film>(cc.output_dir);
		dcpomatic_log = film->log ();
		dcpomatic_log->set_types (Config::instance()->log_types());
		if (cc.template_name) {
			film->use_template (cc.template_name.get());
		}
		film->set_name (cc.name);

		if (cc.container_ratio) {
			film->set_container (cc.container_ratio);
		}
		film->set_dcp_content_type (cc.dcp_content_type);
		film->set_interop (cc.standard == dcp::Standard::INTEROP);
		film->set_use_isdcf_name (!cc.no_use_isdcf_name);
		film->set_encrypted (cc.encrypt);
		film->set_three_d (cc.threed);
		if (cc.fourk) {
			film->set_resolution (Resolution::FOUR_K);
		}
		if (cc.j2k_bandwidth) {
			film->set_j2k_bandwidth (*cc.j2k_bandwidth);
		}

		for (auto i: cc.content) {
			auto const can = boost::filesystem::canonical (i.path);
			list<shared_ptr<Content>> content;

			if (boost::filesystem::exists (can / "ASSETMAP") || (boost::filesystem::exists (can / "ASSETMAP.xml"))) {
				content.push_back (make_shared<DCPContent>(can));
			} else {
				/* I guess it's not a DCP */
				content = content_factory (can);
			}

			for (auto j: content) {
				film->examine_and_add_content (j);
			}

			while (jm->work_to_do ()) {
				dcpomatic_sleep_seconds (1);
			}

			while (signal_manager->ui_idle() > 0) {}

			for (auto j: content) {
				if (j->video) {
					j->video->set_frame_type (i.frame_type);
				}
				if (j->audio && i.channel) {
					for (auto stream: j->audio->streams()) {
						AudioMapping mapping(stream->channels(), film->audio_channels());
						for (int channel = 0; channel < stream->channels(); ++channel) {
							mapping.set(channel, *i.channel, 1.0f);
						}
						stream->set_mapping (mapping);
					}
				}
				if (j->audio && i.gain) {
					j->audio->set_gain (*i.gain);
				}
			}
		}

		if (cc.dcp_frame_rate) {
			film->set_video_frame_rate (*cc.dcp_frame_rate);
		}

		for (auto i: film->content()) {
			auto ic = dynamic_pointer_cast<ImageContent> (i);
			if (ic && ic->still()) {
				ic->video->set_length (cc.still_length * 24);
			}
		}

		if (jm->errors ()) {
			for (auto i: jm->get()) {
				if (i->finished_in_error()) {
					cerr << i->error_summary() << "\n"
					     << i->error_details() << "\n";
				}
			}
			exit (EXIT_FAILURE);
		}

		if (cc.output_dir) {
			film->write_metadata ();
		} else {
			film->metadata()->write_to_stream_formatted(cout, "UTF-8");
		}
	} catch (exception& e) {
		cerr << argv[0] << ": " << e.what() << "\n";
		exit (EXIT_FAILURE);
	}

	return 0;
}
