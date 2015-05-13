/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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
#include "lib/audio_content.h"

using std::string;
using std::cout;
using std::min;
using boost::shared_ptr;

class TestAudioDecoder : public AudioDecoder
{
public:
	TestAudioDecoder (shared_ptr<AudioContent> content)
		: AudioDecoder (content)
		, _position (0)
	{}

	bool pass (PassReason)
	{
		AudioFrame const N = min (
			AudioFrame (2000),
			_audio_content->audio_length().frames (_audio_content->resampled_audio_frame_rate ()) - _position
			);

		shared_ptr<AudioBuffers> buffers (new AudioBuffers (_audio_content->audio_channels(), N));
		for (int i = 0; i < _audio_content->audio_channels(); ++i) {
			for (int j = 0; j < N; ++j) {
				buffers->data(i)[j] = j + _position;
			}
		}

		audio (buffers, ContentTime::from_frames (_position, _audio_content->resampled_audio_frame_rate ()));
		_position += N;

		return N < 2000;
	}

	void seek (ContentTime t, bool accurate)
	{
		AudioDecoder::seek (t, accurate);
		_position = t.frames (_audio_content->resampled_audio_frame_rate ());
	}

private:
	AudioFrame _position;
};

class TestAudioContent : public AudioContent
{
public:
	TestAudioContent (shared_ptr<Film> film)
		: Content (film)
		, AudioContent (film, DCPTime ())
	{}

	string summary () const {
		return "";
	}

	string information () const {
		return "";
	}

	DCPTime full_length () const {
		return DCPTime (audio_length().get ());
	}

	int audio_channels () const {
		return 2;
	}

	ContentTime audio_length () const {
		return ContentTime::from_seconds (61.2942);
	}

	int audio_frame_rate () const {
		return 48000;
	}

	AudioMapping audio_mapping () const {
		return AudioMapping (audio_channels ());
	}

	void set_audio_mapping (AudioMapping) {}
};

shared_ptr<TestAudioContent> content;
shared_ptr<TestAudioDecoder> decoder;

static shared_ptr<ContentAudio>
get (AudioFrame from, AudioFrame length)
{
	decoder->seek (ContentTime::from_frames (from, content->resampled_audio_frame_rate ()), true);
	shared_ptr<ContentAudio> ca = decoder->get_audio (from, length, true);
	BOOST_CHECK_EQUAL (ca->frame, from);
	return ca;
}

static void
check (AudioFrame from, AudioFrame length)
{
	shared_ptr<ContentAudio> ca = get (from, length);
	for (int i = 0; i < content->audio_channels(); ++i) {
		for (int j = 0; j < length; ++j) {
			BOOST_CHECK_EQUAL (ca->audio->data(i)[j], j + from);
			assert (ca->audio->data(i)[j] == j + from);
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

	AudioFrame const from = content->resampled_audio_frame_rate() * 61;
	AudioFrame const length = content->resampled_audio_frame_rate() * 4;
	shared_ptr<ContentAudio> ca = get (from, length);
	
	for (int i = 0; i < content->audio_channels(); ++i) {
		for (int j = 0; j < ca->audio->frames(); ++j) {
			BOOST_CHECK_EQUAL (ca->audio->data(i)[j], j + from);
			assert (ca->audio->data(i)[j] == j + from);
		}
	}
}
