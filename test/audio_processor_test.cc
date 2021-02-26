/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/audio_processor_test.cc
 *  @brief Test audio processors.
 *  @ingroup feature
 */


#include "lib/audio_processor.h"
#include "lib/analyse_audio_job.h"
#include "lib/dcp_content_type.h"
#include "lib/job_manager.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::shared_ptr;
using std::make_shared;


/** Test the mid-side decoder for analysis and DCP-making */
BOOST_AUTO_TEST_CASE (audio_processor_test)
{
	auto film = new_test_film ("audio_processor_test");
	film->set_name ("audio_processor_test");
	auto c = make_shared<FFmpegContent>("test/data/white.wav");
	film->examine_and_add_content (c);
	BOOST_REQUIRE (!wait_for_jobs());

	film->set_audio_channels (6);
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film->set_audio_processor (AudioProcessor::from_id ("mid-side-decoder"));

	/* Analyse the audio and check it doesn't crash */
	auto job = make_shared<AnalyseAudioJob> (film, film->playlist(), false);
	JobManager::instance()->add (job);
	BOOST_REQUIRE (!wait_for_jobs());

	/* Make a DCP and check it */
	make_and_verify_dcp (film, {dcp::VerificationNote::Code::MISSING_CPL_METADATA});
	check_dcp ("test/data/audio_processor_test", film->dir (film->dcp_name ()));
}
