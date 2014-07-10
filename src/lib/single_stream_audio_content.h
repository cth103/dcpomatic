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

/** @file  src/lib/single_stream_audio_content.h
 *  @brief SingleStreamAudioContent class.
 */

#ifndef DCPOMATIC_SINGLE_STREAM_AUDIO_CONTENT_H
#define DCPOMATIC_SINGLE_STREAM_AUDIO_CONTENT_H

#include "audio_content.h"

class AudioExaminer;

/** @class SingleStreamAudioContent
 *  @brief A piece of AudioContent that has a single audio stream.
 */
class SingleStreamAudioContent : public AudioContent
{
public:
	SingleStreamAudioContent (boost::shared_ptr<const Film>);
	SingleStreamAudioContent (boost::shared_ptr<const Film>, boost::filesystem::path);
	SingleStreamAudioContent (boost::shared_ptr<const Film> f, cxml::ConstNodePtr node, int version);

	void as_xml (xmlpp::Node* node) const;

	/* AudioContent */
	int audio_channels () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _audio_channels;
	}
	
	ContentTime audio_length () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _audio_length;
	}
	
	int audio_frame_rate () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _audio_frame_rate;
	}

	AudioMapping audio_mapping () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _audio_mapping;
	}

	void set_audio_mapping (AudioMapping);

	void take_from_audio_examiner (boost::shared_ptr<AudioExaminer>);

protected:
	int _audio_channels;
	ContentTime _audio_length;
	int _audio_frame_rate;
	AudioMapping _audio_mapping;
};

#endif
