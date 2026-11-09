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
#include <functional>
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


/** @return @param value as a Plex(8) field of ST 2098-2 (5.2) */
static vector<uint8_t>
plex8(uint32_t value)
{
	if (value < 0xff) {
		return { static_cast<uint8_t>(value) };
	}
	return { 0xff, static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xff) };
}


/** @return an IAElement: ElementID and ElementSize as Plex(8), then @param body */
static vector<uint8_t>
element(uint32_t id, vector<uint8_t> body)
{
	auto out = plex8(id);
	auto const size = plex8(body.size());
	out.insert(out.end(), size.begin(), size.end());
	out.insert(out.end(), body.begin(), body.end());
	return out;
}


/** @return an ObjectDefinition element whose first sub-block has the ObjectGainPrefix @param gain_prefix
 *  (0 unity, 1 silence, 2 gain in the stream)
 */
static vector<uint8_t>
object(uint8_t gain_prefix, bool conditional = false)
{
	/* MetaID 1, AudioDataID 0 */
	vector<uint8_t> body = { 0x01, 0x00 };
	if (conditional) {
		/* ConditionalObject 1, Reserved 1, ObjectUseCase 0xff (6 bits here and 2 in the next byte), Reserved 0,
		 * ObjectGainPrefix, then padding
		 */
		body.insert(body.end(), { 0xff, static_cast<uint8_t>(0xc0 | (gain_prefix << 3)) });
	} else {
		/* ConditionalObject 0, Reserved 0, ObjectGainPrefix, then padding */
		body.push_back(static_cast<uint8_t>(gain_prefix << 4));
	}
	body.resize(body.size() + 6, 0);
	return element(0x40, body);
}


/** @return a frame with a preamble of @param preamble then an IAFrame (ST 2098-2) with @param elements */
static vector<uint8_t>
frame_with_elements(vector<uint8_t> preamble, vector<vector<uint8_t>> elements)
{
	/* Version 1, SampleRate 48kHz, BitDepth 0, FrameRate 24, MaxRendered, SubElementCount */
	vector<uint8_t> body = { 0x01, 0x00, 0x76 };
	auto const count = plex8(elements.size());
	body.insert(body.end(), count.begin(), count.end());
	for (auto const& e: elements) {
		body.insert(body.end(), e.begin(), e.end());
	}
	auto const ia = element(0x08, body);

	auto big_endian = [](vector<uint8_t>& out, uint32_t value) {
		out.insert(out.end(), { static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value) });
	};

	vector<uint8_t> frame = { 0x01 };
	big_endian(frame, preamble.size());
	frame.insert(frame.end(), preamble.begin(), preamble.end());
	frame.push_back(0x02);
	big_endian(frame, ia.size());
	frame.insert(frame.end(), ia.begin(), ia.end());
	return frame;
}


static vector<uint8_t>
frame_with_object(vector<uint8_t> preamble, uint8_t gain_prefix)
{
	return frame_with_elements(preamble, { object(gain_prefix) });
}


static vector<uint8_t> const dolby_preamble_value = { 0x11, 0x01, 0x0e, 0x00, 0x00, 0x00 };


/** Writes bit fields most significant bit first */
class BitWriter
{
public:
	void put(uint32_t value, int bits)
	{
		for (int i = bits - 1; i >= 0; --i) {
			if (_bits % 8 == 0) {
				_data.push_back(0);
			}
			if ((value >> i) & 1) {
				_data.back() |= 0x80 >> (_bits % 8);
			}
			++_bits;
		}
	}

	vector<uint8_t> const& data() const
	{
		return _data;
	}

private:
	vector<uint8_t> _data;
	int _bits = 0;
};


/** How one AudioDataDLC element is coded */
struct DLC
{
	int sample_rate = 0;
	int shift_bits = 8;
	/** The order of each predictor region */
	vector<int> orders = { 16 };
	/** The BitDepth of the PCM sub-blocks, or -1 for Rice/Golomb sub-blocks of RiceRemBits 4 */
	int pcm_bit_depth = -1;
	/** Sub-blocks to write, fewer than the frame has to cut the element short */
	int sub_blocks = 10;
	/** Give the Rice/Golomb sub-blocks residuals other than zero: 16 (a quotient and no remainder), -5 and 0 in turn */
	bool residuals = false;
};


