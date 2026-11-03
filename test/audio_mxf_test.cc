/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


/** @file  test/audio_mxf_test.cc
 *  @brief Tests of AudioMXFContent, a PCM sound MXF added to a film on its own.
 *  @ingroup feature
 */


#include "lib/audio_analysis.h"
#include "lib/audio_content.h"
#include "lib/audio_mxf_content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/hints.h"
#include "lib/job_manager.h"
#include "lib/signal_manager.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/language_tag.h>
#include <dcp/reel.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_writer.h>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <cmath>


using std::dynamic_pointer_cast;
using std::string;


/** Make a DCP with a sound track and @return the path of its PCM sound MXF */
static boost::filesystem::path
make_audio_mxf(string name)
{
	auto film = new_test_film(name, { content_factory("test/data/sine_16_48_220_10.wav")[0] });
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });
	return find_file(film->dir(film->dcp_name()), "pcm_");
}


/** A PCM sound MXF is recognised as audio MXF content, not as something for FFmpeg */
BOOST_AUTO_TEST_CASE(audio_mxf_is_recognised_test)
{
	auto const mxf = make_audio_mxf("audio_mxf_is_recognised_test_source");
	BOOST_CHECK(AudioMXFContent::valid_mxf(mxf));

	auto content = content_factory(mxf);
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	BOOST_CHECK(dynamic_pointer_cast<AudioMXFContent>(content[0]));

	auto film = new_test_film("audio_mxf_is_recognised_test", content);
	BOOST_REQUIRE(content[0]->audio);
	BOOST_CHECK(!content[0]->video);

	dcp::SoundAsset const asset(mxf);
	BOOST_CHECK_EQUAL(content[0]->audio->stream()->channels(), asset.channels());
	BOOST_CHECK_EQUAL(content[0]->audio->stream()->frame_rate(), asset.sampling_rate());
}


/** The sound of a DCP made from a sound MXF has the length of the file */
BOOST_AUTO_TEST_CASE(audio_mxf_makes_a_dcp_test)
{
	auto const mxf = make_audio_mxf("audio_mxf_makes_a_dcp_test_source");
	dcp::SoundAsset const source(mxf);

	auto film = new_test_film("audio_mxf_makes_a_dcp_test", content_factory(mxf));
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto const reels = dcp.cpls()[0]->reels();
	BOOST_REQUIRE_EQUAL(reels.size(), 1U);
	BOOST_REQUIRE(reels[0]->main_sound());
	BOOST_CHECK_EQUAL(reels[0]->main_sound()->actual_duration(), source.intrinsic_duration());
}


/** Write a 2 second sound MXF at 24fps and 48kHz with a 440Hz tone in each channel that is not in silent
 *  @param labels true to write the MCA channel labels (libdcp labels the 5.1 channels and any extra active ones)
 *  @param silent the channels that have no sound
 */
static boost::filesystem::path
write_audio_mxf(
	string name, int channels, bool labels, std::vector<int> silent, int sampling_rate = 48000, int edit_rate = 24, boost::optional<dcp::Key> key = {}
	)
{
	auto const path = boost::filesystem::path("build") / "test" / (name + ".mxf");
	boost::filesystem::remove(path);

	dcp::SoundAsset asset(dcp::Fraction(edit_rate, 1), sampling_rate, channels, dcp::LanguageTag("en-US"), dcp::Standard::SMPTE);
	if (key) {
		asset.set_key(*key);
	}
	auto writer = asset.start_write(
		path, {}, dcp::SoundAsset::AtmosSync::DISABLED,
		labels ? dcp::SoundAsset::MCASubDescriptors::ENABLED : dcp::SoundAsset::MCASubDescriptors::DISABLED
		);

	int const block = sampling_rate / edit_rate;
	std::vector<float> tone(block);
	for (int i = 0; i < block; ++i) {
		tone[i] = 0.5 * sin(2 * M_PI * 440 * i / static_cast<double>(sampling_rate));
	}
	std::vector<float> quiet(block, 0.0F);

	std::vector<float const*> data(channels);
	for (int c = 0; c < channels; ++c) {
		data[c] = std::find(silent.begin(), silent.end(), c) == silent.end() ? tone.data() : quiet.data();
	}

	for (int i = 0; i < 2 * edit_rate; ++i) {
		writer->write(data.data(), channels, block);
	}
	writer->finalize();

	return path;
}


