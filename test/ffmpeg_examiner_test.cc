/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/ffmpeg_examiner_test.cc
 *  @brief FFmpegExaminer tests
 *  @ingroup selfcontained
 */


#include <boost/test/unit_test.hpp>
#include "lib/ffmpeg_examiner.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_audio_stream.h"
#include "test.h"


using std::make_shared;
using namespace dcpomatic;


/** Check that the FFmpegExaminer can extract the first video and audio time
 *  correctly from data/count300bd24.m2ts.
 */
BOOST_AUTO_TEST_CASE (ffmpeg_examiner_test)
{
	auto content = make_shared<FFmpegContent>("test/data/count300bd24.m2ts");
	auto film = new_test_film2("ffmpeg_examiner_test", { content });
	auto examiner = make_shared<FFmpegExaminer>(content);

	BOOST_CHECK_EQUAL (examiner->first_video().get().get(), ContentTime::from_seconds(600).get());
	BOOST_CHECK_EQUAL (examiner->audio_streams().size(), 1U);
	BOOST_CHECK_EQUAL (examiner->audio_streams()[0]->first_audio.get().get(), ContentTime::from_seconds(600).get());
}


/** Check that audio sampling rate and channel counts are correctly picked up from
 *  a problematic file.  When we used to specify analyzeduration and probesize
 *  this file's details were picked up incorrectly.
 */
BOOST_AUTO_TEST_CASE (ffmpeg_examiner_probesize_test)
{
	auto content = make_shared<FFmpegContent>(TestPaths::private_data() / "RockyTop10 Playlist Flat.m4v");
	auto examiner = make_shared<FFmpegExaminer>(content);

	BOOST_CHECK_EQUAL (examiner->audio_streams().size(), 2U);
	BOOST_CHECK_EQUAL (examiner->audio_streams()[0]->frame_rate(), 48000);
	BOOST_CHECK_EQUAL (examiner->audio_streams()[0]->channels(), 2);
	BOOST_CHECK_EQUAL (examiner->audio_streams()[1]->frame_rate(), 48000);
	BOOST_CHECK_EQUAL (examiner->audio_streams()[1]->channels(), 6);
}


/** Check that a file can be examined without error */
BOOST_AUTO_TEST_CASE (ffmpeg_examiner_vob_test)
{
	auto content = make_shared<FFmpegContent>(TestPaths::private_data() / "bad.vob");
	auto examiner = make_shared<FFmpegExaminer>(content);
}


/** Check that another file can be examined without error */
BOOST_AUTO_TEST_CASE (ffmpeg_examiner_mkv_test)
{
	auto content = make_shared<FFmpegContent>(TestPaths::private_data() / "sample.mkv");
	auto examiner = make_shared<FFmpegExaminer>(content);
}


/** Check that the video stream is correctly picked from a difficult file (#2238) */
BOOST_AUTO_TEST_CASE (ffmpeg_examiner_video_stream_selection_test)
{
	auto content = make_shared<FFmpegContent>(TestPaths::private_data() / "isy.mp4");
	auto examiner = make_shared<FFmpegExaminer>(content);

	BOOST_REQUIRE (examiner->video_frame_rate());
	BOOST_CHECK_EQUAL (examiner->video_frame_rate().get(), 25);
}