/** @return an AudioDataDLC element (ST 2098-2 9.6) of a 24fps 48kHz frame (10 sub-blocks of 200 samples) */
static vector<uint8_t>
dlc(DLC const& d)
{
	BitWriter bits;
	bits.put(d.sample_rate, 2);
	bits.put(d.shift_bits, 5);
	bits.put(d.orders.size(), 2);
	for (auto order: d.orders) {
		bits.put(10, 4);     // RegionLength: every sub-block
		bits.put(order, 5);
		for (int i = 0; i < order; ++i) {
			bits.put(512, 10);   // KCoeff of 0
		}
	}
	for (int block = 0; block < d.sub_blocks; ++block) {
		if (d.pcm_bit_depth >= 0) {
			bits.put(0, 1);
			bits.put(d.pcm_bit_depth, 5);
			for (int sample = 0; sample < 200; ++sample) {
				bits.put(0, d.pcm_bit_depth);
			}
		} else {
			bits.put(1, 1);
			bits.put(4, 5);
			for (int sample = 0; sample < 200; ++sample) {
				if (!d.residuals || sample % 3 == 2) {
					/* Quotient 0 (a single 0), then four bits of remainder 0 and no sign */
					bits.put(0, 5);
				} else if (sample % 3 == 0) {
					/* 16: quotient 1 (1 then 0), remainder 0, sign + */
					bits.put(0b10, 2);
					bits.put(0, 4);
					bits.put(0, 1);
				} else {
					/* -5: quotient 0, remainder 5, sign - */
					bits.put(0, 1);
					bits.put(5, 4);
					bits.put(1, 1);
				}
			}
		}
	}

	/* AudioDataID 1, DLCSize, then the coded audio */
	vector<uint8_t> body = { 0x01, static_cast<uint8_t>(bits.data().size() >> 8), static_cast<uint8_t>(bits.data().size() & 0xff) };
	body.insert(body.end(), bits.data().begin(), bits.data().end());
	return element(0x200, body);
}

static vector<uint8_t> const other_preamble_value = { 0x22, 0x02, 0x0f, 0x00, 0x00, 0x00 };
static vector<uint8_t> const other_frame = frame_with_preamble({ 0x22, 0x02, 0x0f, 0x00, 0x00, 0x00 });


static boost::filesystem::path
write_atmos_frames(string name, string company, vector<vector<uint8_t>> frames, bool encrypted = false);


static boost::filesystem::path
write_atmos(string name, string company, vector<uint8_t> frame, bool encrypted = false)
{
	return write_atmos_frames(name, company, vector<vector<uint8_t>>(24, frame), encrypted);
}


/** Write a track file as libdcp does, with the Dolby Atmos sub-descriptor, one frame of @param frames after another */
static boost::filesystem::path
write_atmos_frames(string name, string company, vector<vector<uint8_t>> frames, bool encrypted)
{
	auto const path = boost::filesystem::path("build/test") / (name + ".mxf");
	boost::filesystem::remove(path);
	dcp::AtmosAsset asset({24, 1}, 0, 16, 128, 0);
	asset.set_metadata(dcp::MXFMetadata(company, "test", "1"));
	if (encrypted) {
		asset.set_key(dcp::Key());
	}
	auto writer = asset.start_write(path);
	for (auto const& frame: frames) {
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
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_exact_third_byte_03", "Dolby Laboratories", frame_with_preamble({ 0x11, 0x01, 0x03, 0x00, 0x00, 0x00 }))) == ImmersiveAudioOrigin::IAB);
	/* The first or second byte differs from the older preamble */
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_exact_old_first_byte", "Dolby Laboratories", frame_with_preamble({ 0x12, 0x01, 0x02, 0x00, 0x00, 0x00 }))) == ImmersiveAudioOrigin::IAB);
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_exact_old_second_byte", "Dolby Laboratories", frame_with_preamble({ 0x11, 0x00, 0x02, 0x00, 0x00, 0x00 }))) == ImmersiveAudioOrigin::IAB);
}


/** The preamble of Dolby's older encoders, 11 01 02, is Dolby's too: with a Dolby name, and wrapped again by another
 *  program only if an object has the silence gain prefix
 */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_old_dolby_preamble_test)
{
	vector<uint8_t> const old_preamble = { 0x11, 0x01, 0x02, 0x00, 0x00, 0x00 };
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_old_dolby", "Dolby Laboratories", frame_with_preamble(old_preamble))) == ImmersiveAudioOrigin::DOLBY);
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_old_rewrapped", "DCP-o-matic", frame_with_preamble(old_preamble))) == ImmersiveAudioOrigin::IAB);
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_old_rewrapped_silent", "DCP-o-matic", frame_with_object(old_preamble, 0x1))) == ImmersiveAudioOrigin::DOLBY);
	BOOST_CHECK(immersive_audio_origin(write_dcdata("immersive_audio_origin_old_dolby_dcdata", "Dolby Laboratories", frame_with_preamble(old_preamble))) == ImmersiveAudioOrigin::DOLBY);
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


/** Dolby's preamble with another CompanyName (Dolby's frames wrapped again) is Dolby if an object has the silence
 *  gain prefix, which IAB Profile 1 does not allow, and IAB if not
 */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_silent_object_test)
{
	auto const silent = frame_with_object(dolby_preamble_value, 1);
	auto const unity = frame_with_object(dolby_preamble_value, 0);

	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_silent_object", "DCP-o-matic", silent)) == ImmersiveAudioOrigin::DOLBY);
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_unity_object", "DCP-o-matic", unity)) == ImmersiveAudioOrigin::IAB);
	/* Without Dolby's preamble the silence prefix is not enough */
	BOOST_CHECK(immersive_audio_origin(write_atmos("immersive_audio_origin_silent_object_other_preamble", "DCP-o-matic", frame_with_object(other_preamble_value, 1))) == ImmersiveAudioOrigin::IAB);
	/* Also in a file without the Dolby Atmos sub-descriptor */
	BOOST_CHECK(immersive_audio_origin(write_dcdata("immersive_audio_origin_silent_object_dcdata", "Another encoder", silent)) == ImmersiveAudioOrigin::DOLBY);
}


