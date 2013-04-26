/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <fstream>
#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time.hpp>
#include "format.h"
#include "film.h"
#include "filter.h"
#include "job_manager.h"
#include "util.h"
#include "exceptions.h"
#include "image.h"
#include "log.h"
#include "dcp_video_frame.h"
#include "config.h"
#include "server.h"
#include "cross.h"
#include "job.h"
#include "subtitle.h"
#include "scaler.h"
#include "ffmpeg_decoder.h"
#include "sndfile_decoder.h"
#include "dcp_content_type.h"
#include "trimmer.h"
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE dcpomatic_test
#include <boost/test/unit_test.hpp>

using std::string;
using std::list;
using std::stringstream;
using std::vector;
using boost::shared_ptr;
using boost::thread;
using boost::dynamic_pointer_cast;

void
setup_test_config ()
{
	Config::instance()->set_num_local_encoding_threads (1);
	Config::instance()->set_servers (vector<ServerDescription*> ());
	Config::instance()->set_server_port (61920);
	Config::instance()->set_default_dci_metadata (DCIMetadata ());
}

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
	boost::filesystem::path p = test_film_dir (name);
	if (boost::filesystem::exists (p)) {
		boost::filesystem::remove_all (p);
	}
	
	return shared_ptr<Film> (new Film (p.string(), false));
}


/* Check that Image::make_black works, and doesn't use values which crash
   sws_scale().
*/
BOOST_AUTO_TEST_CASE (make_black_test)
{
	/* This needs to happen in the first test */
	dcpomatic_setup ();

	libdcp::Size in_size (512, 512);
	libdcp::Size out_size (1024, 1024);

	list<AVPixelFormat> pix_fmts;
	pix_fmts.push_back (AV_PIX_FMT_RGB24);
	pix_fmts.push_back (AV_PIX_FMT_YUV420P);
	pix_fmts.push_back (AV_PIX_FMT_YUV422P10LE);
	pix_fmts.push_back (AV_PIX_FMT_YUV444P9LE);
	pix_fmts.push_back (AV_PIX_FMT_YUV444P9BE);
	pix_fmts.push_back (AV_PIX_FMT_YUV444P10LE);
	pix_fmts.push_back (AV_PIX_FMT_YUV444P10BE);
	pix_fmts.push_back (AV_PIX_FMT_UYVY422);

	int N = 0;
	for (list<AVPixelFormat>::const_iterator i = pix_fmts.begin(); i != pix_fmts.end(); ++i) {
		boost::shared_ptr<Image> foo (new SimpleImage (*i, in_size, true));
		foo->make_black ();
		boost::shared_ptr<Image> bar = foo->scale_and_convert_to_rgb (out_size, 0, Scaler::from_id ("bicubic"), true);
		
		uint8_t* p = bar->data()[0];
		for (int y = 0; y < bar->size().height; ++y) {
			uint8_t* q = p;
			for (int x = 0; x < bar->line_size()[0]; ++x) {
				BOOST_CHECK_EQUAL (*q++, 0);
			}
			p += bar->stride()[0];
		}

		++N;
	}
}

shared_ptr<const Image> trimmer_test_last_video;
shared_ptr<const AudioBuffers> trimmer_test_last_audio;

void
trimmer_test_video_helper (shared_ptr<const Image> image, bool, shared_ptr<Subtitle>)
{
	trimmer_test_last_video = image;
}

void
trimmer_test_audio_helper (shared_ptr<const AudioBuffers> audio)
{
	trimmer_test_last_audio = audio;
}

BOOST_AUTO_TEST_CASE (trimmer_passthrough_test)
{
	Trimmer trimmer (shared_ptr<Log> (), 0, 0, 200, 48000, 25, 25);
	trimmer.Video.connect (bind (&trimmer_test_video_helper, _1, _2, _3));
	trimmer.Audio.connect (bind (&trimmer_test_audio_helper, _1));

	shared_ptr<SimpleImage> video (new SimpleImage (PIX_FMT_RGB24, libdcp::Size (1998, 1080), true));
	shared_ptr<AudioBuffers> audio (new AudioBuffers (6, 42 * 1920));

	trimmer.process_video (video, false, shared_ptr<Subtitle> ());
	trimmer.process_audio (audio);

	BOOST_CHECK_EQUAL (video.get(), trimmer_test_last_video.get());
	BOOST_CHECK_EQUAL (audio.get(), trimmer_test_last_audio.get());
	BOOST_CHECK_EQUAL (audio->frames(), trimmer_test_last_audio->frames());
}


