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


/** @file  test/immersive_audio_origin_test.cc
 *  @brief Test immersive_audio_origin(), which tells a Dolby Atmos track file from an IAB one.
 *  @ingroup selfcontained
 */


#include "lib/atmos_content.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/exceptions.h"
#include "lib/film.h"
#include "lib/video_content.h"
#include "lib/immersive_audio_origin.h"
#include <dcp/atmos_asset.h>
#include <dcp/atmos_asset_writer.h>
#include <dcp/exceptions.h>
#include <dcp/key.h>
#include <dcp/metadata.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <asdcp/AS_DCP.h>
#include <asdcp/KM_fileio.h>
#include <asdcp/KM_prng.h>
LIBDCP_ENABLE_WARNINGS
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <cstring>
#include <vector>


using std::string;
using std::vector;


/** @return a frame that starts with an IA preamble: PreambleTag 0x01, the 32-bit length and then @param payload */
static vector<uint8_t>
frame_with_preamble(vector<uint8_t> payload)
{
	vector<uint8_t> frame = { 0x01, 0x00, 0x00, 0x00, static_cast<uint8_t>(payload.size()) };
	frame.insert(frame.end(), payload.begin(), payload.end());
	frame.resize(1024, 0);
	return frame;
}


static vector<uint8_t> const dolby_frame = frame_with_preamble({ 0x11, 0x01, 0x0e, 0x00, 0x00, 0x00 });
static vector<uint8_t> const other_frame = frame_with_preamble({ 0x22, 0x02, 0x0f, 0x00, 0x00, 0x00 });


/** Write a track file as libdcp does, with the Dolby Atmos sub-descriptor */
static boost::filesystem::path
write_atmos(string name, string company, vector<uint8_t> frame, bool encrypted = false)
{
	auto const path = boost::filesystem::path("build/test") / (name + ".mxf");
	boost::filesystem::remove(path);
	dcp::AtmosAsset asset({24, 1}, 0, 16, 128, 0);
	asset.set_metadata(dcp::MXFMetadata(company, "test", "1"));
	if (encrypted) {
		asset.set_key(dcp::Key());
	}
	auto writer = asset.start_write(path);
	for (int i = 0; i < 24; ++i) {
		writer->write(frame.data(), frame.size());
	}
	writer->finalize();
	return path;
}


/** Write a track file with asdcplib's DCData writer, so without the Dolby Atmos sub-descriptor, as other IAB encoders do */
static boost::filesystem::path
write_dcdata(string name, string company, vector<uint8_t> frame, int frames = 24)
{
	auto const path = boost::filesystem::path("build/test") / (name + ".mxf");
	boost::filesystem::remove(path);

	ASDCP::WriterInfo info;
	info.LabelSetType = ASDCP::LS_MXF_SMPTE;
	info.CompanyName = company;
	info.ProductName = "test";
	info.ProductVersion = "1";
	Kumu::GenRandomUUID(info.AssetUUID);

	ASDCP::DCData::DCDataDescriptor descriptor;
	descriptor.EditRate = ASDCP::Rational(24, 1);
	descriptor.ContainerDuration = frames;
	/* The immersive audio Data Essence Coding of ST 429-18 */
	uint8_t const coding[16] = { 0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x05, 0x0e, 0x09, 0x06, 0x04, 0x00, 0x00, 0x00, 0x00 };
	memcpy(descriptor.DataEssenceCoding, coding, 16);

	ASDCP::DCData::MXFWriter writer;
	BOOST_REQUIRE(ASDCP_SUCCESS(writer.OpenWrite(path.string(), info, descriptor)));
	ASDCP::DCData::FrameBuffer buffer(frame.size());
	memcpy(buffer.Data(), frame.data(), frame.size());
	buffer.Size(frame.size());
	for (int i = 0; i < frames; ++i) {
		BOOST_REQUIRE(ASDCP_SUCCESS(writer.WriteFrame(buffer)));
	}
	BOOST_REQUIRE(ASDCP_SUCCESS(writer.Finalize()));
	return path;
}


/** Dolby's preamble and a Dolby CompanyName: Dolby */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_dolby_test)
{
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_dolby", "Dolby Laboratories", dolby_frame)) == ImmersiveAudioOrigin::DOLBY);
}


/** Dolby's preamble written by another program (frames wrapped again): IAB */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_rewrapped_test)
{
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_rewrapped", "DCP-o-matic", dolby_frame)) == ImmersiveAudioOrigin::IAB);
}


/** A Dolby CompanyName without Dolby's preamble: the name alone is not enough, IAB */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_dolby_name_only_test)
{
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_dolby_name_only", "Dolby Laboratories", other_frame)) == ImmersiveAudioOrigin::IAB);
}


/** A file with no preamble at all, as the test Atmos files in test/data: IAB */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_no_preamble_test)
{
	BOOST_CHECK(immersive_audio_origin("test/data/atmos_0.mxf") == ImmersiveAudioOrigin::IAB);
	BOOST_CHECK(immersive_audio_origin("test/data/atmos_1.mxf") == ImmersiveAudioOrigin::IAB);
}


/** An IAB file without the Dolby Atmos sub-descriptor, which dcp::AtmosAsset cannot open, is still read */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_without_sub_descriptor_test)
{
	auto const iab = write_dcdata("immersive_audio_origin_without_sub_descriptor", "Another encoder", other_frame);
	BOOST_CHECK_THROW(dcp::AtmosAsset{iab}, dcp::MXFFileError);
	BOOST_CHECK(immersive_audio_origin(iab) == ImmersiveAudioOrigin::IAB);

	/* And the rule is the same for such a file: Dolby's preamble and name make it Dolby */
	auto const dolby = write_dcdata("immersive_audio_origin_without_sub_descriptor_dolby", "Dolby Laboratories", dolby_frame);
	BOOST_CHECK(immersive_audio_origin(dolby) == ImmersiveAudioOrigin::DOLBY);
}