/** The silence prefix is looked for in the first 1000 frames */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_silent_object_late_test)
{
	auto const silent = frame_with_object(dolby_preamble_value, 1);
	auto const unity = frame_with_object(dolby_preamble_value, 0);

	vector<vector<uint8_t>> frames(1100, unity);
	frames[999] = silent;
	BOOST_CHECK(immersive_audio_origin(write_atmos_frames("immersive_audio_origin_silent_object_999", "DCP-o-matic", frames)) == ImmersiveAudioOrigin::DOLBY);

	frames[999] = unity;
	frames[1000] = silent;
	BOOST_CHECK(immersive_audio_origin(write_atmos_frames("immersive_audio_origin_silent_object_1000", "DCP-o-matic", frames)) == ImmersiveAudioOrigin::IAB);
}


/** A DCP that DCP-o-matic makes from Dolby Atmos with an inactive object stays Dolby Atmos, though libdcp writes its own CompanyName */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_silent_object_dcp_test)
{
	auto picture = content_factory("test/data/flat_red.png")[0];
	auto atmos = content_factory(write_atmos("immersive_audio_origin_silent_object_dcp_source", "Dolby Laboratories", frame_with_object(dolby_preamble_value, 1)))[0];
	auto film = new_test_film("immersive_audio_origin_silent_object_dcp", { picture, atmos });
	picture->video->set_length(24);
	make_and_verify_dcp(film, {}, false, false);

	auto dcp = std::make_shared<DCPContent>(film->dir(film->dcp_name()));
	auto from_dcp = new_test_film("immersive_audio_origin_silent_object_dcp_reread", { dcp });
	BOOST_REQUIRE(dcp->atmos);
	BOOST_CHECK(dcp->atmos->origin() == ImmersiveAudioOrigin::DOLBY);
	BOOST_CHECK_MESSAGE(from_dcp->isdcf_name(false, false).find("-ATMOS_") != string::npos, from_dcp->isdcf_name(false, false));
}


