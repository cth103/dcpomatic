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
#include "format.h"
#include "film.h"
#include "filter.h"
#include "job_manager.h"
#include "util.h"
#include "exceptions.h"
#include "dvd.h"
#include "delay_line.h"
#include "image.h"
#include "log.h"
#include "dcp_video_frame.h"
#include "config.h"
#include "server.h"
#include "cross.h"
#include "job.h"
#include "subtitle.h"
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE dvdomatic_test
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost;

void
setup_test_config ()
{
	Config::instance()->set_num_local_encoding_threads (1);
	Config::instance()->set_colour_lut_index (0);
	Config::instance()->set_j2k_bandwidth (200000000);
	Config::instance()->set_servers (vector<ServerDescription*> ());
	Config::instance()->set_server_port (61920);
}

BOOST_AUTO_TEST_CASE (film_metadata_test)
{
	dvdomatic_setup ();
	setup_test_config ();
	
	string const test_film = "build/test/film";
	
	if (boost::filesystem::exists (test_film)) {
		boost::filesystem::remove_all (test_film);
	}

	BOOST_CHECK_THROW (new Film ("build/test/film", true), OpenFileError);
	
	Film f (test_film, false);
	BOOST_CHECK (f.format() == 0);
	BOOST_CHECK (f.dcp_content_type() == 0);
	BOOST_CHECK (f.filters ().empty());

	f.set_name ("fred");
	BOOST_CHECK_THROW (f.set_content ("jim"), OpenFileError);
	f.set_dcp_content_type (DCPContentType::from_pretty_name ("Short"));
	f.set_format (Format::from_nickname ("Flat"));
	f.set_left_crop (1);
	f.set_right_crop (2);
	f.set_top_crop (3);
	f.set_bottom_crop (4);
	vector<Filter const *> f_filters;
	f_filters.push_back (Filter::from_id ("pphb"));
	f_filters.push_back (Filter::from_id ("unsharp"));
	f.set_filters (f_filters);
	f.set_dcp_frames (42);
	f.set_dcp_ab (true);
	f.write_metadata ();

	stringstream s;
	s << "diff -u test/metadata.ref " << test_film << "/metadata";
	BOOST_CHECK_EQUAL (::system (s.str().c_str ()), 0);

	Film g (test_film, true);

	BOOST_CHECK_EQUAL (g.name(), "fred");
	BOOST_CHECK_EQUAL (g.dcp_content_type(), DCPContentType::from_pretty_name ("Short"));
	BOOST_CHECK_EQUAL (g.format(), Format::from_nickname ("Flat"));
	BOOST_CHECK_EQUAL (g.crop().left, 1);
	BOOST_CHECK_EQUAL (g.crop().right, 2);
	BOOST_CHECK_EQUAL (g.crop().top, 3);
	BOOST_CHECK_EQUAL (g.crop().bottom, 4);
	vector<Filter const *> g_filters = g.filters ();
	BOOST_CHECK_EQUAL (g_filters.size(), 2);
	BOOST_CHECK_EQUAL (g_filters.front(), Filter::from_id ("pphb"));
	BOOST_CHECK_EQUAL (g_filters.back(), Filter::from_id ("unsharp"));
	BOOST_CHECK_EQUAL (g.dcp_frames(), 42);
	BOOST_CHECK_EQUAL (g.dcp_ab(), true);
	
	g.write_metadata ();
	BOOST_CHECK_EQUAL (::system (s.str().c_str ()), 0);
}

