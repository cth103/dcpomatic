/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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



/** @file  test/test.cc
 *  @brief Overall test stuff and useful methods for tests.
 */


#include "lib/compose.hpp"
#include "lib/config.h"
#include "lib/cross.h"
#include "lib/dcp_content_type.h"
#include "lib/dcpomatic_log.h"
#include "lib/encode_server_finder.h"
#include "lib/ffmpeg_image_proxy.h"
#include "lib/file_log.h"
#include "lib/film.h"
#include "lib/image.h"
#include "lib/job.h"
#include "lib/job_manager.h"
#include "lib/log_entry.h"
#include "lib/make_dcp.h"
#include "lib/ratio.h"
#include "lib/signal_manager.h"
#include "lib/util.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/equality_options.h>
#include <dcp/filesystem.h>
#include <dcp/mono_picture_asset.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/openjpeg_image.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/warnings.h>
#include <asdcp/AS_DCP.h>
#include <png.h>
#include <sndfile.h>
#include <libxml++/libxml++.h>
extern "C" {
#include <libavformat/avformat.h>
}
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE dcpomatic_test
#include <boost/algorithm/string.hpp>
LIBDCP_DISABLE_WARNINGS
#include <boost/random.hpp>
LIBDCP_ENABLE_WARNINGS
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <list>
#include <vector>


using std::abs;
using std::cerr;
using std::cout;
using std::list;
using std::make_shared;
using std::min;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::scoped_array;
using std::dynamic_pointer_cast;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


boost::filesystem::path
TestPaths::TestPaths::private_data ()
{
	char* env = getenv("DCPOMATIC_TEST_PRIVATE");
	if (env) {
		return boost::filesystem::path(env);
	}

	auto relative = boost::filesystem::path ("..") / boost::filesystem::path ("dcpomatic-test-private");
	if (!boost::filesystem::exists(relative)) {
		std::cerr << "No private test data found! Tests may fail.\n";
		return relative;
	}

	return boost::filesystem::canonical(relative);
}


boost::filesystem::path TestPaths::xsd ()
{
	return boost::filesystem::current_path().parent_path() / "libdcp" / "xsd";
}


static void
setup_test_config ()
{
	Config::instance()->set_master_encoding_threads (boost::thread::hardware_concurrency() / 2);
	Config::instance()->set_server_encoding_threads (1);
	Config::instance()->set_server_port_base (61921);
	Config::instance()->set_default_dcp_content_type (static_cast<DCPContentType*> (0));
	Config::instance()->set_default_audio_delay (0);
	Config::instance()->set_default_j2k_bandwidth (100000000);
	Config::instance()->set_default_interop (false);
	Config::instance()->set_default_still_length (10);
	Config::instance()->set_default_dcp_audio_channels(8);
	Config::instance()->set_log_types (
		LogEntry::TYPE_GENERAL | LogEntry::TYPE_WARNING |
		LogEntry::TYPE_ERROR | LogEntry::TYPE_DISK
		);
	Config::instance()->set_automatic_audio_analysis (false);
	auto signer = make_shared<dcp::CertificateChain>(dcp::file_to_string("test/data/signer_chain"));
	signer->set_key(dcp::file_to_string("test/data/signer_key"));
	Config::instance()->set_signer_chain (signer);
	auto decryption = make_shared<dcp::CertificateChain>(dcp::file_to_string("test/data/decryption_chain"));
	decryption->set_key(dcp::file_to_string("test/data/decryption_key"));
	Config::instance()->set_decryption_chain (decryption);
	Config::instance()->set_dcp_asset_filename_format(dcp::NameFormat("%t"));
	Config::instance()->set_cinemas_file("test/data/empty_cinemas.xml");
}


class TestSignalManager : public SignalManager
{
public:
	/* No wakes in tests: we call ui_idle ourselves */
	void wake_ui () override
	{

	}
};