/** Only the silence prefix counts, wherever the object is in the frame and however the frame is laid out */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_silent_object_parse_test)
{
	auto check = [](string name, vector<uint8_t> frame, ImmersiveAudioOrigin expected) {
		BOOST_CHECK_MESSAGE(immersive_audio_origin(write_atmos("immersive_audio_origin_parse_" + name, "DCP-o-matic", frame)) == expected, name);
	};

	/* A gain in the stream (prefix 2) is not silence */
	check("gain_in_stream", frame_with_object(dolby_preamble_value, 2), ImmersiveAudioOrigin::IAB);
	/* A conditional object has two more fields before its gain prefix */
	check("conditional", frame_with_elements(dolby_preamble_value, { object(1, true) }), ImmersiveAudioOrigin::DOLBY);
	check("conditional_unity", frame_with_elements(dolby_preamble_value, { object(0, true) }), ImmersiveAudioOrigin::IAB);
	/* The silent object after other elements, one of them longer than 254 bytes (a Plex(8) size with its escape code) */
	check("after_elements", frame_with_elements(dolby_preamble_value, { element(0x10, vector<uint8_t>(300, 0x01)), object(0), object(1) }), ImmersiveAudioOrigin::DOLBY);
	check("after_elements_unity", frame_with_elements(dolby_preamble_value, { element(0x10, vector<uint8_t>(300, 0x01)), object(0), object(0) }), ImmersiveAudioOrigin::IAB);
	/* A preamble as long as Dolby's at 24fps: 11 01 0e then 1600 bytes of reference signal */
	auto preamble = dolby_preamble_value;
	preamble.resize(1603, 0x42);
	check("full_preamble", frame_with_object(preamble, 1), ImmersiveAudioOrigin::DOLBY);
}


/** Dolby's preamble with another CompanyName is Dolby if its audio is coded within what Dolby's encoders use, and IAB if
 *  any element is coded otherwise
 */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_dlc_test)
{
	auto check = [](string name, vector<uint8_t> preamble, vector<DLC> elements, ImmersiveAudioOrigin expected) {
		vector<vector<uint8_t>> coded;
		for (auto const& e: elements) {
			coded.push_back(dlc(e));
		}
		auto const frame = frame_with_elements(preamble, coded);
		BOOST_CHECK_MESSAGE(immersive_audio_origin(write_atmos("immersive_audio_origin_dlc_" + name, "DCP-o-matic", frame)) == expected, name);
	};

	DLC const dolby;
	check("dolby", dolby_preamble_value, { dolby }, ImmersiveAudioOrigin::DOLBY);
	check("dolby_old_preamble", { 0x11, 0x01, 0x02, 0x00, 0x00, 0x00 }, { dolby }, ImmersiveAudioOrigin::DOLBY);
	check("dolby_two_elements", dolby_preamble_value, { dolby, dolby }, ImmersiveAudioOrigin::DOLBY);
	/* Residuals other than zero are walked over, with their quotients and signs, to the next sub-block */
	DLC with_residuals;
	with_residuals.residuals = true;
	check("residuals", dolby_preamble_value, { with_residuals }, ImmersiveAudioOrigin::DOLBY);
	DLC with_residuals_17;
	with_residuals_17.residuals = true;
	with_residuals_17.orders = { 17 };
	check("residuals_then_other", dolby_preamble_value, { with_residuals, with_residuals_17 }, ImmersiveAudioOrigin::IAB);
	/* Without Dolby's preamble the coding is not enough */
	check("other_preamble", other_preamble_value, { dolby }, ImmersiveAudioOrigin::IAB);

	/* The edges of what Dolby's encoders use */
	auto with = [](std::function<void (DLC&)> change) {
		DLC d;
		change(d);
		return d;
	};
	check("shift_8", dolby_preamble_value, { with([](DLC& d) { d.shift_bits = 8; }) }, ImmersiveAudioOrigin::DOLBY);
	check("shift_12", dolby_preamble_value, { with([](DLC& d) { d.shift_bits = 12; }) }, ImmersiveAudioOrigin::DOLBY);
	check("shift_7", dolby_preamble_value, { with([](DLC& d) { d.shift_bits = 7; }) }, ImmersiveAudioOrigin::IAB);
	check("shift_13", dolby_preamble_value, { with([](DLC& d) { d.shift_bits = 13; }) }, ImmersiveAudioOrigin::IAB);
	check("no_region", dolby_preamble_value, { with([](DLC& d) { d.orders = {}; }) }, ImmersiveAudioOrigin::DOLBY);
	check("two_regions", dolby_preamble_value, { with([](DLC& d) { d.orders = { 4, 4 }; }) }, ImmersiveAudioOrigin::IAB);
	check("order_17", dolby_preamble_value, { with([](DLC& d) { d.orders = { 17 }; }) }, ImmersiveAudioOrigin::IAB);
	check("order_0", dolby_preamble_value, { with([](DLC& d) { d.orders = { 0 }; }) }, ImmersiveAudioOrigin::DOLBY);
	check("pcm_0", dolby_preamble_value, { with([](DLC& d) { d.pcm_bit_depth = 0; }) }, ImmersiveAudioOrigin::DOLBY);
	check("pcm_1", dolby_preamble_value, { with([](DLC& d) { d.pcm_bit_depth = 1; }) }, ImmersiveAudioOrigin::IAB);
	check("pcm_24", dolby_preamble_value, { with([](DLC& d) { d.pcm_bit_depth = 24; }) }, ImmersiveAudioOrigin::IAB);
	check("sample_rate_96k", dolby_preamble_value, { with([](DLC& d) { d.sample_rate = 1; }) }, ImmersiveAudioOrigin::IAB);
	/* One element coded otherwise is enough */
	check("second_element", dolby_preamble_value, { dolby, with([](DLC& d) { d.orders = { 17 }; }) }, ImmersiveAudioOrigin::IAB);
	/* An element cut short says nothing, so nothing is seen of the coding */
	check("cut_short", dolby_preamble_value, { with([](DLC& d) { d.sub_blocks = 3; }) }, ImmersiveAudioOrigin::IAB);
	/* Nor does a frame with no AudioDataDLC */
	check("none", dolby_preamble_value, {}, ImmersiveAudioOrigin::IAB);
}


