/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <boost/test/unit_test.hpp>
#include "lib/film.h"
#include "lib/dcp_content_type.h"
#include "lib/ratio.h"
#include "lib/sndfile_content.h"
#include "lib/audio_buffers.h"
#include "lib/player.h"
#include "test.h"

using std::cout;
using boost::shared_ptr;

static
void
process_audio (shared_ptr<const AudioBuffers> buffers, int* samples)
{
	cout << "weeeeeeeeee " << buffers->frames() << "\n";
	*samples += buffers->frames ();
}

/** Test the situation where a piece of SndfileContent has its video
 *  frame rate specified (i.e. the rate that it was prepared for),
 *  and hence might need resampling.
 */
BOOST_AUTO_TEST_CASE (audio_with_specified_video_frame_rate_test)
{
	/* Make a film using staircase.wav with the DCP at 30fps and the audio specified
	   as being prepared for 29.97.
	*/
	
	shared_ptr<Film> film = new_test_film ("audio_with_specified_video_frame_rate_test");
	film->set_dcp_content_type (DCPContentType::from_dci_name ("FTR"));
	film->set_container (Ratio::from_id ("185"));
	film->set_name ("audio_with_specified_video_frame_rate_test");

	shared_ptr<SndfileContent> content (new SndfileContent (film, "test/data/sine_440.wav"));
	content->set_video_frame_rate (29.97);
	film->examine_and_add_content (content);
	
	wait_for_jobs ();

	film->set_video_frame_rate (30);

	BOOST_CHECK_EQUAL (content->content_audio_frame_rate(), 48000);
	BOOST_CHECK_EQUAL (content->output_audio_frame_rate(), 47952);
}
