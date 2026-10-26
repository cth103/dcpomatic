/*
    Copyright (C) 2026 Jaume Puig <james.puig@elcine.com>

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


/** @file test/atmos_reference_asset_test.cc
 *  @brief Tests of referencing the Atmos track of an existing DCP from a VF.
 *
 *  These use test/data/atmos_0.mxf and atmos_1.mxf, which are in the public test data,
 *  so unlike atmos_test.cc they do not need the private data.
 *  @ingroup feature
 */


#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/film_util.h"
#include "lib/player.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_atmos_asset.h>
#include <dcp/reel_sound_asset.h>
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;


/** Make and verify an OV containing the Atmos track test/data/atmos_0.mxf */
static
shared_ptr<Film>
make_atmos_ov(string name)
{
	auto ov = new_test_film(name, content_factory("test/data/atmos_0.mxf"));
	make_and_verify_dcp(ov, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });
	return ov;
}


static
shared_ptr<DCPContent>
dcp_content_of(shared_ptr<const Film> film)
{
	return make_shared<DCPContent>(film->dir(film->dcp_name()));
}


BOOST_AUTO_TEST_CASE(atmos_reference_test)
{
	auto ov = make_atmos_ov("atmos_reference_test_ov");

	/* @return the Atmos reel asset of the first reel of the film's DCP */
	auto atmos_asset = [](shared_ptr<const Film> film) {
		dcp::DCP dcp(film->dir(film->dcp_name()));
		dcp.read();
		BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
		BOOST_REQUIRE_EQUAL(dcp.cpls()[0]->reels().size(), 1U);
		BOOST_REQUIRE(dcp.cpls()[0]->reels()[0]->atmos());
		return dcp.cpls()[0]->reels()[0]->atmos();
	};

	auto const ov_asset = atmos_asset(ov);
	auto const ov_id = ov_asset->asset_ref().id();

	auto make_vf = [ov](string name, bool reference_atmos, dcpomatic::ContentTime trim_start, dcpomatic::ContentTime trim_end) {
		auto ov_content = dcp_content_of(ov);
		auto vf = new_test_film(name, { ov_content });
		ov_content->set_reference_video(true);
		string why_not;
		BOOST_CHECK(ov_content->can_reference_atmos(vf, why_not));
		ov_content->set_reference_atmos(reference_atmos);
		ov_content->set_trim_start(vf, trim_start);
		ov_content->set_trim_end(trim_end);
		make_and_verify_dcp(vf, { dcp::VerificationNote::Code::MISSING_CPL_METADATA, dcp::VerificationNote::Code::EXTERNAL_ASSET }, false);
		return vf;
	};

	/* Without referencing, the Atmos is written again with a new ID */
	BOOST_CHECK(atmos_asset(make_vf("atmos_reference_test_vf_rewrite", false, {}, {}))->asset_ref().id() != ov_id);

	/* With referencing, the VF's CPL points at the OV's Atmos asset */
	auto const whole = atmos_asset(make_vf("atmos_reference_test_vf_reference", true, {}, {}));
	BOOST_CHECK_EQUAL(whole->asset_ref().id(), ov_id);
	BOOST_CHECK_EQUAL(whole->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(whole->actual_duration(), ov_asset->actual_duration());

	/* With trims, the VF still points at the same asset (same ID), and only the entry point and duration in the CPL change */
	auto const vf = make_vf("atmos_reference_test_vf_trim", true, dcpomatic::ContentTime::from_seconds(1), dcpomatic::ContentTime::from_seconds(2));
	auto const trimmed = atmos_asset(vf);
	auto const rate = vf->video_frame_rate();
	BOOST_CHECK_EQUAL(trimmed->asset_ref().id(), ov_id);
	BOOST_CHECK_EQUAL(trimmed->entry_point().get_value_or(0), rate);
	BOOST_CHECK_EQUAL(trimmed->actual_duration(), ov_asset->actual_duration() - 3 * rate);
}


/** The flag is saved in the project's metadata and comes back when it is read, and a project saved
 *  without it (from before it existed) reads as not referencing the Atmos track.
 */
BOOST_AUTO_TEST_CASE(atmos_reference_metadata_test)
{
	auto ov = make_atmos_ov("atmos_reference_metadata_test_ov");

	auto content = dcp_content_of(ov);
	auto film = new_test_film("atmos_reference_metadata_test", { content });
	content->set_reference_video(true);
	content->set_reference_atmos(true);
	BOOST_REQUIRE(content->reference_atmos());
	film->write_metadata();

	auto reread = make_shared<Film>(film->directory().get());
	reread->read_metadata();
	BOOST_REQUIRE_EQUAL(reread->content().size(), 1U);
	auto reread_content = dynamic_pointer_cast<DCPContent>(reread->content().front());
	BOOST_REQUIRE(reread_content);
	BOOST_CHECK(reread_content->reference_atmos());

	content->set_reference_atmos(false);
	film->write_metadata();

	auto reread_again = make_shared<Film>(film->directory().get());
	reread_again->read_metadata();
	BOOST_REQUIRE_EQUAL(reread_again->content().size(), 1U);
	BOOST_CHECK(!dynamic_pointer_cast<DCPContent>(reread_again->content().front())->reference_atmos());
}


/** The identifier is part of the path of the encoded video, so referencing the Atmos track must not
 *  change it: that would make the video be encoded again for no reason.
 */
BOOST_AUTO_TEST_CASE(atmos_reference_identifier_test)
{
	auto ov = make_atmos_ov("atmos_reference_identifier_test_ov");

	auto content = dcp_content_of(ov);
	auto film = new_test_film("atmos_reference_identifier_test", { content });
	auto const not_referenced = content->identifier();

	content->set_reference_atmos(true);
	BOOST_REQUIRE(content->reference_atmos());
	BOOST_CHECK_EQUAL(content->identifier(), not_referenced);
}


/** A DCP without an Atmos track cannot have it referenced, and neither can one whose Atmos track
 *  overlaps other Atmos content in the film.
 */
BOOST_AUTO_TEST_CASE(atmos_reference_unavailable_test)
{
	/* No Atmos track */
	auto picture_only = new_test_film("atmos_reference_unavailable_test_picture", content_factory("test/data/flat_red.png"));
	make_and_verify_dcp(picture_only);

	auto picture_only_content = dcp_content_of(picture_only);
	auto vf_picture = new_test_film("atmos_reference_unavailable_test_vf_picture", { picture_only_content });
	string why_not;
	BOOST_CHECK(!picture_only_content->can_reference_atmos(vf_picture, why_not));
	BOOST_CHECK_EQUAL(why_not, "it has no Atmos track.");

	/* Overlapping Atmos content */
	auto ov = make_atmos_ov("atmos_reference_unavailable_test_ov");
	auto ov_content = dcp_content_of(ov);
	auto atmos_1 = content_factory("test/data/atmos_1.mxf");
	auto vf = new_test_film("atmos_reference_unavailable_test_vf", { ov_content, atmos_1.front() });
	ov_content->set_reference_video(true);
	atmos_1.front()->set_position(vf, dcpomatic::DCPTime());
	why_not.clear();
	BOOST_CHECK(!ov_content->can_reference_atmos(vf, why_not));
	BOOST_CHECK_EQUAL(why_not, "it overlaps other Atmos content.");
}


/** If the Atmos track can no longer be referenced, the film switches the flag off. */
BOOST_AUTO_TEST_CASE(atmos_reference_reset_test)
{
	auto ov = make_atmos_ov("atmos_reference_reset_test_ov");

	auto ov_content = dcp_content_of(ov);
	auto atmos_1 = content_factory("test/data/atmos_1.mxf");
	auto vf = new_test_film("atmos_reference_reset_test_vf", { ov_content, atmos_1.front() });
	atmos_1.front()->set_position(vf, dcpomatic::DCPTime());

	/* The other Atmos content overlaps, so this must not stay on */
	ov_content->set_reference_video(true);
	ov_content->set_reference_atmos(true);
	BOOST_CHECK(!ov_content->reference_atmos());
}


/** Referencing the Atmos track makes a film a VF, even if nothing else is referenced. */
BOOST_AUTO_TEST_CASE(atmos_reference_is_vf_test)
{
	auto ov = make_atmos_ov("atmos_reference_is_vf_test_ov");

	auto content = dcp_content_of(ov);
	auto film = new_test_film("atmos_reference_is_vf_test", { content });
	BOOST_CHECK(!dcpomatic::film::is_vf(film));

	content->set_reference_atmos(true);
	BOOST_REQUIRE(content->reference_atmos());
	BOOST_CHECK(!content->reference_video());
	BOOST_CHECK(!content->reference_audio());
	BOOST_CHECK(dcpomatic::film::is_vf(film));
}


/** A referenced Atmos track is not decoded, because the output points at the original asset. */
BOOST_AUTO_TEST_CASE(atmos_reference_not_decoded_test)
{
	auto ov = make_atmos_ov("atmos_reference_not_decoded_test_ov");

	auto atmos_frames = [ov](bool reference_atmos) {
		auto content = dcp_content_of(ov);
		auto film = new_test_film(reference_atmos ? "atmos_reference_not_decoded_test_referenced" : "atmos_reference_not_decoded_test_rewritten", { content });
		content->set_reference_video(true);
		content->set_reference_atmos(reference_atmos);

		auto player = make_shared<Player>(film, Image::Alignment::COMPACT, false);
		int frames = 0;
		player->Atmos.connect([&frames](shared_ptr<const dcp::AtmosFrame>, dcpomatic::DCPTime, AtmosMetadata) {
			++frames;
		});
		while (!player->pass()) {}
		return frames;
	};

	BOOST_CHECK(atmos_frames(false) > 0);
	BOOST_CHECK_EQUAL(atmos_frames(true), 0);
}


/** Which of the OV's sound and Atmos assets does the VF point at, for each combination of referencing them?
 *  The two are independent: each is the OV's if and only if it is referenced.
 */
BOOST_AUTO_TEST_CASE(atmos_reference_audio_test)
{
	auto ov = make_atmos_ov("atmos_reference_audio_test_ov");

	auto first_reel = [](shared_ptr<const Film> film) {
		dcp::DCP dcp(film->dir(film->dcp_name()));
		dcp.read();
		BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
		BOOST_REQUIRE_EQUAL(dcp.cpls()[0]->reels().size(), 1U);
		return dcp.cpls()[0]->reels()[0];
	};

	auto const ov_reel = first_reel(ov);
	BOOST_REQUIRE(ov_reel->main_sound());
	BOOST_REQUIRE(ov_reel->atmos());

	/* Setting the two in either order, so that neither can undo the other */
	for (auto atmos_first: { false, true }) {
		for (auto reference_audio: { false, true }) {
			for (auto reference_atmos: { false, true }) {
				auto content = dcp_content_of(ov);
				auto vf = new_test_film(
					string("atmos_reference_audio_test_vf_") + (atmos_first ? "atmos_first" : "audio_first") + (reference_audio ? "_audio" : "_noaudio") + (reference_atmos ? "_atmos" : "_noatmos"),
					{ content }
					);
				content->set_reference_video(true);
				if (atmos_first) {
					content->set_reference_atmos(reference_atmos);
					content->set_reference_audio(reference_audio);
				} else {
					content->set_reference_audio(reference_audio);
					content->set_reference_atmos(reference_atmos);
				}
				make_and_verify_dcp(vf, { dcp::VerificationNote::Code::MISSING_CPL_METADATA, dcp::VerificationNote::Code::EXTERNAL_ASSET }, false);

				auto const reel = first_reel(vf);
				BOOST_REQUIRE(reel->main_sound());
				BOOST_REQUIRE(reel->atmos());
				BOOST_CHECK_EQUAL(content->reference_audio(), reference_audio);
				BOOST_CHECK_EQUAL(content->reference_atmos(), reference_atmos);
				BOOST_CHECK_EQUAL(reel->main_sound()->asset_ref().id() == ov_reel->main_sound()->asset_ref().id(), reference_audio);
				BOOST_CHECK_EQUAL(reel->atmos()->asset_ref().id() == ov_reel->atmos()->asset_ref().id(), reference_atmos);
			}
		}
	}
}


/** Make and verify an OV of test/data/atmos_0.mxf with two reels, split at 2s */
static
shared_ptr<Film>
make_two_reel_atmos_ov(string name)
{
	auto ov = new_test_film(name, content_factory("test/data/atmos_0.mxf"));
	ov->set_reel_type(ReelType::CUSTOM);
	ov->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_seconds(2) });
	make_and_verify_dcp(ov, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });
	return ov;
}