/** The coding is looked at in the first 1000 frames, and one frame coded otherwise among them makes the file IAB */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_dlc_frames_test)
{
	auto const like = frame_with_elements(dolby_preamble_value, { dlc(DLC()) });
	DLC other_coding;
	other_coding.orders = { 17 };
	auto const other = frame_with_elements(dolby_preamble_value, { dlc(other_coding) });
	auto const none = frame_with_elements(dolby_preamble_value, {});

	vector<vector<uint8_t>> frames(1100, like);
	BOOST_CHECK(immersive_audio_origin(write_atmos_frames("immersive_audio_origin_dlc_frames_like", "DCP-o-matic", frames)) == ImmersiveAudioOrigin::DOLBY);

	frames[999] = other;
	BOOST_CHECK(immersive_audio_origin(write_atmos_frames("immersive_audio_origin_dlc_frames_999", "DCP-o-matic", frames)) == ImmersiveAudioOrigin::IAB);

	frames[999] = like;
	frames[1000] = other;
	BOOST_CHECK(immersive_audio_origin(write_atmos_frames("immersive_audio_origin_dlc_frames_1000", "DCP-o-matic", frames)) == ImmersiveAudioOrigin::DOLBY);

	/* Frames without DLC around one with it */
	vector<vector<uint8_t>> sparse(1100, none);
	sparse[999] = like;
	BOOST_CHECK(immersive_audio_origin(write_atmos_frames("immersive_audio_origin_dlc_frames_sparse", "DCP-o-matic", sparse)) == ImmersiveAudioOrigin::DOLBY);
	sparse[999] = none;
	sparse[1000] = like;
	BOOST_CHECK(immersive_audio_origin(write_atmos_frames("immersive_audio_origin_dlc_frames_sparse_1000", "DCP-o-matic", sparse)) == ImmersiveAudioOrigin::IAB);
}


/** A DCP that DCP-o-matic makes from Dolby Atmos frames coded as Dolby's encoders do stays Dolby Atmos */
BOOST_AUTO_TEST_CASE(immersive_audio_origin_dlc_dcp_test)
{
	auto picture = content_factory("test/data/flat_red.png")[0];
	auto atmos = content_factory(write_atmos("immersive_audio_origin_dlc_dcp_source", "Dolby Laboratories", frame_with_elements(dolby_preamble_value, { dlc(DLC()) })))[0];
	auto film = new_test_film("immersive_audio_origin_dlc_dcp", { picture, atmos });
	picture->video->set_length(24);
	make_and_verify_dcp(film, {}, false, false);

	auto dcp = std::make_shared<DCPContent>(film->dir(film->dcp_name()));
	auto from_dcp = new_test_film("immersive_audio_origin_dlc_dcp_reread", { dcp });
	BOOST_REQUIRE(dcp->atmos);
	BOOST_CHECK(dcp->atmos->origin() == ImmersiveAudioOrigin::DOLBY);
	BOOST_CHECK_MESSAGE(from_dcp->isdcf_name(false, false).find("-ATMOS_") != string::npos, from_dcp->isdcf_name(false, false));
}
