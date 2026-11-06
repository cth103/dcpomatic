/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


#include "audio_examiner.h"
#include <memory>
#include <vector>


class Job;
class SoundMXFContent;


namespace dcp {
	class SoundAsset;
}


class SoundMXFExaminer : public AudioExaminer
{
public:
	/** @param job Job to report progress to, or 0 */
	SoundMXFExaminer(std::shared_ptr<const SoundMXFContent>, std::shared_ptr<Job> job);

	bool has_audio() const override {
		return true;
	}

	int audio_channels() const override;
	Frame audio_length() const override;
	int audio_frame_rate() const override;

	/** @return one entry for each channel of the file: true if the channel has sound.  A channel has sound if
	 *  the file labels it (MCA audio channel label) or if any of its samples is not silent.  If no channel has
	 *  sound at all (or the file is empty) every entry is true, so that nothing is left out by guesswork.
	 */
	std::vector<bool> channels_with_sound() const {
		return _channels_with_sound;
	}

private:
	std::shared_ptr<dcp::SoundAsset> _asset;
	std::vector<bool> _channels_with_sound;
};