/** Test the audio handling of the Trimmer */
BOOST_AUTO_TEST_CASE (trimmer_audio_test)
{
	Trimmer trimmer (shared_ptr<Log> (), 25, 75, 200, 48000, 25, 25);

	trimmer.Audio.connect (bind (&trimmer_test_audio_helper, _1));

	/* 21 video frames-worth of audio frames; should be completely stripped */
	trimmer_test_last_audio.reset ();
	shared_ptr<AudioBuffers> audio (new AudioBuffers (6, 21 * 1920));
	trimmer.process_audio (audio);
	BOOST_CHECK (trimmer_test_last_audio == 0);

	/* 42 more video frames-worth, 4 should be stripped from the start */
	audio.reset (new AudioBuffers (6, 42 * 1920));
	trimmer.process_audio (audio);
	BOOST_CHECK (trimmer_test_last_audio);
	BOOST_CHECK_EQUAL (trimmer_test_last_audio->frames(), 38 * 1920);

	/* 42 more video frames-worth, should be kept as-is */
	trimmer_test_last_audio.reset ();
	audio.reset (new AudioBuffers (6, 42 * 1920));
	trimmer.process_audio (audio);
	BOOST_CHECK (trimmer_test_last_audio);
	BOOST_CHECK_EQUAL (trimmer_test_last_audio->frames(), 42 * 1920);

	/* 25 more video frames-worth, 5 should be trimmed from the end */
	trimmer_test_last_audio.reset ();
	audio.reset (new AudioBuffers (6, 25 * 1920));
	trimmer.process_audio (audio);
	BOOST_CHECK (trimmer_test_last_audio);
	BOOST_CHECK_EQUAL (trimmer_test_last_audio->frames(), 20 * 1920);

	/* Now some more; all should be trimmed */
	trimmer_test_last_audio.reset ();
	audio.reset (new AudioBuffers (6, 100 * 1920));
	trimmer.process_audio (audio);
	BOOST_CHECK (trimmer_test_last_audio == 0);
}


BOOST_AUTO_TEST_CASE (film_metadata_test)
{
	setup_test_config ();

	string const test_film = "build/test/film_metadata_test";
	
	if (boost::filesystem::exists (test_film)) {
		boost::filesystem::remove_all (test_film);
	}

	BOOST_CHECK_THROW (new Film (test_film, true), OpenFileError);
	
	shared_ptr<Film> f (new Film (test_film, false));
	f->_dci_date = boost::gregorian::from_undelimited_string ("20130211");
	BOOST_CHECK (f->format() == 0);
	BOOST_CHECK (f->dcp_content_type() == 0);
	BOOST_CHECK (f->filters ().empty());

	f->set_name ("fred");
//	BOOST_CHECK_THROW (f->set_content ("jim"), OpenFileError);
	f->set_dcp_content_type (DCPContentType::from_pretty_name ("Short"));
	f->set_format (Format::from_nickname ("Flat"));
	f->set_left_crop (1);
	f->set_right_crop (2);
	f->set_top_crop (3);
	f->set_bottom_crop (4);
	vector<Filter const *> f_filters;
	f_filters.push_back (Filter::from_id ("pphb"));
	f_filters.push_back (Filter::from_id ("unsharp"));
	f->set_filters (f_filters);
	f->set_trim_start (42);
	f->set_trim_end (99);
	f->set_ab (true);
	f->write_metadata ();

	stringstream s;
	s << "diff -u test/metadata.ref " << test_film << "/metadata";
	BOOST_CHECK_EQUAL (::system (s.str().c_str ()), 0);

	shared_ptr<Film> g (new Film (test_film, true));

	BOOST_CHECK_EQUAL (g->name(), "fred");
	BOOST_CHECK_EQUAL (g->dcp_content_type(), DCPContentType::from_pretty_name ("Short"));
	BOOST_CHECK_EQUAL (g->format(), Format::from_nickname ("Flat"));
	BOOST_CHECK_EQUAL (g->crop().left, 1);
	BOOST_CHECK_EQUAL (g->crop().right, 2);
	BOOST_CHECK_EQUAL (g->crop().top, 3);
	BOOST_CHECK_EQUAL (g->crop().bottom, 4);
	vector<Filter const *> g_filters = g->filters ();
	BOOST_CHECK_EQUAL (g_filters.size(), 2);
	BOOST_CHECK_EQUAL (g_filters.front(), Filter::from_id ("pphb"));
	BOOST_CHECK_EQUAL (g_filters.back(), Filter::from_id ("unsharp"));
	BOOST_CHECK_EQUAL (g->trim_start(), 42);
	BOOST_CHECK_EQUAL (g->trim_end(), 99);
	BOOST_CHECK_EQUAL (g->ab(), true);
	
	g->write_metadata ();
	BOOST_CHECK_EQUAL (::system (s.str().c_str ()), 0);
}