/** A VF whose reels do not start where the referenced Atmos reels start: each Atmos reel of the VF
 *  starts at the right point inside the referenced Atmos asset.
 */
BOOST_AUTO_TEST_CASE(atmos_reference_reel_offset_test)
{
	auto ov = make_two_reel_atmos_ov("atmos_reference_reel_offset_test_ov");

	auto content = dcp_content_of(ov);
	auto vf = new_test_film("atmos_reference_reel_offset_test_vf", { content });
	/* The VF's reels do not match the OV's, so its picture and sound are written again; only the Atmos is referenced */
	content->set_reference_video(false);
	content->set_reference_audio(false);
	/* The VF's third reel starts at 3s, which is 1s into the OV's second reel */
	vf->set_reel_type(ReelType::CUSTOM);
	vf->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_seconds(2), dcpomatic::DCPTime::from_seconds(3) });
	string why_not;
	BOOST_REQUIRE_MESSAGE(content->can_reference_atmos(vf, why_not), why_not);
	content->set_reference_atmos(true);
	make_and_verify_dcp(vf, { dcp::VerificationNote::Code::MISSING_CPL_METADATA, dcp::VerificationNote::Code::EXTERNAL_ASSET }, false);

	dcp::DCP dcp(vf->dir(vf->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto const reels = dcp.cpls()[0]->reels();
	BOOST_REQUIRE_EQUAL(reels.size(), 3U);
	BOOST_REQUIRE(reels[2]->atmos());
	BOOST_CHECK_EQUAL(reels[2]->atmos()->entry_point().get_value_or(0), 24);
}


/** A custom reel boundary at the same point as a reel start of the referenced Atmos must not make an empty reel */
BOOST_AUTO_TEST_CASE(atmos_reference_reel_boundaries_test)
{
	auto ov = make_two_reel_atmos_ov("atmos_reference_reel_boundaries_test_ov");

	auto content = dcp_content_of(ov);
	auto vf = new_test_film("atmos_reference_reel_boundaries_test_vf", { content });
	content->set_reference_video(false);
	content->set_reference_audio(false);
	vf->set_reel_type(ReelType::CUSTOM);
	vf->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_seconds(2), dcpomatic::DCPTime::from_seconds(3) });
	content->set_reference_atmos(true);
	BOOST_REQUIRE(content->reference_atmos());

	auto const reels = vf->reels();
	BOOST_REQUIRE_EQUAL(reels.size(), 3U);
	for (auto const& reel: reels) {
		BOOST_CHECK(reel.to > reel.from);
	}
}
