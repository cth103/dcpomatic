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

#include "i18n.h"


using std::shared_ptr;
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


shared_ptr<AudioBuffers>
AudioProcessor::run(std::shared_ptr<const AudioBuffers> in, int channels)
{
	auto out = do_run(in, std::min(channels, out_channels()));

	if (out->channels() < channels) {
		out->set_channels(channels);
	}

	for (auto const pass: pass_through()) {
		if (static_cast<int>(pass) < channels && static_cast<int>(pass) < out->channels()) {
			out->copy_channel_from(in.get(), static_cast<int>(pass), static_cast<int>(pass));
		}
	}

	return out;
}


void
AudioProcessor::make_audio_mapping_default(AudioMapping& mapping) const
{
	mapping.make_zero();

	auto const channels = std::min(mapping.input_channels(), mapping.output_channels());

	for (auto pass: pass_through()) {
		if (static_cast<int>(pass) < channels) {
			mapping.set(pass, static_cast<int>(pass), 1);
		}
	}
}


vector<NamedChannel>
AudioProcessor::input_names() const
{
	return {
		NamedChannel(_("HI"), 6),
		NamedChannel(_("VI"), 7),
		NamedChannel(_("DBP"), 13),
		NamedChannel(_("DBS"), 14),
		NamedChannel(_("Sign"), 15)
	};
}


vector<dcp::Channel>
AudioProcessor::pass_through()
{
	return {
		dcp::Channel::HI,
		dcp::Channel::VI,
		dcp::Channel::MOTION_DATA,
		dcp::Channel::SYNC_SIGNAL,
		dcp::Channel::SIGN_LANGUAGE
	};
}