struct TestConfig
{
	TestConfig ()
	{
		State::override_path = "build/test/state";
		boost::filesystem::remove_all (*State::override_path);

		dcpomatic_setup ();
		setup_test_config ();
		capture_ffmpeg_logs();

		EncodeServerFinder::drop();

		signal_manager = new TestSignalManager ();

		dcpomatic_log.reset (new FileLog("build/test/log"));

		auto const& suite = boost::unit_test::framework::master_test_suite();
		int types = LogEntry::TYPE_GENERAL | LogEntry::TYPE_WARNING | LogEntry::TYPE_ERROR;
		for (int i = 1; i < suite.argc; ++i) {
			if (string(suite.argv[i]) == "--log=debug-player") {
				types |= LogEntry::TYPE_DEBUG_PLAYER;
			}
		}

		dcpomatic_log->set_types(types);
	}

	~TestConfig ()
	{
		JobManager::drop ();
	}
};


BOOST_GLOBAL_FIXTURE (TestConfig);


boost::filesystem::path
test_film_dir (string name)
{
	boost::filesystem::path p;
	p /= "build";
	p /= "test";
	p /= name;
	return p;
}


shared_ptr<Film>
new_test_film (string name)
{
	auto p = test_film_dir (name);
	if (boost::filesystem::exists (p)) {
		boost::filesystem::remove_all (p);
	}

	auto film = make_shared<Film>(p);
	film->write_metadata ();
	return film;
}


shared_ptr<Film>
new_test_film2 (string name, vector<shared_ptr<Content>> content, Cleanup* cleanup)
{
	auto p = test_film_dir (name);
	if (boost::filesystem::exists (p)) {
		boost::filesystem::remove_all (p);
	}
	if (cleanup) {
		cleanup->add (p);
	}

	auto film = make_shared<Film>(p);
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TST"));
	film->set_container (Ratio::from_id ("185"));
	film->write_metadata ();

	for (auto i: content) {
		film->examine_and_add_content (i);
		BOOST_REQUIRE (!wait_for_jobs());
	}

	return film;
}


void
check_wav_file (boost::filesystem::path ref, boost::filesystem::path check)
{
	SF_INFO ref_info;
	ref_info.format = 0;
	auto ref_file = sf_open (ref.string().c_str(), SFM_READ, &ref_info);
	BOOST_CHECK (ref_file);

	SF_INFO check_info;
	check_info.format = 0;
	auto check_file = sf_open (check.string().c_str(), SFM_READ, &check_info);
	BOOST_CHECK (check_file);

	BOOST_CHECK_EQUAL (ref_info.frames, check_info.frames);
	BOOST_CHECK_EQUAL (ref_info.samplerate, check_info.samplerate);
	BOOST_CHECK_EQUAL (ref_info.channels, check_info.channels);
	BOOST_CHECK_EQUAL (ref_info.format, check_info.format);

	/* buffer_size is in frames */
	sf_count_t const buffer_size = 65536 * ref_info.channels;
	scoped_array<int32_t> ref_buffer (new int32_t[buffer_size]);
	scoped_array<int32_t> check_buffer (new int32_t[buffer_size]);

	sf_count_t N = ref_info.frames;
	while (N) {
		sf_count_t this_time = min (buffer_size, N);
		sf_count_t r = sf_readf_int (ref_file, ref_buffer.get(), this_time);
		BOOST_CHECK_EQUAL (r, this_time);
		r = sf_readf_int (check_file, check_buffer.get(), this_time);
		BOOST_CHECK_EQUAL (r, this_time);

		for (sf_count_t i = 0; i < this_time; ++i) {
			BOOST_REQUIRE_MESSAGE (
				abs (ref_buffer[i] - check_buffer[i]) <= 65536,
				ref << " differs from " << check << " at " << (ref_info.frames - N + i) << " of " << ref_info.frames
				<< "(" << ref_buffer[i] << " vs " << check_buffer[i] << ")"
				);
		}

		N -= this_time;
	}
}


