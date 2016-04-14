/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/audio_content.h
 *  @brief AudioContent and AudioContentProperty classes.
 */

#ifndef DCPOMATIC_AUDIO_CONTENT_H
#define DCPOMATIC_AUDIO_CONTENT_H

#include "content_part.h"
#include "audio_stream.h"
#include "audio_mapping.h"

/** @class AudioContentProperty
 *  @brief Names for properties of AudioContent.
 */
class AudioContentProperty
{
public:
	static int const AUDIO_STREAMS;
	static int const AUDIO_GAIN;
	static int const AUDIO_DELAY;
	static int const AUDIO_VIDEO_FRAME_RATE;
};

class AudioContent : public ContentPart
{
public:
	AudioContent (Content* parent, boost::shared_ptr<const Film>);
	AudioContent (Content* parent, boost::shared_ptr<const Film>, cxml::ConstNodePtr);
	AudioContent (Content* parent, boost::shared_ptr<const Film>, std::vector<boost::shared_ptr<Content> >);

	void as_xml (xmlpp::Node *) const;
	std::string technical_summary () const;

	AudioMapping audio_mapping () const;
	void set_audio_mapping (AudioMapping);
	int resampled_audio_frame_rate () const;
	bool has_rate_above_48k () const;
	std::vector<std::string> audio_channel_names () const;

	void set_audio_gain (double);
	void set_audio_delay (int);

	double audio_gain () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _audio_gain;
	}

	int audio_delay () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _audio_delay;
	}

	double audio_video_frame_rate () const;
	void set_audio_video_frame_rate (double r);

	std::string processing_description () const;

	std::vector<AudioStreamPtr> streams () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _streams;
	}

	void add_stream (AudioStreamPtr stream);
	void set_stream (AudioStreamPtr stream);
	void set_streams (std::vector<AudioStreamPtr> streams);
	AudioStreamPtr stream () const;

	void add_properties (std::list<UserProperty> &) const;

private:

	/** Gain to apply to audio in dB */
	double _audio_gain;
	/** Delay to apply to audio (positive moves audio later) in milliseconds */
	int _audio_delay;
	boost::optional<double> _audio_video_frame_rate;
	std::vector<AudioStreamPtr> _streams;
};

#endif