/** @return the sound MXF content of a new film with a file added to it */
static std::shared_ptr<AudioMXFContent>
add_audio_mxf(std::shared_ptr<Film> film, boost::filesystem::path path)
{
	auto content = content_factory(path);
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	film->examine_and_add_content(content);
	BOOST_REQUIRE(!wait_for_jobs());
	auto sound = dynamic_pointer_cast<AudioMXFContent>(content[0]);
	BOOST_REQUIRE(sound);
	return sound;
}


/** A sound MXF comes back when the film is saved and read again */
BOOST_AUTO_TEST_CASE(audio_mxf_xml_roundtrip_test)
{
	auto film = new_test_film("audio_mxf_xml_roundtrip_test");
	auto sound = add_audio_mxf(film, write_audio_mxf("audio_mxf_xml_roundtrip_test_source", 8, false, { 3 }));
	film->write_metadata();

	auto film2 = std::make_shared<Film>(film->directory().get());
	film2->read_metadata();
	BOOST_REQUIRE_EQUAL(film2->content().size(), 1U);
	auto sound2 = dynamic_pointer_cast<AudioMXFContent>(film2->content()[0]);
	BOOST_REQUIRE(sound2);
	BOOST_CHECK_EQUAL(sound2->audio->stream()->channels(), sound->audio->stream()->channels());
	BOOST_CHECK_EQUAL(sound2->audio->stream()->length(), sound->audio->stream()->length());
}


/** An encrypted source file is refused when it is examined, with a clear error */
BOOST_AUTO_TEST_CASE(audio_mxf_encrypted_source_is_refused_test)
{
	auto const mxf = write_audio_mxf("audio_mxf_encrypted_source_is_refused_test_source", 6, true, {}, 48000, 24, dcp::Key());

	auto film = new_test_film("audio_mxf_encrypted_source_is_refused_test");
	film->examine_and_add_content(content_factory(mxf));
	BOOST_CHECK(wait_for_jobs());
}


/** @return a new film set up so that the sound MXF at @param path can be used in it as it is */
static std::shared_ptr<Film>
film_for_public_mxf(string name, boost::filesystem::path path)
{
	dcp::SoundAsset const source(path);
	auto film = new_test_film(name);
	film->set_audio_channels(source.channels());
	film->set_video_frame_rate(source.edit_rate().numerator);
	film->set_interop(source.standard() == dcp::Standard::INTEROP);
	film->examine_and_add_content(content_factory(path));
	BOOST_REQUIRE(!wait_for_jobs());
	return film;
}


/** What the audio panel's level graph and peak read: an analysis of a sound MXF that goes into the DCP as it is */
BOOST_AUTO_TEST_CASE(audio_mxf_audio_analysis_test)
{
	auto film = film_for_public_mxf(
		"audio_mxf_audio_analysis_test", "test/data/audio_processor_test/pcm_908ca457-a5cd-472e-9d7d-1341917bd7d2.mxf"
		);
	boost::signals2::connection connection;
	JobManager::instance()->analyse_audio(film, film->playlist(), false, connection, [](Job::Result) {});
	BOOST_REQUIRE(!wait_for_jobs());

	AudioAnalysis analysis(film->audio_analysis_path(film->playlist()));
	BOOST_CHECK(analysis.overall_sample_peak().first.peak > 0);
}


/** The hints analyse the real sound of a sound MXF that goes into the DCP as it is, not silence */
BOOST_AUTO_TEST_CASE(audio_mxf_hints_see_the_sound_test)
{
	auto film = film_for_public_mxf(
		"audio_mxf_hints_see_the_sound_test", "test/data/audio_processor_test/pcm_908ca457-a5cd-472e-9d7d-1341917bd7d2.mxf"
		);

	Hints hints(film);
	hints.start();
	hints.join();
	while (signal_manager->ui_idle()) {}
	hints.rethrow();

	AudioAnalysis analysis(film->audio_analysis_path(film->playlist()));
	BOOST_CHECK(analysis.overall_sample_peak().first.peak > 0);
}


/** What the content menu's Properties shows, for a sound MXF */
BOOST_AUTO_TEST_CASE(audio_mxf_properties_test)
{
	auto film = film_for_public_mxf(
		"audio_mxf_properties_test", "test/data/audio_processor_test/pcm_908ca457-a5cd-472e-9d7d-1341917bd7d2.mxf"
		);
	BOOST_CHECK(!film->content()[0]->user_properties(film).empty());
	BOOST_CHECK(!film->content()[0]->summary().empty());
	BOOST_CHECK(!film->content()[0]->technical_summary().empty());
}