void
check_mxf_audio_file (boost::filesystem::path ref, boost::filesystem::path check)
{
	ASDCP::PCM::MXFReader ref_reader;
	BOOST_REQUIRE (!ASDCP_FAILURE (ref_reader.OpenRead (ref.string().c_str())));

	ASDCP::PCM::AudioDescriptor ref_desc;
	BOOST_REQUIRE (!ASDCP_FAILURE (ref_reader.FillAudioDescriptor (ref_desc)));

	ASDCP::PCM::MXFReader check_reader;
	BOOST_REQUIRE (!ASDCP_FAILURE (check_reader.OpenRead (check.string().c_str())));

	ASDCP::PCM::AudioDescriptor check_desc;
	BOOST_REQUIRE (!ASDCP_FAILURE (check_reader.FillAudioDescriptor (check_desc)));

	BOOST_REQUIRE_EQUAL (ref_desc.ContainerDuration, check_desc.ContainerDuration);
	BOOST_REQUIRE_MESSAGE(ref_desc.ChannelCount, check_desc.ChannelCount);

	ASDCP::PCM::FrameBuffer ref_buffer (Kumu::Megabyte);
	ASDCP::PCM::FrameBuffer check_buffer (Kumu::Megabyte);
	for (size_t i = 0; i < ref_desc.ContainerDuration; ++i) {
		ref_reader.ReadFrame (i, ref_buffer, 0);
		check_reader.ReadFrame (i, check_buffer, 0);
		BOOST_REQUIRE_MESSAGE(memcmp(ref_buffer.RoData(), check_buffer.RoData(), ref_buffer.Size()) == 0, "Audio MXF differs in frame " << i);
	}
}


/** @return true if the files are the same, otherwise false */
bool
mxf_atmos_files_same (boost::filesystem::path ref, boost::filesystem::path check, bool verbose)
{
	ASDCP::ATMOS::MXFReader ref_reader;
	BOOST_REQUIRE (!ASDCP_FAILURE(ref_reader.OpenRead(ref.string().c_str())));

	ASDCP::ATMOS::AtmosDescriptor ref_desc;
	BOOST_REQUIRE (!ASDCP_FAILURE(ref_reader.FillAtmosDescriptor(ref_desc)));

	ASDCP::ATMOS::MXFReader check_reader;
	BOOST_REQUIRE (!ASDCP_FAILURE(check_reader.OpenRead(check.string().c_str())));

	ASDCP::ATMOS::AtmosDescriptor check_desc;
	BOOST_REQUIRE (!ASDCP_FAILURE(check_reader.FillAtmosDescriptor(check_desc)));

	if (ref_desc.EditRate.Numerator != check_desc.EditRate.Numerator) {
		if (verbose) {
			std::cout << "EditRate.Numerator differs.\n";
		}
		return false;
	}
	if (ref_desc.EditRate.Denominator != check_desc.EditRate.Denominator) {
		if (verbose) {
			std::cout << "EditRate.Denominator differs.\n";
		}
		return false;
	}
	if (ref_desc.ContainerDuration != check_desc.ContainerDuration) {
		if (verbose) {
			std::cout << "EditRate.ContainerDuration differs.\n";
		}
		return false;
	}
	if (ref_desc.FirstFrame != check_desc.FirstFrame) {
		if (verbose) {
			std::cout << "EditRate.FirstFrame differs.\n";
		}
		return false;
	}
	if (ref_desc.MaxChannelCount != check_desc.MaxChannelCount) {
		if (verbose) {
			std::cout << "EditRate.MaxChannelCount differs.\n";
		}
		return false;
	}
	if (ref_desc.MaxObjectCount != check_desc.MaxObjectCount) {
		if (verbose) {
			std::cout << "EditRate.MaxObjectCount differs.\n";
		}
		return false;
	}
	if (ref_desc.AtmosVersion != check_desc.AtmosVersion) {
		if (verbose) {
			std::cout << "EditRate.AtmosVersion differs.\n";
		}
		return false;
	}

	ASDCP::DCData::FrameBuffer ref_buffer (Kumu::Megabyte);
	ASDCP::DCData::FrameBuffer check_buffer (Kumu::Megabyte);
	for (size_t i = 0; i < ref_desc.ContainerDuration; ++i) {
		ref_reader.ReadFrame (i, ref_buffer, 0);
		check_reader.ReadFrame (i, check_buffer, 0);
		if (memcmp(ref_buffer.RoData(), check_buffer.RoData(), ref_buffer.Size())) {
			if (verbose) {
				std::cout << "data differs.\n";
			}
			return false;
		}
	}

	return true;
}


