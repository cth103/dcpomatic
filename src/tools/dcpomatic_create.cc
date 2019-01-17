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

#include "lib/version.h"
#include "lib/film.h"
#include "lib/util.h"
#include "lib/content_factory.h"
#include "lib/job_manager.h"
#include "lib/signal_manager.h"
#include "lib/job.h"
#include "lib/dcp_content_type.h"
#include "lib/ratio.h"
#include "lib/image_content.h"
#include "lib/video_content.h"
#include "lib/cross.h"
#include "lib/config.h"
#include "lib/dcp_content.h"
#include "lib/create_cli.h"
#include "lib/version.h"
#include <dcp/exceptions.h>
#include <libxml++/libxml++.h>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <getopt.h>
#include <string>
#include <iostream>
#include <cstdlib>
#include <stdexcept>

using std::string;
using std::cout;
using std::cerr;
using std::list;
using std::exception;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
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
		cerr << "dcpomatic version " << dcpomatic_version << " " << dcpomatic_git_commit << "\n";
		exit (1);
	}

	if (cc.config_dir) {
		Config::override_path = *cc.config_dir;
	}

	signal_manager = new SimpleSignalManager ();
	JobManager* jm = JobManager::instance ();

	try {
		shared_ptr<Film> film (new Film(cc.output_dir));
		if (cc.template_name) {
			film->use_template (cc.template_name.get());
		}
		film->set_name (cc.name);

		film->set_container (cc.container_ratio);
		film->set_dcp_content_type (cc.dcp_content_type);
		film->set_interop (cc.standard == dcp::INTEROP);
		film->set_use_isdcf_name (!cc.no_use_isdcf_name);
		film->set_signed (!cc.no_sign);
		film->set_encrypted (cc.encrypt);
		film->set_three_d (cc.threed);

		BOOST_FOREACH (CreateCLI::Content i, cc.content) {
			boost::filesystem::path const can = boost::filesystem::canonical (i.path);
			list<shared_ptr<Content> > content;

			if (boost::filesystem::exists (can / "ASSETMAP") || (boost::filesystem::exists (can / "ASSETMAP.xml"))) {
				content.push_back (shared_ptr<DCPContent>(new DCPContent(can)));
			} else {
				/* I guess it's not a DCP */
				content = content_factory (can);
			}

			BOOST_FOREACH (shared_ptr<Content> j, content) {
				film->examine_and_add_content (j);
			}

			while (jm->work_to_do ()) {
				dcpomatic_sleep (1);
			}

			while (signal_manager->ui_idle() > 0) {}

			BOOST_FOREACH (shared_ptr<Content> j, content) {
				if (j->video) {
					j->video->set_scale (VideoContentScale(cc.content_ratio));
					j->video->set_frame_type (i.frame_type);
				}
			}
		}

		if (cc.dcp_frame_rate) {
			film->set_video_frame_rate (*cc.dcp_frame_rate);
		}

		BOOST_FOREACH (shared_ptr<Content> i, film->content()) {
			shared_ptr<ImageContent> ic = dynamic_pointer_cast<ImageContent> (i);
			if (ic && ic->still()) {
				ic->video->set_length (cc.still_length * 24);
			}
		}

		if (jm->errors ()) {
			BOOST_FOREACH (shared_ptr<Job> i, jm->get()) {
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
