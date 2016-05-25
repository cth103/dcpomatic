/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/upmixer_a.h
 *  @brief UpmixerA class.
 */

#include "audio_processor.h"
#include "audio_filter.h"

/** @class UpmixerA
 *  @brief Stereo to 5.1 upmixer algorithm by Gérald Maruccia.
 */
class UpmixerA : public AudioProcessor
{
public:
	UpmixerA (int sampling_rate);

	std::string name () const;
	std::string id () const;
	int out_channels () const;
	boost::shared_ptr<AudioProcessor> clone (int) const;
	boost::shared_ptr<AudioBuffers> run (boost::shared_ptr<const AudioBuffers>, int channels);
	void flush ();
	void make_audio_mapping_default (AudioMapping& mapping) const;
	std::vector<std::string> input_names () const;

private:
	BandPassAudioFilter _left;
	BandPassAudioFilter _right;
	BandPassAudioFilter _centre;
	LowPassAudioFilter _lfe;
	BandPassAudioFilter _ls;
	BandPassAudioFilter _rs;
};
