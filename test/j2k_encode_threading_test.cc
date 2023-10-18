#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/dcp_encoder.h"
#include "lib/dcp_transcode_job.h"
#include "lib/encode_server_description.h"
#include "lib/film.h"
#include "lib/j2k_encoder.h"
#include "lib/job_manager.h"
#include "lib/make_dcp.h"
#include "lib/transcode_job.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::list;


BOOST_AUTO_TEST_CASE(local_threads_created_and_destroyed)
{
	auto film = new_test_film2("local_threads_created_and_destroyed", {});
	Writer writer(film, {});
	J2KEncoder encoder(film, writer);

	encoder.remake_threads(32, 0, {});
	BOOST_CHECK_EQUAL(encoder._threads.size(), 32U);

	encoder.remake_threads(9, 0, {});
	BOOST_CHECK_EQUAL(encoder._threads.size(), 9U);

	encoder.end();
	BOOST_CHECK_EQUAL(encoder._threads.size(), 0U);
}


BOOST_AUTO_TEST_CASE(remote_threads_created_and_destroyed)
{
	auto film = new_test_film2("remote_threads_created_and_destroyed", {});
	Writer writer(film, {});
	J2KEncoder encoder(film, writer);

	list<EncodeServerDescription> servers = {
		{ "fred", 7, SERVER_LINK_VERSION },
		{ "jim", 2, SERVER_LINK_VERSION },
		{ "sheila", 14, SERVER_LINK_VERSION },
	};

	encoder.remake_threads(0, 0, servers);
	BOOST_CHECK_EQUAL(encoder._threads.size(), 7U + 2U + 14U);

	servers = {
		{ "fred", 7, SERVER_LINK_VERSION },
		{ "jim", 5, SERVER_LINK_VERSION },
		{ "sheila", 14, SERVER_LINK_VERSION },
	};

	encoder.remake_threads(0, 0, servers);
	BOOST_CHECK_EQUAL(encoder._threads.size(), 7U + 5U + 14U);

	servers = {
		{ "fred", 0, SERVER_LINK_VERSION },
		{ "jim", 0, SERVER_LINK_VERSION },
		{ "sheila", 11, SERVER_LINK_VERSION },
	};

	encoder.remake_threads(0, 0, servers);
	BOOST_CHECK_EQUAL(encoder._threads.size(), 11U);
}


BOOST_AUTO_TEST_CASE(frames_not_lost_when_threads_disappear)
{
	auto content = content_factory(TestPaths::private_data() / "clapperboard.mp4");
	auto film = new_test_film2("frames_not_lost", content);
	film->write_metadata();
	auto job = make_dcp(film, TranscodeJob::ChangedBehaviour::IGNORE);
	auto& encoder = dynamic_pointer_cast<DCPEncoder>(job->_encoder)->_j2k_encoder;

	while (JobManager::instance()->work_to_do()) {
		encoder.remake_threads(rand() % 8, 0, {});
		dcpomatic_sleep_seconds(1);
	}

	BOOST_CHECK(!JobManager::instance()->errors());

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL(dcp.cpls()[0]->reels().size(), 1U);
	BOOST_REQUIRE_EQUAL(dcp.cpls()[0]->reels()[0]->main_picture()->intrinsic_duration(), 423U);
}