BOOST_AUTO_TEST_CASE (format_test)
{
	Format::setup_formats ();
	
	Format const * f = Format::from_nickname ("Flat");
	BOOST_CHECK (f);
	BOOST_CHECK_EQUAL (f->dcp_size().width, 1998);
	BOOST_CHECK_EQUAL (f->dcp_size().height, 1080);
	
	f = Format::from_nickname ("Scope");
	BOOST_CHECK (f);
	BOOST_CHECK_EQUAL (f->dcp_size().width, 2048);
	BOOST_CHECK_EQUAL (f->dcp_size().height, 858);
}

BOOST_AUTO_TEST_CASE (util_test)
{
	string t = "Hello this is a string \"with quotes\" and indeed without them";
	vector<string> b = split_at_spaces_considering_quotes (t);
	vector<string>::iterator i = b.begin ();
	BOOST_CHECK_EQUAL (*i++, "Hello");
	BOOST_CHECK_EQUAL (*i++, "this");
	BOOST_CHECK_EQUAL (*i++, "is");
	BOOST_CHECK_EQUAL (*i++, "a");
	BOOST_CHECK_EQUAL (*i++, "string");
	BOOST_CHECK_EQUAL (*i++, "with quotes");
	BOOST_CHECK_EQUAL (*i++, "and");
	BOOST_CHECK_EQUAL (*i++, "indeed");
	BOOST_CHECK_EQUAL (*i++, "without");
	BOOST_CHECK_EQUAL (*i++, "them");
}

class NullLog : public Log
{
public:
	void do_log (string) {}
};

BOOST_AUTO_TEST_CASE (md5_digest_test)
{
	string const t = md5_digest ("test/md5.test");
	BOOST_CHECK_EQUAL (t, "15058685ba99decdc4398c7634796eb0");

	BOOST_CHECK_THROW (md5_digest ("foobar"), OpenFileError);
}

void
do_remote_encode (shared_ptr<DCPVideoFrame> frame, ServerDescription* description, shared_ptr<EncodedData> locally_encoded)
{
	shared_ptr<EncodedData> remotely_encoded;
	BOOST_CHECK_NO_THROW (remotely_encoded = frame->encode_remotely (description));
	BOOST_CHECK (remotely_encoded);
	
	BOOST_CHECK_EQUAL (locally_encoded->size(), remotely_encoded->size());
	BOOST_CHECK (memcmp (locally_encoded->data(), remotely_encoded->data(), locally_encoded->size()) == 0);
}

BOOST_AUTO_TEST_CASE (client_server_test)
{
	shared_ptr<Image> image (new SimpleImage (PIX_FMT_RGB24, libdcp::Size (1998, 1080), true));
	uint8_t* p = image->data()[0];
	
	for (int y = 0; y < 1080; ++y) {
		uint8_t* q = p;
		for (int x = 0; x < 1998; ++x) {
			*q++ = x % 256;
			*q++ = y % 256;
			*q++ = (x + y) % 256;
		}
		p += image->stride()[0];
	}

	shared_ptr<Image> sub_image (new SimpleImage (PIX_FMT_RGBA, libdcp::Size (100, 200), true));
	p = sub_image->data()[0];
	for (int y = 0; y < 200; ++y) {
		uint8_t* q = p;
		for (int x = 0; x < 100; ++x) {
			*q++ = y % 256;
			*q++ = x % 256;
			*q++ = (x + y) % 256;
			*q++ = 1;
		}
		p += sub_image->stride()[0];
	}

	shared_ptr<Subtitle> subtitle (new Subtitle (Position (50, 60), sub_image));

	shared_ptr<FileLog> log (new FileLog ("build/test/client_server_test.log"));

	shared_ptr<DCPVideoFrame> frame (
		new DCPVideoFrame (
			image,
			subtitle,
			libdcp::Size (1998, 1080),
			0,
			0,
			1,
			Scaler::from_id ("bicubic"),
			0,
			24,
			"",
			0,
			200000000,
			log
			)
		);

	shared_ptr<EncodedData> locally_encoded = frame->encode_locally ();
	BOOST_ASSERT (locally_encoded);
	
	Server* server = new Server (log);

	new thread (boost::bind (&Server::run, server, 2));

	/* Let the server get itself ready */
	dcpomatic_sleep (1);

	ServerDescription description ("localhost", 2);

	list<thread*> threads;
	for (int i = 0; i < 8; ++i) {
		threads.push_back (new thread (boost::bind (do_remote_encode, frame, &description, locally_encoded)));
	}

	for (list<thread*>::iterator i = threads.begin(); i != threads.end(); ++i) {
		(*i)->join ();
	}

	for (list<thread*>::iterator i = threads.begin(); i != threads.end(); ++i) {
		delete *i;
	}
}