static
double
rms_error (boost::filesystem::path ref, boost::filesystem::path check)
{
	FFmpegImageProxy ref_proxy (ref);
	auto ref_image = ref_proxy.image(Image::Alignment::COMPACT).image;
	FFmpegImageProxy check_proxy (check);
	auto check_image = check_proxy.image(Image::Alignment::COMPACT).image;

	BOOST_REQUIRE_EQUAL (ref_image->planes(), check_image->planes());

	BOOST_REQUIRE_EQUAL (ref_image->pixel_format(), check_image->pixel_format());
	auto const format = ref_image->pixel_format();

	BOOST_REQUIRE (ref_image->size() == check_image->size());
	int const width = ref_image->size().width;
	int const height = ref_image->size().height;

	double sum_square = 0;
	switch (format) {
		case AV_PIX_FMT_RGBA:
		{
			for (int y = 0; y < height; ++y) {
				uint8_t* p = ref_image->data()[0] + y * ref_image->stride()[0];
				uint8_t* q = check_image->data()[0] + y * check_image->stride()[0];
				for (int x = 0; x < width; ++x) {
					for (int c = 0; c < 4; ++c) {
						sum_square += pow((*p++ - *q++), 2);
					}
				}
			}
			break;
		}
		case AV_PIX_FMT_RGB24:
		{
			for (int y = 0; y < height; ++y) {
				uint8_t* p = ref_image->data()[0] + y * ref_image->stride()[0];
				uint8_t* q = check_image->data()[0] + y * check_image->stride()[0];
				for (int x = 0; x < width; ++x) {
					for (int c = 0; c < 3; ++c) {
						sum_square += pow((*p++ - *q++), 2);
					}
				}
			}
			break;
		}
		case AV_PIX_FMT_RGB48BE:
		{
			for (int y = 0; y < height; ++y) {
				uint16_t* p = reinterpret_cast<uint16_t*>(ref_image->data()[0] + y * ref_image->stride()[0]);
				uint16_t* q = reinterpret_cast<uint16_t*>(check_image->data()[0] + y * check_image->stride()[0]);
				for (int x = 0; x < width; ++x) {
					for (int c = 0; c < 3; ++c) {
						sum_square += pow((*p++ - *q++), 2);
					}
				}
			}
			break;
		}
		case AV_PIX_FMT_YUVJ420P:
		{
			for (int c = 0; c < ref_image->planes(); ++c) {
				for (int y = 0; y < height / ref_image->vertical_factor(c); ++y) {
					auto p = ref_image->data()[c] + y * ref_image->stride()[c];
					auto q = check_image->data()[c] + y * check_image->stride()[c];
					for (int x = 0; x < ref_image->line_size()[c]; ++x) {
						sum_square += pow((*p++ - *q++), 2);
					}
				}
			}
			break;
		}
		default:
			BOOST_REQUIRE_MESSAGE (false, "unrecognised pixel format " << format);
	}

	return sqrt(sum_square / (height * width));
}


BOOST_AUTO_TEST_CASE (rms_error_test)
{
	BOOST_CHECK_CLOSE (rms_error("test/data/check_image0.png", "test/data/check_image0.png"), 0, 0.001);
	BOOST_CHECK_CLOSE (rms_error("test/data/check_image0.png", "test/data/check_image1.png"), 2.2778, 0.001);
	BOOST_CHECK_CLOSE (rms_error("test/data/check_image0.png", "test/data/check_image2.png"), 59.8896, 0.001);
	BOOST_CHECK_CLOSE (rms_error("test/data/check_image0.png", "test/data/check_image3.png"), 0.89164, 0.001);
}


void
check_image (boost::filesystem::path ref, boost::filesystem::path check, double threshold)
{
	double const e = rms_error (ref, check);
	BOOST_CHECK_MESSAGE (e < threshold, ref << " differs from " << check << " " << e);
}


