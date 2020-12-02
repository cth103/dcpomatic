/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

/** @file src/lib/audio_decoder.h
 *  @brief Parent class for audio decoders.
 */

#ifndef DCPOMATIC_AUDIO_DECODER_H
#define DCPOMATIC_AUDIO_DECODER_H

#include "decoder.h"
#include "content_audio.h"
#include "audio_stream.h"
#include "decoder_part.h"
#include <boost/enable_shared_from_this.hpp>
#include <boost/signals2.hpp>

class AudioBuffers;
class AudioContent;
class AudioDecoderStream;
class Log;
class Film;
class Resampler;

/** @class AudioDecoder.
 *  @brief Parent class for audio decoders.
 */
class AudioDecoder : public boost::enable_shared_from_this<AudioDecoder>, public DecoderPart
{
public:
	AudioDecoder (Decoder* parent, boost::shared_ptr<const AudioContent> content, bool fast);

	boost::optional<dcpomatic::ContentTime> position (boost::shared_ptr<const Film> film) const;
	void emit (boost::shared_ptr<const Film> film, AudioStreamPtr stream, boost::shared_ptr<const AudioBuffers>, dcpomatic::ContentTime, bool time_already_delayed = false);
	void seek ();
	void flush ();

	dcpomatic::ContentTime stream_position (boost::shared_ptr<const Film> film, AudioStreamPtr stream) const;

	boost::signals2::signal<void (AudioStreamPtr, ContentAudio)> Data;

private:
	void silence (int milliseconds);

	boost::shared_ptr<const AudioContent> _content;
	/** Frame after the last one that was emitted from Data (i.e. at the resampled rate, if applicable)
	 *  for each AudioStream.
	 */
	typedef std::map<AudioStreamPtr, Frame> PositionMap;
	PositionMap _positions;
	typedef std::map<AudioStreamPtr, boost::shared_ptr<Resampler> > ResamplerMap;
	ResamplerMap _resamplers;

	bool _fast;
};

#endif