BOOST_AUTO_TEST_CASE (make_dcp_test)
{
	shared_ptr<Film> film = new_test_film ("make_dcp_test");
	film->set_name ("test_film2");
//	film->set_content ("../../../test/test.mp4");
	film->set_format (Format::from_nickname ("Flat"));
	film->set_dcp_content_type (DCPContentType::from_pretty_name ("Test"));
	film->make_dcp ();
	film->write_metadata ();

	while (JobManager::instance()->work_to_do ()) {
		dcpomatic_sleep (1);
	}
	
	BOOST_CHECK_EQUAL (JobManager::instance()->errors(), false);
}

/** Test Film::have_dcp().  Requires the output from make_dcp_test above */
BOOST_AUTO_TEST_CASE (have_dcp_test)
{
	boost::filesystem::path p = test_film_dir ("make_dcp_test");
	Film f (p.string ());
	BOOST_CHECK (f.have_dcp());

	p /= f.dcp_name();
	p /= f.dcp_video_mxf_filename();
	boost::filesystem::remove (p);
	BOOST_CHECK (!f.have_dcp ());
}

BOOST_AUTO_TEST_CASE (make_dcp_with_range_test)
{
	shared_ptr<Film> film = new_test_film ("make_dcp_with_range_test");
	film->set_name ("test_film3");
//	film->set_content ("../../../test/test.mp4");
//	film->examine_content ();
	film->set_format (Format::from_nickname ("Flat"));
	film->set_dcp_content_type (DCPContentType::from_pretty_name ("Test"));
	film->set_trim_end (42);
	film->make_dcp ();

	while (JobManager::instance()->work_to_do() && !JobManager::instance()->errors()) {
		dcpomatic_sleep (1);
	}

	BOOST_CHECK_EQUAL (JobManager::instance()->errors(), false);
}

/* Test best_dcp_frame_rate and FrameRateConversion */
BOOST_AUTO_TEST_CASE (best_dcp_frame_rate_test)
{
	/* Run some tests with a limited range of allowed rates */
	
	std::list<int> afr;
	afr.push_back (24);
	afr.push_back (25);
	afr.push_back (30);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	int best = best_dcp_frame_rate (60);
	FrameRateConversion frc = FrameRateConversion (60, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, true);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	
	best = best_dcp_frame_rate (50);
	frc = FrameRateConversion (50, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, true);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	best = best_dcp_frame_rate (48);
	frc = FrameRateConversion (48, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, true);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	
	best = best_dcp_frame_rate (30);
	frc = FrameRateConversion (30, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	best = best_dcp_frame_rate (29.97);
	frc = FrameRateConversion (29.97, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, true);
	
	best = best_dcp_frame_rate (25);
	frc = FrameRateConversion (25, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	best = best_dcp_frame_rate (24);
	frc = FrameRateConversion (24, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	best = best_dcp_frame_rate (14.5);
	frc = FrameRateConversion (14.5, best);
	BOOST_CHECK_EQUAL (best, 30);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, true);
	BOOST_CHECK_EQUAL (frc.change_speed, true);

	best = best_dcp_frame_rate (12.6);
	frc = FrameRateConversion (12.6, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, true);
	BOOST_CHECK_EQUAL (frc.change_speed, true);

	best = best_dcp_frame_rate (12.4);
	frc = FrameRateConversion (12.4, best);
	BOOST_CHECK_EQUAL (best, 25);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, true);
	BOOST_CHECK_EQUAL (frc.change_speed, true);

	best = best_dcp_frame_rate (12);
	frc = FrameRateConversion (12, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, true);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	/* Now add some more rates and see if it will use them
	   in preference to skip/repeat.
	*/

	afr.push_back (48);
	afr.push_back (50);
	afr.push_back (60);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	best = best_dcp_frame_rate (60);
	frc = FrameRateConversion (60, best);
	BOOST_CHECK_EQUAL (best, 60);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);
	
	best = best_dcp_frame_rate (50);
	frc = FrameRateConversion (50, best);
	BOOST_CHECK_EQUAL (best, 50);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	best = best_dcp_frame_rate (48);
	frc = FrameRateConversion (48, best);
	BOOST_CHECK_EQUAL (best, 48);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, false);

	/* Check some out-there conversions (not the best) */
	
	frc = FrameRateConversion (14.99, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, true);
	BOOST_CHECK_EQUAL (frc.change_speed, true);

	/* Check some conversions with limited DCP targets */

	afr.clear ();
	afr.push_back (24);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	best = best_dcp_frame_rate (25);
	frc = FrameRateConversion (25, best);
	BOOST_CHECK_EQUAL (best, 24);
	BOOST_CHECK_EQUAL (frc.skip, false);
	BOOST_CHECK_EQUAL (frc.repeat, false);
	BOOST_CHECK_EQUAL (frc.change_speed, true);
}

