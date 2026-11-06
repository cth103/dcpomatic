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


#include "audio_buffers.h"
#include "dcdm_audio_layout.h"


using std::make_shared;
using std::pair;
using std::set;
using std::shared_ptr;
using std::string;
using std::vector;


DCDMLayout
dcdm_layout(set<dcp::Channel> used)
{
	if (used.count(dcp::Channel::BSL) || used.count(dcp::Channel::BSR)) {
		return DCDMLayout::SEVEN_ONE;
	}

	return DCDMLayout::FIVE_ONE;
}


vector<pair<dcp::Channel, string>>
dcdm_extra_channels(set<dcp::Channel> used)
{
	pair<dcp::Channel, string> const extras[] = {
		{ dcp::Channel::HI, "HI" },
		{ dcp::Channel::VI, "VI" },
		{ dcp::Channel::SIGN_LANGUAGE, "SL" }
	};

	vector<pair<dcp::Channel, string>> out;
	for (auto const& extra: extras) {
		if (used.count(extra.first)) {
			out.push_back(extra);
		}
	}
	return out;
}


vector<string>
dcdm_channel_labels(DCDMLayout layout)
{
	vector<string> labels = { "L", "R", "C", "LFE", "Ls", "Rs" };
	if (layout == DCDMLayout::SEVEN_ONE) {
		labels.push_back("Lrs");
		labels.push_back("Rrs");
	}
	return labels;
}


vector<dcp::Channel>
dcdm_channels(DCDMLayout layout)
{
	vector<dcp::Channel> channels = {
		dcp::Channel::LEFT, dcp::Channel::RIGHT, dcp::Channel::CENTRE, dcp::Channel::LFE, dcp::Channel::LS, dcp::Channel::RS
	};
	if (layout == DCDMLayout::SEVEN_ONE) {
		channels.push_back(dcp::Channel::BSL);
		channels.push_back(dcp::Channel::BSR);
	}
	return channels;
}


shared_ptr<AudioBuffers>
dcdm_pick_channels(shared_ptr<const AudioBuffers> in, vector<dcp::Channel> const& channels)
{
	auto out = make_shared<AudioBuffers>(static_cast<int>(channels.size()), in->frames());
	out->make_silent();
	for (size_t i = 0; i < channels.size(); ++i) {
		auto const from = static_cast<int>(channels[i]);
		if (from < in->channels()) {
			out->copy_channel_from(in.get(), from, static_cast<int>(i));
		}
	}
	return out;
}


shared_ptr<AudioBuffers>
dcdm_layout_channels(shared_ptr<const AudioBuffers> in, DCDMLayout layout)
{
	return dcdm_pick_channels(in, dcdm_channels(layout));
}
