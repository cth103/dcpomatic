/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  src/lib/audio_processor.h
 *  @brief AudioProcessor class.
 */


#ifndef DCPOMATIC_AUDIO_PROCESSOR_H
#define DCPOMATIC_AUDIO_PROCESSOR_H


#include "named_channel.h"
#include <dcp/types.h>
#include <memory>
#include <string>
#include <vector>


class AudioBuffers;
class AudioMapping;


/** @class AudioProcessor
 *  @brief A parent class for processors of audio data.
 *
 *  These are used to process data before it goes into the DCP, for things like
 *  stereo -> 5.1 upmixing.
 */
class AudioProcessor
{
public:
	virtual ~AudioProcessor() {}

	/** @return User-visible (translated) name */
	virtual std::string name() const = 0;
	/** @return An internal identifier */
	virtual std::string id() const = 0;
	/** @return Number of output channels */
	virtual int out_channels() const = 0;
	/** @return A clone of this AudioProcessor for operation at the specified sampling rate */
	virtual std::shared_ptr<AudioProcessor> clone(int sampling_rate) const = 0;
	/** Process some data, returning the processed result truncated or padded to `channels' */
	std::shared_ptr<AudioBuffers> run(std::shared_ptr<const AudioBuffers>, int channels);
	virtual void flush() {}
	/** Make the supplied audio mapping into a sensible default for this processor */
	virtual void make_audio_mapping_default(AudioMapping& mapping) const = 0;
	/** @return the user-visible (translated) names of each of our inputs, in order */
	virtual std::vector<NamedChannel> input_names() const = 0;

	static std::vector<AudioProcessor const *> all();
	static std::vector<AudioProcessor const *> visible();
	static void setup_audio_processors();
	static AudioProcessor const * from_id(std::string);
	static std::vector<dcp::Channel> pass_through();

protected:
	virtual std::shared_ptr<AudioBuffers> do_run(std::shared_ptr<const AudioBuffers>, int channels) = 0;

private:
	static std::vector<std::unique_ptr<const AudioProcessor>> _experimental;
	static std::vector<std::unique_ptr<const AudioProcessor>> _non_experimental;
};


#endif