BOOST_AUTO_TEST_CASE (audio_sampling_rate_test)
{
	std::list<int> afr;
	afr.push_back (24);
	afr.push_back (25);
	afr.push_back (30);
	Config::instance()->set_allowed_dcp_frame_rates (afr);

	shared_ptr<Film> f = new_test_film ("audio_sampling_rate_test");
//	f->set_source_frame_rate (24);
	f->set_dcp_frame_rate (24);

//	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 48000, 0)));
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), 48000);

//	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 44100, 0)));
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), 48000);

//	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 80000, 0)));
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), 96000);

//	f->set_source_frame_rate (23.976);
	f->set_dcp_frame_rate (best_dcp_frame_rate (23.976));
//	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 48000, 0)));
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), 47952);

//	f->set_source_frame_rate (29.97);
	f->set_dcp_frame_rate (best_dcp_frame_rate (29.97));
	BOOST_CHECK_EQUAL (f->dcp_frame_rate (), 30);
//	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 48000, 0)));
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), 47952);

//	f->set_source_frame_rate (25);
	f->set_dcp_frame_rate (24);
//	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 48000, 0)));
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), 50000);

//	f->set_source_frame_rate (25);
	f->set_dcp_frame_rate (24);
//	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 44100, 0)));
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), 50000);

	/* Check some out-there conversions (not the best) */
	
//	f->set_source_frame_rate (14.99);
	f->set_dcp_frame_rate (25);
//	f->set_content_audio_stream (shared_ptr<AudioStream> (new FFmpegAudioStream ("a", 42, 16000, 0)));
	/* The FrameRateConversion within target_audio_sample_rate should choose to double-up
	   the 14.99 fps video to 30 and then run it slow at 25.
	*/
	BOOST_CHECK_EQUAL (f->target_audio_sample_rate(), rint (48000 * 2 * 14.99 / 25));
}

class TestJob : public Job
{
public:
	TestJob (shared_ptr<Film> f)
		: Job (f)
	{

	}

	void set_finished_ok () {
		set_state (FINISHED_OK);
	}

	void set_finished_error () {
		set_state (FINISHED_ERROR);
	}

	void run ()
	{
		while (1) {
			if (finished ()) {
				return;
			}
		}
	}

	string name () const {
		return "";
	}
};

BOOST_AUTO_TEST_CASE (job_manager_test)
{
	shared_ptr<Film> f;

	/* Single job */
	shared_ptr<TestJob> a (new TestJob (f));

	JobManager::instance()->add (a);
	dcpomatic_sleep (1);
	BOOST_CHECK_EQUAL (a->running (), true);
	a->set_finished_ok ();
	dcpomatic_sleep (2);
	BOOST_CHECK_EQUAL (a->finished_ok(), true);
}

