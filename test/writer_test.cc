/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/audio_buffers.h"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/cross.h"
#include "lib/dcp_encoder.h"
#include "lib/film.h"
#include "lib/job.h"
#include "lib/video_content.h"
#include "lib/writer.h"
#include "test.h"
#include <dcp/openjpeg_image.h>
#include <dcp/j2k_transcode.h>
#include <boost/test/unit_test.hpp>
#include <memory>


using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;


BOOST_AUTO_TEST_CASE (test_write_odd_amount_of_silence)
{
	auto content = content_factory("test/data/flat_red.png");
	auto film = new_test_film2 ("test_write_odd_amount_of_silence", content);
	content[0]->video->set_length(24);
	auto writer = make_shared<Writer>(film, shared_ptr<Job>());

	auto audio = make_shared<AudioBuffers>(6, 48000);
	audio->make_silent ();
	writer->write (audio, dcpomatic::DCPTime(1));
}


BOOST_AUTO_TEST_CASE (interrupt_writer)
{
	Cleanup cl;

	auto film = new_test_film2 ("test_interrupt_writer", {}, &cl);

	auto content = content_factory("test/data/check_image0.png")[0];
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());

	/* Add some dummy content to the film so that it has a reel of the right length */
	auto constexpr frames = 24 * 60;
	content->video->set_length (frames);

	/* Make a random J2K image */
	auto size = dcp::Size(1998, 1080);
	auto image = make_shared<dcp::OpenJPEGImage>(size);
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < (size.width * size.height); ++j) {
			image->data(i)[j] = rand() % 4095;
		}
	}

	/* Write some data */
	auto video = dcp::compress_j2k(image, 100000000, 24, false, false);
	auto video_ptr = make_shared<dcp::ArrayData>(video.data(), video.size());
	auto audio = make_shared<AudioBuffers>(6, 48000 / 24);

	auto writer = make_shared<Writer>(film, shared_ptr<Job>());
	writer->start ();

	for (int i = 0; i < frames; ++i) {
		writer->write (video_ptr, i, Eyes::BOTH);
		writer->write (audio, dcpomatic::DCPTime::from_frames(i, 24));
	}

	/* Start digest calculations then abort them; there should be no crash or error */
	boost::thread thread([film, writer]() {
		writer->finish(film->dir(film->dcp_name()));
	});

	dcpomatic_sleep_seconds	(1);

	thread.interrupt ();
	thread.join ();

	dcpomatic_sleep_seconds (1);
	cl.run ();
}


BOOST_AUTO_TEST_CASE(writer_progress_test)
{
	class TestJob : public Job
	{
	public:
		explicit TestJob(shared_ptr<const Film> film)
			: Job(film)
		{}

		~TestJob()
		{
			stop_thread();
		}

		std::string name() const override {
			return "test";
		}
		std::string json_name() const override {
			return "test";
		}
		void run() override {};
	};

	auto picture1 = content_factory("test/data/flat_red.png")[0];
	auto picture2 = content_factory("test/data/flat_red.png")[0];

	auto film = new_test_film2("writer_progress_test", { picture1, picture2 });
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	picture1->video->set_length(240);
	picture2->video->set_length(240);
	picture2->set_position(film, dcpomatic::DCPTime::from_seconds(10));

	auto job = std::make_shared<TestJob>(film);
	job->set_rate_limit_progress(false);

	float last_progress = 0;
	string last_sub_name;
	boost::signals2::scoped_connection connection = job->Progress.connect([job, &last_progress, &last_sub_name]() {
		auto const progress = job->progress().get_value_or(0);
		BOOST_REQUIRE(job->sub_name() != last_sub_name || progress >= last_progress);
		last_progress = progress;
		last_sub_name = job->sub_name();
	});

	DCPEncoder encoder(film, job);
	encoder.go();
}

