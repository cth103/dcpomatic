/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/interrupt_encoder_test.cc
 *  @brief Test clean shutdown of threads if a DCP encode is interrupted.
 *  @ingroup feature
 */


#include "lib/audio_content.h"
#include "lib/cross.h"
#include "lib/dcp_content_type.h"
#include "lib/ffmpeg_content.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include "lib/ratio.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;


/** Interrupt a DCP encode when it is in progress, as this used to (still does?)
 *  sometimes give an error related to pthreads.
 */
BOOST_AUTO_TEST_CASE (interrupt_encoder_test)
{
	auto film = new_test_film ("interrupt_encoder_test");
	film->set_dcp_content_type (DCPContentType::from_isdcf_name("FTR"));
	film->set_container (Ratio::from_id("185"));
	film->set_name ("interrupt_encoder_test");

	auto content = make_shared<FFmpegContent>(TestPaths::private_data() / "prophet_long_clip.mkv");
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	film->make_dcp (TranscodeJob::ChangedBehaviour::IGNORE);

	dcpomatic_sleep_seconds (10);

	JobManager::drop ();
}
