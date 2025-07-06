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
#include "config.h"
#include "mid_side_decoder.h"
#include "upmixer_a.h"
#include "upmixer_b.h"


using std::string;
using std::unique_ptr;
using std::vector;


vector<unique_ptr<const AudioProcessor>> AudioProcessor::_experimental;
vector<unique_ptr<const AudioProcessor>> AudioProcessor::_non_experimental;


void
AudioProcessor::setup_audio_processors()
{
	_non_experimental.push_back(unique_ptr<AudioProcessor>(new MidSideDecoder()));

	_experimental.push_back(unique_ptr<AudioProcessor>(new UpmixerA(48000)));
	_experimental.push_back(unique_ptr<AudioProcessor>(new UpmixerB(48000)));
}


AudioProcessor const *
AudioProcessor::from_id(string id)
{
	for (auto& i: _non_experimental) {
		if (i->id() == id) {
			return i.get();
		}
	}

	for (auto& i: _experimental) {
		if (i->id() == id) {
			return i.get();
		}
	}

	return nullptr;
}


vector<AudioProcessor const *>
AudioProcessor::visible()
{
	vector<AudioProcessor const *> raw;
	if (Config::instance()->show_experimental_audio_processors()) {
		for (auto& processor: _experimental) {
			raw.push_back(processor.get());
		}
	}

	for (auto& processor: _non_experimental) {
		raw.push_back(processor.get());
	}

	return raw;
}


vector<AudioProcessor const *>
AudioProcessor::all()
{
	vector<AudioProcessor const *> raw;
	for (auto& processor: _experimental) {
		raw.push_back(processor.get());
	}

	for (auto& processor: _non_experimental) {
		raw.push_back(processor.get());
	}

	return raw;
}
