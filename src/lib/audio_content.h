/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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
	static int const STREAMS;
	static int const GAIN;
	static int const DELAY;
};

class AudioContent : public ContentPart
{
public:
	AudioContent (Content* parent);
	AudioContent (Content* parent, std::vector<boost::shared_ptr<Content> >);

	void as_xml (xmlpp::Node *) const;
	std::string technical_summary () const;

	AudioMapping mapping () const;
	void set_mapping (AudioMapping);
	int resampled_frame_rate () const;
	bool has_rate_above_48k () const;
	std::vector<std::string> channel_names () const;

	void set_gain (double);
	void set_delay (int);

	double gain () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _gain;
	}

	int delay () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _delay;
	}

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

	static boost::shared_ptr<AudioContent> from_xml (Content* parent, cxml::ConstNodePtr);

private:

	AudioContent (Content* parent, cxml::ConstNodePtr);

	/** Gain to apply to audio in dB */
	double _gain;
	/** Delay to apply to audio (positive moves audio later) in milliseconds */
	int _delay;
	std::vector<AudioStreamPtr> _streams;
};

#endif
