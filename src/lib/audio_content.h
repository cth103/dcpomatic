/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_AUDIO_CONTENT_H
#define DCPOMATIC_AUDIO_CONTENT_H

#include "content.h"
#include "audio_mapping.h"

namespace cxml {
	class Node;
}

class AudioContentProperty
{
public:
	static int const AUDIO_CHANNELS;
	static int const AUDIO_LENGTH;
	static int const AUDIO_FRAME_RATE;
	static int const AUDIO_GAIN;
	static int const AUDIO_DELAY;
	static int const AUDIO_MAPPING;
};

class AudioContent : public virtual Content
{
public:
	typedef int64_t Frame;
	
	AudioContent (boost::shared_ptr<const Film>, DCPTime);
	AudioContent (boost::shared_ptr<const Film>, boost::filesystem::path);
	AudioContent (boost::shared_ptr<const Film>, boost::shared_ptr<const cxml::Node>);
	AudioContent (boost::shared_ptr<const Film>, std::vector<boost::shared_ptr<Content> >);

	void as_xml (xmlpp::Node *) const;
	std::string technical_summary () const;

	virtual int audio_channels () const = 0;
	virtual AudioFrame audio_length () const = 0;
	virtual int content_audio_frame_rate () const = 0;
	virtual int output_audio_frame_rate () const = 0;
	virtual AudioMapping audio_mapping () const = 0;
	virtual void set_audio_mapping (AudioMapping) = 0;

	boost::signals2::connection analyse_audio (boost::function<void()>);
	boost::filesystem::path audio_analysis_path () const;

	void set_audio_gain (float);
	void set_audio_delay (int);
	
	float audio_gain () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _audio_gain;
	}

	int audio_delay () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _audio_delay;
	}

	Frame time_to_content_audio_frames (DCPTime, DCPTime) const;
	
private:
	/** Gain to apply to audio in dB */
	float _audio_gain;
	/** Delay to apply to audio (positive moves audio later) in milliseconds */
	int _audio_delay;
};

#endif