/** Files that cannot be judged are refused */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_refused_test)
{
	BOOST_CHECK_THROW(immersive_audio_origin("test/data/flat_red.png"), FileError);
	BOOST_CHECK_THROW(immersive_audio_origin("build/test/does_not_exist.mxf"), FileError);
	BOOST_CHECK_THROW(immersive_audio_origin(write_atmos("immersive_audio_origin_encrypted", "Dolby Laboratories", dolby_frame, true)), FileError);
}


/** Every byte of the preamble and the whole of "Dolby" count */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_exact_test)
{
	/* The name may be just "Dolby", or longer, as "Dolby Laboratories Inc." */
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_exact_dolby", "Dolby", dolby_frame)) == ImmersiveAudioOrigin::DOLBY);
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_exact_dolby_inc", "Dolby Laboratories Inc.", dolby_frame)) == ImmersiveAudioOrigin::DOLBY);
	/* A name that only starts like it */
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_exact_dolb", "Dolbo Systems", dolby_frame)) == ImmersiveAudioOrigin::IAB);

	/* Dolby's three bytes, but not after the PreambleTag 0x01 */
	auto tag = dolby_frame;
	tag[0] = 0x02;
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_exact_tag", "Dolby Laboratories", tag)) == ImmersiveAudioOrigin::IAB);

	/* Only the last of Dolby's three bytes differs */
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_exact_third_byte", "Dolby Laboratories", frame_with_preamble({ 0x11, 0x01, 0x0f, 0x00, 0x00, 0x00 }))) == ImmersiveAudioOrigin::IAB);
}


/** A first frame that cannot be read (here, larger than the 4MB that is read) is an error, not IAB */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_unreadable_frame_test)
{
	auto big = dolby_frame;
	big.resize(5 * 1024 * 1024, 0);
	BOOST_CHECK_THROW(immersive_audio_origin(write_dcdata("immersive_audio_origin_unreadable_frame", "Dolby Laboratories", big, 1)), FileError);
}


/** An Atmos MXF remembers who made it: its summary and the film's ISDCF name say Atmos or IAB, also after a reload */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_content_test)
{
	struct Case {
		string name;
		string company;
		ImmersiveAudioOrigin origin;
		string summary;
		string isdcf;
	};

	for (auto const& c: vector<Case>{
		{ "dolby", "Dolby Laboratories", ImmersiveAudioOrigin::DOLBY, "[Atmos]", "-ATMOS_" },
		{ "iab", "DCP-o-matic", ImmersiveAudioOrigin::IAB, "[IAB]", "-IAB_" },
	}) {
		auto const mxf = write_atmos("immersive_audio_origin_content_" + c.name + "_source", c.company, dolby_frame);
		auto content = content_factory(mxf)[0];
		auto film = new_test_film("immersive_audio_origin_content_" + c.name, { content });
		BOOST_REQUIRE(content->atmos);
		BOOST_CHECK(content->atmos->origin() == c.origin);
		BOOST_CHECK_MESSAGE(content->summary().find(c.summary) != string::npos, content->summary());
		BOOST_CHECK_MESSAGE(film->isdcf_name(false, false).find(c.isdcf) != string::npos, film->isdcf_name(false, false));

		film->write_metadata();
		auto reloaded = std::make_shared<Film>(film->directory().get());
		reloaded->read_metadata();
		BOOST_REQUIRE_EQUAL(reloaded->content().size(), 1U);
		BOOST_REQUIRE(reloaded->content()[0]->atmos);
		BOOST_CHECK(reloaded->content()[0]->atmos->origin() == c.origin);
		BOOST_CHECK_MESSAGE(reloaded->isdcf_name(false, false).find(c.isdcf) != string::npos, reloaded->isdcf_name(false, false));
	}
}


/** With Dolby and IAB content together the film is IAB; and a DCP's Atmos track is looked at when the DCP is examined */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_film_test)
{
	auto dolby = content_factory(write_atmos("immersive_audio_origin_film_dolby", "Dolby Laboratories", dolby_frame))[0];
	auto iab = content_factory(write_atmos("immersive_audio_origin_film_iab", "Another encoder", other_frame))[0];
	auto picture = content_factory("test/data/flat_red.png")[0];
	auto film = new_test_film("immersive_audio_origin_film", { picture, dolby, iab });
	picture->video->set_length(48);
	dolby->set_position(film, dcpomatic::DCPTime());
	iab->set_position(film, dcpomatic::DCPTime::from_seconds(1));
	BOOST_CHECK_MESSAGE(film->isdcf_name(false, false).find("-IAB_") != string::npos, film->isdcf_name(false, false));

	/* A DCP that DCP-o-matic makes from Dolby's frames is written by libdcp, so it is not Dolby's any more */
	auto only_dolby = new_test_film("immersive_audio_origin_film_dcp", { content_factory("test/data/flat_red.png")[0], content_factory(write_atmos("immersive_audio_origin_film_dcp_source", "Dolby Laboratories", dolby_frame))[0] });
	only_dolby->content()[0]->video->set_length(24);
	make_and_verify_dcp(only_dolby, {}, false, false);
	auto dcp = std::make_shared<DCPContent>(only_dolby->dir(only_dolby->dcp_name()));
	auto from_dcp = new_test_film("immersive_audio_origin_film_dcp_reread", { dcp });
	BOOST_REQUIRE(dcp->atmos);
	BOOST_CHECK(dcp->atmos->origin() == ImmersiveAudioOrigin::IAB);
}