BOOST_AUTO_TEST_CASE (compact_image_test)
{
	SimpleImage* s = new SimpleImage (PIX_FMT_RGB24, libdcp::Size (50, 50), false);
	BOOST_CHECK_EQUAL (s->components(), 1);
	BOOST_CHECK_EQUAL (s->stride()[0], 50 * 3);
	BOOST_CHECK_EQUAL (s->line_size()[0], 50 * 3);
	BOOST_CHECK (s->data()[0]);
	BOOST_CHECK (!s->data()[1]);
	BOOST_CHECK (!s->data()[2]);
	BOOST_CHECK (!s->data()[3]);

	/* copy constructor */
	SimpleImage* t = new SimpleImage (*s);
	BOOST_CHECK_EQUAL (t->components(), 1);
	BOOST_CHECK_EQUAL (t->stride()[0], 50 * 3);
	BOOST_CHECK_EQUAL (t->line_size()[0], 50 * 3);
	BOOST_CHECK (t->data()[0]);
	BOOST_CHECK (!t->data()[1]);
	BOOST_CHECK (!t->data()[2]);
	BOOST_CHECK (!t->data()[3]);
	BOOST_CHECK (t->data() != s->data());
	BOOST_CHECK (t->data()[0] != s->data()[0]);
	BOOST_CHECK (t->line_size() != s->line_size());
	BOOST_CHECK (t->line_size()[0] == s->line_size()[0]);
	BOOST_CHECK (t->stride() != s->stride());
	BOOST_CHECK (t->stride()[0] == s->stride()[0]);

	/* assignment operator */
	SimpleImage* u = new SimpleImage (PIX_FMT_YUV422P, libdcp::Size (150, 150), true);
	*u = *s;
	BOOST_CHECK_EQUAL (u->components(), 1);
	BOOST_CHECK_EQUAL (u->stride()[0], 50 * 3);
	BOOST_CHECK_EQUAL (u->line_size()[0], 50 * 3);
	BOOST_CHECK (u->data()[0]);
	BOOST_CHECK (!u->data()[1]);
	BOOST_CHECK (!u->data()[2]);
	BOOST_CHECK (!u->data()[3]);
	BOOST_CHECK (u->data() != s->data());
	BOOST_CHECK (u->data()[0] != s->data()[0]);
	BOOST_CHECK (u->line_size() != s->line_size());
	BOOST_CHECK (u->line_size()[0] == s->line_size()[0]);
	BOOST_CHECK (u->stride() != s->stride());
	BOOST_CHECK (u->stride()[0] == s->stride()[0]);

	delete s;
	delete t;
	delete u;
}

BOOST_AUTO_TEST_CASE (aligned_image_test)
{
	SimpleImage* s = new SimpleImage (PIX_FMT_RGB24, libdcp::Size (50, 50), true);
	BOOST_CHECK_EQUAL (s->components(), 1);
	/* 160 is 150 aligned to the nearest 32 bytes */
	BOOST_CHECK_EQUAL (s->stride()[0], 160);
	BOOST_CHECK_EQUAL (s->line_size()[0], 150);
	BOOST_CHECK (s->data()[0]);
	BOOST_CHECK (!s->data()[1]);
	BOOST_CHECK (!s->data()[2]);
	BOOST_CHECK (!s->data()[3]);

	/* copy constructor */
	SimpleImage* t = new SimpleImage (*s);
	BOOST_CHECK_EQUAL (t->components(), 1);
	BOOST_CHECK_EQUAL (t->stride()[0], 160);
	BOOST_CHECK_EQUAL (t->line_size()[0], 150);
	BOOST_CHECK (t->data()[0]);
	BOOST_CHECK (!t->data()[1]);
	BOOST_CHECK (!t->data()[2]);
	BOOST_CHECK (!t->data()[3]);
	BOOST_CHECK (t->data() != s->data());
	BOOST_CHECK (t->data()[0] != s->data()[0]);
	BOOST_CHECK (t->line_size() != s->line_size());
	BOOST_CHECK (t->line_size()[0] == s->line_size()[0]);
	BOOST_CHECK (t->stride() != s->stride());
	BOOST_CHECK (t->stride()[0] == s->stride()[0]);

	/* assignment operator */
	SimpleImage* u = new SimpleImage (PIX_FMT_YUV422P, libdcp::Size (150, 150), false);
	*u = *s;
	BOOST_CHECK_EQUAL (u->components(), 1);
	BOOST_CHECK_EQUAL (u->stride()[0], 160);
	BOOST_CHECK_EQUAL (u->line_size()[0], 150);
	BOOST_CHECK (u->data()[0]);
	BOOST_CHECK (!u->data()[1]);
	BOOST_CHECK (!u->data()[2]);
	BOOST_CHECK (!u->data()[3]);
	BOOST_CHECK (u->data() != s->data());
	BOOST_CHECK (u->data()[0] != s->data()[0]);
	BOOST_CHECK (u->line_size() != s->line_size());
	BOOST_CHECK (u->line_size()[0] == s->line_size()[0]);
	BOOST_CHECK (u->stride() != s->stride());
	BOOST_CHECK (u->stride()[0] == s->stride()[0]);

	delete s;
	delete t;
	delete u;
}