void
check_file (boost::filesystem::path ref, boost::filesystem::path check)
{
	auto N = boost::filesystem::file_size (ref);
	BOOST_CHECK_EQUAL (N, boost::filesystem::file_size (check));
	dcp::File ref_file(ref, "rb");
	BOOST_CHECK (ref_file);
	dcp::File check_file(check, "rb");
	BOOST_CHECK (check_file);

	int const buffer_size = 65536;
	std::vector<uint8_t> ref_buffer(buffer_size);
	std::vector<uint8_t> check_buffer(buffer_size);

	string error = "File " + check.string() + " differs from reference " + ref.string();

	while (N) {
		uintmax_t this_time = min (uintmax_t (buffer_size), N);
		size_t r = ref_file.read (ref_buffer.data(), 1, this_time);
		BOOST_CHECK_EQUAL (r, this_time);
		r = check_file.read(check_buffer.data(), 1, this_time);
		BOOST_CHECK_EQUAL (r, this_time);

		BOOST_CHECK_MESSAGE (memcmp (ref_buffer.data(), check_buffer.data(), this_time) == 0, error);
		if (memcmp (ref_buffer.data(), check_buffer.data(), this_time)) {
			break;
		}

		N -= this_time;
	}
}


void
check_text_file (boost::filesystem::path ref, boost::filesystem::path check)
{
	dcp::File ref_file(ref, "r");
	BOOST_CHECK (ref_file);
	dcp::File check_file(check, "r");
	BOOST_CHECK (check_file);

	int const buffer_size = std::max(
		boost::filesystem::file_size(ref),
		boost::filesystem::file_size(check)
		);

	DCPOMATIC_ASSERT (buffer_size < 1024 * 1024);

	std::vector<uint8_t> ref_buffer(buffer_size);
	auto ref_read = ref_file.read(ref_buffer.data(), 1, buffer_size);
	std::vector<uint8_t> check_buffer(buffer_size);
	auto check_read = check_file.read(check_buffer.data(), 1, buffer_size);
	BOOST_CHECK_EQUAL (ref_read, check_read);

	string const error = "File " + check.string() + " differs from reference " + ref.string();
	BOOST_CHECK_MESSAGE(memcmp(ref_buffer.data(), check_buffer.data(), ref_read) == 0, error);
}


static void
note (dcp::NoteType t, string n)
{
	if (t == dcp::NoteType::ERROR) {
		cerr << n << "\n";
	}
}


void
check_dcp (boost::filesystem::path ref, shared_ptr<const Film> film)
{
	check_dcp (ref, film->dir(film->dcp_name()));
}


void
check_dcp(boost::filesystem::path ref, boost::filesystem::path check, bool sound_can_differ)
{
	dcp::DCP ref_dcp (ref);
	ref_dcp.read ();
	dcp::DCP check_dcp (check);
	check_dcp.read ();

	dcp::EqualityOptions options;
	options.max_mean_pixel_error = 5;
	options.max_std_dev_pixel_error = 5;
	options.max_audio_sample_error = 255;
	options.cpl_annotation_texts_can_differ = true;
	options.reel_annotation_texts_can_differ = true;
	options.reel_hashes_can_differ = true;
	options.asset_hashes_can_differ = true;
	options.issue_dates_can_differ = true;
	options.max_subtitle_vertical_position_error = 0.001;
	options.sound_assets_can_differ = sound_can_differ;

	BOOST_CHECK_MESSAGE(ref_dcp.equals(check_dcp, options, boost::bind (note, _1, _2)), check << " does not match " << ref);
}

