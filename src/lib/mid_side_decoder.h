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


#include "audio_processor.h"


class MidSideDecoder : public AudioProcessor
{
public:
	std::string name () const;
	std::string id () const;
	int out_channels () const;
	std::shared_ptr<AudioProcessor> clone (int) const;
	std::shared_ptr<AudioBuffers> run (std::shared_ptr<const AudioBuffers>, int channels);
	void make_audio_mapping_default (AudioMapping& mapping) const;
	std::vector<NamedChannel> input_names () const;
};