BOOST_AUTO_TEST_CASE (format_test)
{
	Format::setup_formats ();
	
	Format const * f = Format::from_nickname ("Flat");
	BOOST_CHECK (f);
	BOOST_CHECK_EQUAL (f->ratio_as_integer(0), 185);
	
	f = Format::from_nickname ("Scope");
	BOOST_CHECK (f);
	BOOST_CHECK_EQUAL (f->ratio_as_integer(0), 239);
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

BOOST_AUTO_TEST_CASE (dvd_test)
{
	list<DVDTitle> const t = dvd_titles ("test/dvd");
	BOOST_CHECK_EQUAL (t.size(), 3);
	list<DVDTitle>::const_iterator i = t.begin ();
	
	BOOST_CHECK_EQUAL (i->number, 1);
	BOOST_CHECK_EQUAL (i->size, 0);
	++i;
	
	BOOST_CHECK_EQUAL (i->number, 2);
	BOOST_CHECK_EQUAL (i->size, 14);
	++i;
	
	BOOST_CHECK_EQUAL (i->number, 3);
	BOOST_CHECK_EQUAL (i->size, 7);
}

void
do_positive_delay_line_test (int delay_length, int block_length)
{
	DelayLine d (delay_length);
	uint8_t data[block_length];

	int in = 0;
	int out = 0;
	int returned = 0;
	int zeros = 0;
	
	for (int i = 0; i < 64; ++i) {
		for (int j = 0; j < block_length; ++j) {
			data[j] = in;
			++in;
		}

		int const a = d.feed (data, block_length);
		returned += a;

		for (int j = 0; j < a; ++j) {
			if (zeros < delay_length) {
				BOOST_CHECK_EQUAL (data[j], 0);
				++zeros;
			} else {
				BOOST_CHECK_EQUAL (data[j], out & 0xff);
				++out;
			}
		}
	}

	BOOST_CHECK_EQUAL (returned, 64 * block_length);
}

void
do_negative_delay_line_test (int delay_length, int block_length)
{
	DelayLine d (delay_length);
	uint8_t data[block_length];

	int in = 0;
	int out = -delay_length;
	int returned = 0;
	
	for (int i = 0; i < 256; ++i) {
		for (int j = 0; j < block_length; ++j) {
			data[j] = in;
			++in;
		}

		int const a = d.feed (data, block_length);
		returned += a;

		for (int j = 0; j < a; ++j) {
			BOOST_CHECK_EQUAL (data[j], out & 0xff);
			++out;
		}
	}

	uint8_t remainder[-delay_length];
	d.get_remaining (remainder);
	returned += -delay_length;

	for (int i = 0; i < -delay_length; ++i) {
		BOOST_CHECK_EQUAL (remainder[i], 0);
		++out;
	}

	BOOST_CHECK_EQUAL (returned, 256 * block_length);
	
}

BOOST_AUTO_TEST_CASE (delay_line_test)
{
	do_positive_delay_line_test (64, 128);
	do_positive_delay_line_test (128, 64);
	do_positive_delay_line_test (3, 512);
	do_positive_delay_line_test (512, 3);

	do_positive_delay_line_test (0, 64);

	do_negative_delay_line_test (-64, 128);
	do_negative_delay_line_test (-128, 64);
	do_negative_delay_line_test (-3, 512);
	do_negative_delay_line_test (-512, 3);
}

BOOST_AUTO_TEST_CASE (md5_digest_test)
{
	string const t = md5_digest ("test/md5.test");
	BOOST_CHECK_EQUAL (t, "15058685ba99decdc4398c7634796eb0");

	BOOST_CHECK_THROW (md5_digest ("foobar"), OpenFileError);
}

BOOST_AUTO_TEST_CASE (paths_test)
{
	FilmState s;
	s.set_directory ("build/test/a/b/c/d/e");
	vector<int> thumbs;
	thumbs.push_back (42);
	s.set_thumbs (thumbs);
	BOOST_CHECK_EQUAL (s.thumb_file (0), "build/test/a/b/c/d/e/thumbs/00000042.png");

	s._content = "/foo/bar/baz";
	BOOST_CHECK_EQUAL (s.content_path(), "/foo/bar/baz");
	s._content = "foo/bar/baz";
	BOOST_CHECK_EQUAL (s.content_path(), "build/test/a/b/c/d/e/foo/bar/baz");
}

void
do_remote_encode (shared_ptr<DCPVideoFrame> frame, ServerDescription* description, shared_ptr<EncodedData> locally_encoded, int N)
{
	shared_ptr<EncodedData> remotely_encoded;
	BOOST_CHECK_NO_THROW (remotely_encoded = frame->encode_remotely (description));
	BOOST_CHECK (remotely_encoded);
	
	BOOST_CHECK_EQUAL (locally_encoded->size(), remotely_encoded->size());
	BOOST_CHECK (memcmp (locally_encoded->data(), remotely_encoded->data(), locally_encoded->size()) == 0);
}

BOOST_AUTO_TEST_CASE (client_server_test)
{
	shared_ptr<Image> image (new CompactImage (PIX_FMT_RGB24, Size (1998, 1080)));
	uint8_t* p = image->data()[0];
	
	for (int y = 0; y < 1080; ++y) {
		for (int x = 0; x < 1998; ++x) {
			*p++ = x % 256;
			*p++ = y % 256;
			*p++ = (x + y) % 256;
		}
	}

	shared_ptr<Image> sub_image (new CompactImage (PIX_FMT_RGBA, Size (100, 200)));
	p = sub_image->data()[0];
	for (int y = 0; y < 200; ++y) {
		for (int x = 0; x < 100; ++x) {
			*p++ = y % 256;
			*p++ = x % 256;
			*p++ = (x + y) % 256;
			*p++ = 1;
		}
	}

	shared_ptr<Subtitle> subtitle (new Subtitle (Position (50, 60), sub_image));

	FileLog log ("build/test/client_server_test.log");

	shared_ptr<DCPVideoFrame> frame (
		new DCPVideoFrame (
			image,
			subtitle,
			Size (1998, 1080),
			0,
			0,
			1,
			Scaler::from_id ("bicubic"),
			0,
			24,
			"",
			0,
			200000000,
			&log
			)
		);

	shared_ptr<EncodedData> locally_encoded = frame->encode_locally ();
	BOOST_ASSERT (locally_encoded);
	
	Server* server = new Server (&log);

	new thread (boost::bind (&Server::run, server, 2));

	/* Let the server get itself ready */
	dvdomatic_sleep (1);

	ServerDescription description ("localhost", 2);

	list<thread*> threads;
	for (int i = 0; i < 8; ++i) {
		threads.push_back (new thread (boost::bind (do_remote_encode, frame, &description, locally_encoded, i)));
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
	string const test_film = "build/test/film2";
	
	if (boost::filesystem::exists (test_film)) {
		boost::filesystem::remove_all (test_film);
	}
	
	Film film (test_film, false);
	film.set_name ("test_film2");
	film.set_content ("../../../test/test.mp4");
	film.set_format (Format::from_nickname ("Flat"));
	film.set_dcp_content_type (DCPContentType::from_pretty_name ("Test"));
	film.make_dcp (true);

	while (JobManager::instance()->work_to_do ()) {
		dvdomatic_sleep (1);
	}
	
	BOOST_CHECK_EQUAL (JobManager::instance()->errors(), false);
}

BOOST_AUTO_TEST_CASE (make_dcp_with_range_test)
{
	string const test_film = "build/test/film3";
	
	if (boost::filesystem::exists (test_film)) {
		boost::filesystem::remove_all (test_film);
	}
	
	Film film (test_film, false);
	film.set_name ("test_film3");
	film.set_content ("../../../test/test.mp4");
	film.examine_content ();
	film.set_format (Format::from_nickname ("Flat"));
	film.set_dcp_content_type (DCPContentType::from_pretty_name ("Test"));
	film.set_dcp_frames (42);
	film.make_dcp (true);

	while (JobManager::instance()->work_to_do() && !JobManager::instance()->errors()) {
		dvdomatic_sleep (1);
	}

	BOOST_CHECK_EQUAL (JobManager::instance()->errors(), false);
}

BOOST_AUTO_TEST_CASE (audio_sampling_rate_test)
{
	FilmState fs;
	fs.set_frames_per_second (24);

	fs.set_audio_sample_rate (48000);
	BOOST_CHECK_EQUAL (fs.target_audio_sample_rate(), 48000);

	fs.set_audio_sample_rate (44100);
	BOOST_CHECK_EQUAL (fs.target_audio_sample_rate(), 48000);

	fs.set_audio_sample_rate (80000);
	BOOST_CHECK_EQUAL (fs.target_audio_sample_rate(), 96000);

	fs.set_frames_per_second (23.976);
	fs.set_audio_sample_rate (48000);
	BOOST_CHECK_EQUAL (fs.target_audio_sample_rate(), 47952);

	fs.set_frames_per_second (29.97);
	fs.set_audio_sample_rate (48000);
	BOOST_CHECK_EQUAL (fs.target_audio_sample_rate(), 47952);
}

class TestJob : public Job
{
public:
	TestJob (shared_ptr<const FilmState> s, Log* l, shared_ptr<Job> req)
		: Job (s, l, req)
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
	shared_ptr<const FilmState> s;
	FileLog log ("build/test/job_manager_test.log");

	/* Single job, no dependency */
	shared_ptr<TestJob> a (new TestJob (s, &log, shared_ptr<Job> ()));

	JobManager::instance()->add (a);
	dvdomatic_sleep (1);
	BOOST_CHECK_EQUAL (a->running (), true);
	a->set_finished_ok ();
	dvdomatic_sleep (2);
	BOOST_CHECK_EQUAL (a->finished_ok(), true);

	/* Two jobs, dependency */
	a.reset (new TestJob (s, &log, shared_ptr<Job> ()));
	shared_ptr<TestJob> b (new TestJob (s, &log, a));

	JobManager::instance()->add (a);
	JobManager::instance()->add (b);
	dvdomatic_sleep (2);
	BOOST_CHECK_EQUAL (a->running(), true);
	BOOST_CHECK_EQUAL (b->running(), false);
	a->set_finished_ok ();
	dvdomatic_sleep (2);
	BOOST_CHECK_EQUAL (a->finished_ok(), true);
	BOOST_CHECK_EQUAL (b->running(), true);
	b->set_finished_ok ();
	dvdomatic_sleep (2);
	BOOST_CHECK_EQUAL (b->finished_ok(), true);

	/* Two jobs, dependency, first fails */
	a.reset (new TestJob (s, &log, shared_ptr<Job> ()));
	b.reset (new TestJob (s, &log, a));

	JobManager::instance()->add (a);
	JobManager::instance()->add (b);
	dvdomatic_sleep (2);
	BOOST_CHECK_EQUAL (a->running(), true);
	BOOST_CHECK_EQUAL (b->running(), false);
	a->set_finished_error ();
	dvdomatic_sleep (2);
	BOOST_CHECK_EQUAL (a->finished_in_error(), true);
	BOOST_CHECK_EQUAL (b->running(), false);
}

BOOST_AUTO_TEST_CASE (stream_test)
{
	AudioStream a ("4 9 hello there world");
	BOOST_CHECK_EQUAL (a.id(), 4);
	BOOST_CHECK_EQUAL (a.channels(), 9);
	BOOST_CHECK_EQUAL (a.name(), "hello there world");
	BOOST_CHECK_EQUAL (a.to_string(), "4 9 hello there world");

	SubtitleStream s ("5 a b c");
	BOOST_CHECK_EQUAL (s.id(), 5);
	BOOST_CHECK_EQUAL (s.name(), "a b c");
}
