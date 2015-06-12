/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

/** @file  test/audio_decoder_test.cc
 *  @brief Tests of the AudioDecoder class.
 */

#include <cassert>
#include <boost/test/unit_test.hpp>
#include "test.h"
#include "lib/audio_decoder.h"
#include "lib/single_stream_audio_content.h"

using std::string;
using std::cout;
using std::min;
using boost::shared_ptr;

class TestAudioContent : public SingleStreamAudioContent
{
public:
	TestAudioContent (shared_ptr<const Film> film)
		: Content (film)
		, SingleStreamAudioContent (film)
 	{
		_audio_stream.reset (new AudioStream (48000, 2));
	}

	std::string summary () const {
		return "";
	}

	DCPTime full_length () const {
		return DCPTime::from_seconds (float (audio_length()) / audio_stream()->frame_rate ());
	}
	
	Frame audio_length () const {
		return rint (61.2942 * audio_stream()->frame_rate ());
	}
};

class TestAudioDecoder : public AudioDecoder
{
public:
	TestAudioDecoder (shared_ptr<TestAudioContent> content)
		: AudioDecoder (content)
		, _test_audio_content (content)
		, _position (0)
	{}

	bool pass ()
	{
		Frame const N = min (
			Frame (2000),
			_test_audio_content->audio_length() - _position
			);

		shared_ptr<AudioBuffers> buffers (new AudioBuffers (_test_audio_content->audio_stream()->channels(), N));
		for (int i = 0; i < _test_audio_content->audio_stream()->channels(); ++i) {
			for (int j = 0; j < N; ++j) {
				buffers->data(i)[j] = j + _position;
			}
		}

		audio (_test_audio_content->audio_stream(), buffers, ContentTime::from_frames (_position, 48000));
		_position += N;

		return N < 2000;
	}

	void seek (ContentTime t, bool accurate)
	{
		AudioDecoder::seek (t, accurate);
		_position = t.frames (_test_audio_content->resampled_audio_frame_rate ());
	}

private:
	boost::shared_ptr<TestAudioContent> _test_audio_content;
	Frame _position;
};

shared_ptr<TestAudioContent> content;
shared_ptr<TestAudioDecoder> decoder;

static ContentAudio
get (Frame from, Frame length)
{
	decoder->seek (ContentTime::from_frames (from, content->resampled_audio_frame_rate ()), true);
	ContentAudio ca = decoder->get_audio (content->audio_stream(), from, length, true);
	BOOST_CHECK_EQUAL (ca.frame, from);
	return ca;
}

static void
check (Frame from, Frame length)
{
	ContentAudio ca = get (from, length);
	for (int i = 0; i < content->audio_stream()->channels(); ++i) {
		for (int j = 0; j < length; ++j) {
			BOOST_REQUIRE_EQUAL (ca.audio->data(i)[j], j + from);
		}
	}
}

/** Check the logic in AudioDecoder::get_audio */
BOOST_AUTO_TEST_CASE (audio_decoder_get_audio_test)
{
	shared_ptr<Film> film = new_test_film ("audio_decoder_test");

	content.reset (new TestAudioContent (film));
	decoder.reset (new TestAudioDecoder (content));
	
	/* Simple reads */
	check (0, 48000);
	check (44, 9123);
	check (9991, 22);

	/* Read off the end */

	Frame const from = content->resampled_audio_frame_rate() * 61;
	Frame const length = content->resampled_audio_frame_rate() * 4;
	ContentAudio ca = get (from, length);
	
	for (int i = 0; i < content->audio_stream()->channels(); ++i) {
		for (int j = 0; j < ca.audio->frames(); ++j) {
			BOOST_REQUIRE_EQUAL (ca.audio->data(i)[j], j + from);
		}
	}
}