void
check_xml (xmlpp::Element* ref, xmlpp::Element* test, list<string> ignore)
{
	BOOST_CHECK_EQUAL (ref->get_name (), test->get_name ());
	BOOST_CHECK_EQUAL (ref->get_namespace_prefix (), test->get_namespace_prefix ());

	if (find (ignore.begin(), ignore.end(), ref->get_name()) != ignore.end ()) {
		return;
	}

	auto ref_children = ref->get_children ();
	auto test_children = test->get_children ();
	BOOST_REQUIRE_MESSAGE (
		ref_children.size() == test_children.size(),
		ref->get_name() << " has " << ref_children.size() << " or " << test_children.size() << " children"
		);

	auto k = ref_children.begin ();
	auto l = test_children.begin ();
	while (k != ref_children.end ()) {

		/* XXX: should be doing xmlpp::EntityReference, xmlpp::XIncludeEnd, xmlpp::XIncludeStart */

		auto ref_el = dynamic_cast<xmlpp::Element*>(*k);
		auto test_el = dynamic_cast<xmlpp::Element*>(*l);
		BOOST_CHECK ((ref_el && test_el) || (!ref_el && !test_el));
		if (ref_el && test_el) {
			check_xml (ref_el, test_el, ignore);
		}

		auto ref_cn = dynamic_cast<xmlpp::ContentNode*>(*k);
		auto test_cn = dynamic_cast<xmlpp::ContentNode*>(*l);
		BOOST_CHECK ((ref_cn && test_cn) || (!ref_cn && !test_cn));
		if (ref_cn && test_cn) {
			BOOST_CHECK_EQUAL (ref_cn->get_content(), test_cn->get_content ());
		}

		++k;
		++l;
	}

	auto ref_attributes = ref->get_attributes ();
	auto test_attributes = test->get_attributes ();
	BOOST_CHECK_EQUAL (ref_attributes.size(), test_attributes.size ());

	auto m = ref_attributes.begin();
	auto n = test_attributes.begin();
	while (m != ref_attributes.end ()) {
		BOOST_CHECK_EQUAL ((*m)->get_name(), (*n)->get_name());
		BOOST_CHECK_EQUAL ((*m)->get_value(), (*n)->get_value());

		++m;
		++n;
	}
}

void
check_xml (boost::filesystem::path ref, boost::filesystem::path test, list<string> ignore)
{
	auto ref_parser = new xmlpp::DomParser(ref.string());
	auto ref_root = ref_parser->get_document()->get_root_node();
	auto test_parser = new xmlpp::DomParser(test.string());
	auto test_root = test_parser->get_document()->get_root_node();

	check_xml (ref_root, test_root, ignore);
}

bool
wait_for_jobs ()
{
	auto jm = JobManager::instance ();
	while (jm->work_to_do()) {
		while (signal_manager->ui_idle()) {}
		dcpomatic_sleep_seconds (1);
	}

	if (jm->errors ()) {
		int N = 0;
		for (auto i: jm->_jobs) {
			if (i->finished_in_error()) {
				++N;
			}
		}
		cerr << N << " errors.\n";

		for (auto i: jm->_jobs) {
			if (i->finished_in_error()) {
				cerr << i->name() << ":\n"
				     << "\tsummary: " << i->error_summary() << "\n"
				     << "\tdetails: " << i->error_details() << "\n";
			}
		}
	}

	while (signal_manager->ui_idle ()) {}

	if (jm->errors ()) {
		JobManager::drop ();
		return true;
	}

	return false;
}


class Memory
{
public:
	Memory () {}

	~Memory ()
	{
		free (data);
	}

	Memory (Memory const&) = delete;
	Memory& operator= (Memory const&) = delete;

	uint8_t* data = nullptr;
	size_t size = 0;
};


static void
png_write_data (png_structp png_ptr, png_bytep data, png_size_t length)
{
	auto mem = reinterpret_cast<Memory*>(png_get_io_ptr(png_ptr));
	size_t size = mem->size + length;

	if (mem->data) {
		mem->data = reinterpret_cast<uint8_t*>(realloc(mem->data, size));
	} else {
		mem->data = reinterpret_cast<uint8_t*>(malloc(size));
	}

	BOOST_REQUIRE (mem->data);

	memcpy (mem->data + mem->size, data, length);
	mem->size += length;
}


static void
png_flush (png_structp)
{

}


static void
png_error_fn (png_structp, char const * message)
{
	throw EncodeError (String::compose("Error during PNG write: %1", message));
}


