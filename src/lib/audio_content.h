/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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
	static int const FADE_IN;
	static int const FADE_OUT;
	static int const USE_SAME_FADES_AS_VIDEO;
};


class AudioContent : public ContentPart
{
public:
	explicit AudioContent (Content* parent);
	AudioContent (Content* parent, std::vector<std::shared_ptr<Content>>);
	AudioContent (Content* parent, cxml::ConstNodePtr);

	void as_xml (xmlpp::Node *) const;
	std::string technical_summary () const;
	void take_settings_from (std::shared_ptr<const AudioContent> c);

	AudioMapping mapping () const;
	void set_mapping (AudioMapping);
	int resampled_frame_rate (std::shared_ptr<const Film> film) const;
	std::vector<NamedChannel> channel_names () const;

	/** Set gain in dB */
	void set_gain (double);
	/** Set delay in milliseconds (positive moves audio later) */
	void set_delay (int);

	double gain () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _gain;
	}

	int delay () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _delay;
	}

	dcpomatic::ContentTime fade_in () const;
	dcpomatic::ContentTime fade_out () const;

	void set_fade_in (dcpomatic::ContentTime time);
	void set_fade_out (dcpomatic::ContentTime time);
	void set_use_same_fades_as_video (bool s);

	std::string processing_description (std::shared_ptr<const Film> film) const;

	std::vector<AudioStreamPtr> streams () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _streams;
	}

	void add_stream (AudioStreamPtr stream);
	void set_stream (AudioStreamPtr stream);
	void set_streams (std::vector<AudioStreamPtr> streams);
	AudioStreamPtr stream () const;

	void add_properties (std::shared_ptr<const Film> film, std::list<UserProperty> &) const;

	void modify_position (std::shared_ptr<const Film> film, dcpomatic::DCPTime& pos) const;
	void modify_trim_start(std::shared_ptr<const Film> film, dcpomatic::ContentTime& pos) const;

	/** @param frame frame within the whole (untrimmed) content.
	 *  @param frame_rate The frame rate of the audio (it may have been resampled).
	 *  @return a fade coefficient for @ref length samples starting at an offset @frame within
	 *  the content, or an empty vector if the given section has no fade.
	 */
	std::vector<float> fade (AudioStreamPtr stream, Frame frame, Frame length, int frame_rate) const;

	static std::shared_ptr<AudioContent> from_xml (Content* parent, cxml::ConstNodePtr, int version);

private:

	/** Gain to apply to audio in dB */
	double _gain = 0;
	/** Delay to apply to audio (positive moves audio later) in milliseconds */
	int _delay = 0;
	dcpomatic::ContentTime _fade_in;
	dcpomatic::ContentTime _fade_out;
	bool _use_same_fades_as_video = false;
	std::vector<AudioStreamPtr> _streams;
};

#endif