void
write_image (shared_ptr<const Image> image, boost::filesystem::path file)
{
	int png_color_type = PNG_COLOR_TYPE_RGB;
	int bits_per_pixel = 0;
	switch (image->pixel_format()) {
	case AV_PIX_FMT_RGB24:
		bits_per_pixel = 8;
		break;
	case AV_PIX_FMT_XYZ12LE:
		bits_per_pixel = 16;
		break;
	default:
		BOOST_REQUIRE_MESSAGE (false, "unexpected pixel format " << image->pixel_format());
	}

	/* error handling? */
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, reinterpret_cast<void*>(const_cast<Image*>(image.get())), png_error_fn, 0);
	BOOST_REQUIRE (png_ptr);

	Memory state;

	png_set_write_fn (png_ptr, &state, png_write_data, png_flush);

	png_infop info_ptr = png_create_info_struct(png_ptr);
	BOOST_REQUIRE (info_ptr);

	png_set_IHDR (png_ptr, info_ptr, image->size().width, image->size().height, bits_per_pixel, png_color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	auto const width = image->size().width;
	auto const height = image->size().height;
	auto const stride = image->stride()[0];

	vector<vector<uint8_t>> data_to_write(height);
	for (int y = 0; y < height; ++y) {
		data_to_write[y].resize(stride);
	}

	switch (image->pixel_format()) {
	case AV_PIX_FMT_RGB24:
		for (int y = 0; y < height; ++y) {
			memcpy(data_to_write[y].data(), image->data()[0] + y * stride, stride);
		}
		break;
	case AV_PIX_FMT_XYZ12LE:
		/* 16-bit pixel values must be written MSB first */
		for (int y = 0; y < height; ++y) {
			data_to_write[y].resize(stride);
			uint8_t* original = image->data()[0] + y * stride;
			for (int x = 0; x < width * 3; ++x) {
				data_to_write[y][x * 2] = original[1];
				data_to_write[y][x * 2 + 1] = original[0];
				original += 2;
			}
		}
		break;
	default:
		break;
	}

	png_byte ** row_pointers = reinterpret_cast<png_byte**>(png_malloc(png_ptr, image->size().height * sizeof(png_byte *)));
	for (int y = 0; y < height; ++y) {
		row_pointers[y] = reinterpret_cast<png_byte*>(data_to_write[y].data());
	}

	png_write_info (png_ptr, info_ptr);
	png_write_image (png_ptr, row_pointers);
	png_write_end (png_ptr, info_ptr);

	png_destroy_write_struct (&png_ptr, &info_ptr);
	png_free (png_ptr, row_pointers);

	dcp::ArrayData(state.data, state.size).write(file);
}


void
check_ffmpeg (boost::filesystem::path ref, boost::filesystem::path check, int audio_tolerance)
{
	int const r = system (String::compose("ffcmp -t %1 %2 %3", audio_tolerance, ref.string(), check.string()).c_str());
	BOOST_REQUIRE_EQUAL (WEXITSTATUS(r), 0);
}

void
check_one_frame (boost::filesystem::path dcp_dir, int64_t index, boost::filesystem::path ref)
{
	dcp::DCP dcp (dcp_dir);
	dcp.read ();
	auto asset = dynamic_pointer_cast<dcp::MonoPictureAsset> (dcp.cpls().front()->reels().front()->main_picture()->asset());
	BOOST_REQUIRE (asset);
	auto frame = asset->start_read()->get_frame(index);
	auto ref_frame (new dcp::MonoPictureFrame (ref));

	auto image = frame->xyz_image ();
	auto ref_image = ref_frame->xyz_image ();

	BOOST_REQUIRE (image->size() == ref_image->size());

	int off = 0;
	for (int y = 0; y < ref_image->size().height; ++y) {
		for (int x = 0; x < ref_image->size().width; ++x) {
			BOOST_REQUIRE_EQUAL (ref_image->data(0)[off], image->data(0)[off]);
			BOOST_REQUIRE_EQUAL (ref_image->data(1)[off], image->data(1)[off]);
			BOOST_REQUIRE_EQUAL (ref_image->data(2)[off], image->data(2)[off]);
			++off;
		}
	}
}

boost::filesystem::path
dcp_file (shared_ptr<const Film> film, string prefix)
{
	using namespace boost::filesystem;

	vector<directory_entry> matches;
	std::copy_if(recursive_directory_iterator(film->dir(film->dcp_name())), recursive_directory_iterator(), std::back_inserter(matches), [&prefix](directory_entry const& entry) {
		return boost::algorithm::starts_with(entry.path().leaf().string(), prefix);
	});

	BOOST_REQUIRE_MESSAGE(matches.size() == 1, "Found " << matches.size() << " files with prefix " << prefix);
	return matches[0].path();
}

boost::filesystem::path
subtitle_file (shared_ptr<Film> film)
{
	for (auto i: boost::filesystem::recursive_directory_iterator(film->directory().get() / film->dcp_name(false))) {
		if (boost::algorithm::starts_with(i.path().leaf().string(), "sub_")) {
			return i.path();
		}
	}

	BOOST_REQUIRE (false);
	/* Remove warning */
	return boost::filesystem::path("/");
}


void
make_random_file (boost::filesystem::path path, size_t size)
{
	dcp::File random_file(path, "wb");
	BOOST_REQUIRE (random_file);

	boost::random::mt19937 rng(1);
	boost::random::uniform_int_distribution<uint64_t> dist(0);

	while (size > 0) {
		auto this_time = std::min(size, size_t(8));
		uint64_t random = dist(rng);
		random_file.write(&random, this_time, 1);
		size -= this_time;
	}
}


LogSwitcher::LogSwitcher (shared_ptr<Log> log)
	: _old (dcpomatic_log)
{
	dcpomatic_log = log;
}


LogSwitcher::~LogSwitcher ()
{
	dcpomatic_log = _old;
}

std::ostream&
dcp::operator<< (std::ostream& s, dcp::Size i)
{
	s << i.width << "x" << i.height;
	return s;
}

std::ostream&
dcp::operator<< (std::ostream& s, dcp::Standard t)
{
	switch (t) {
	case Standard::INTEROP:
		s << "interop";
		break;
	case Standard::SMPTE:
		s << "smpte";
		break;
	}
	return s;
}

std::ostream&
operator<< (std::ostream&s, VideoFrameType f)
{
	s << video_frame_type_to_string(f);
	return s;
}


void
Cleanup::add (boost::filesystem::path path)
{
	_paths.push_back (path);
}


void
Cleanup::run ()
{
	boost::system::error_code ec;
	for (auto i: _paths) {
		boost::filesystem::remove_all (i, ec);
	}
}


void stage (string, boost::optional<boost::filesystem::path>) {}
void progress (float) {}


void
verify_dcp(boost::filesystem::path dir, vector<dcp::VerificationNote::Code> ignore)
{
	auto notes = dcp::verify({dir}, &stage, &progress, {}, TestPaths::xsd());
	bool ok = true;
	for (auto i: notes) {
		if (find(ignore.begin(), ignore.end(), i.code()) == ignore.end()) {
			std::cout << "\t" << dcp::note_to_string(i) << "\n";
			ok = false;
		}
	}
	BOOST_CHECK(ok);
}


void
make_and_verify_dcp (shared_ptr<Film> film, vector<dcp::VerificationNote::Code> ignore)
{
	film->write_metadata ();
	make_dcp (film, TranscodeJob::ChangedBehaviour::IGNORE);
	BOOST_REQUIRE (!wait_for_jobs());
	verify_dcp({film->dir(film->dcp_name())}, ignore);
}


void
check_int_close (int a, int b, int d)
{
	BOOST_CHECK_MESSAGE (std::abs(a - b) < d, a << " differs from " << b << " by more than " << d);
}


void
check_int_close (std::pair<int, int> a, std::pair<int, int> b, int d)
{
	check_int_close (a.first, b.first, d);
	check_int_close (a.second, b.second, d);
}


ConfigRestorer::~ConfigRestorer()
{
	setup_test_config();
}


boost::filesystem::path
find_file (boost::filesystem::path dir, string filename_part)
{
	boost::optional<boost::filesystem::path> found;
	for (auto i: boost::filesystem::directory_iterator(dir)) {
		if (i.path().filename().string().find(filename_part) != string::npos) {
			BOOST_REQUIRE_MESSAGE(!found, "File " << filename_part << " found more than once in " << dir);
			found = i;
		}
	}
	BOOST_REQUIRE_MESSAGE(found, "Could not find " << filename_part << " in " << dir);
	return *found;
}


Editor::Editor(boost::filesystem::path path)
	: _path(path)
	, _content(dcp::file_to_string(path))
{

}

Editor::~Editor()
{
	auto f = fopen(_path.string().c_str(), "w");
	BOOST_REQUIRE(f);
	fwrite(_content.c_str(), _content.length(), 1, f);
	fclose(f);
}

void
Editor::replace(string a, string b)
{
	auto old_content = _content;
	boost::algorithm::replace_all(_content, a, b);
	BOOST_REQUIRE(_content != old_content);
}
