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


#include "job.h"
#include "sound_mxf_content.h"
#include "sound_mxf_examiner.h"
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_reader.h>
#include <dcp/sound_frame.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <asdcp/AS_DCP.h>
#include <asdcp/Metadata.h>
LIBDCP_ENABLE_WARNINGS
#include <algorithm>
#include <cstdlib>
#include <set>


using std::make_shared;
using std::shared_ptr;
using std::vector;


/** @return the channels (0-based) that have an MCA audio channel label in a sound asset */
static std::set<int>
labelled_channels(shared_ptr<const dcp::SoundAsset> asset)
{
	std::set<int> channels;

	if (!asset->file()) {
		return channels;
	}

	Kumu::FileReaderFactory factory;
	ASDCP::PCM::MXFReader reader(factory);
	if (ASDCP_FAILURE(reader.OpenRead(asset->file()->string().c_str()))) {
		return channels;
	}

	std::list<ASDCP::MXF::InterchangeObject*> labels;
	auto const r = reader.OP1aHeader().GetMDObjectsByType(
		ASDCP::DefaultSMPTEDict().ul(ASDCP::MDD_AudioChannelLabelSubDescriptor), labels
		);
	if (KM_FAILURE(r)) {
		return channels;
	}

	for (auto label: labels) {
		auto channel_label = dynamic_cast<ASDCP::MXF::AudioChannelLabelSubDescriptor*>(label);
		if (channel_label && !channel_label->MCAChannelID.empty()) {
			/* MCAChannelID counts from 1 */
			channels.insert(static_cast<int>(channel_label->MCAChannelID.get()) - 1);
		}
	}

	return channels;
}


/** @return for each channel, true if any of its samples is not silent.  We stop reading when all have been found.
 *  @param job Job to report progress to, or 0.
 */
static vector<bool>
channels_with_samples(shared_ptr<const dcp::SoundAsset> asset, shared_ptr<Job> job)
{
	int const channels = asset->channels();
	vector<bool> found(channels, false);

	/* Samples are compared as 64-bit integers, as the magnitude of a full scale negative sample does not fit in 32,
	 * with the sample in the top bits of 32.  A magnitude below 8192 is 32 steps of 24-bit sound (about -108dBFS): that
	 * is noise or dither, not a signal.
	 */
	int64_t const threshold = 8192;

	auto reader = asset->start_read();
	reader->set_check_hmac(false);

	auto const length = asset->intrinsic_duration();
	for (int64_t frame = 0; frame < length && std::find(found.begin(), found.end(), false) != found.end(); ++frame) {
		if (job) {
			job->set_progress(static_cast<float>(frame) / length);
		}
		auto sound_frame = reader->get_frame(frame);
		auto from = sound_frame->data();
		int const bytes = sound_frame->bits() / 8;
		int const samples = sound_frame->size() / (bytes * channels);
		for (int i = 0; i < samples; ++i) {
			for (int c = 0; c < channels; ++c) {
				int32_t value = 0;
				if (bytes == 3) {
					value = (from[0] << 8) | (from[1] << 16) | (static_cast<uint32_t>(from[2]) << 24);
				} else {
					value = (from[0] << 16) | (static_cast<uint32_t>(from[1]) << 24);
				}
				if (std::abs(static_cast<int64_t>(value)) >= threshold) {
					found[c] = true;
				}
				from += bytes;
			}
		}
	}

	return found;
}


SoundMXFExaminer::SoundMXFExaminer(shared_ptr<const SoundMXFContent> content, shared_ptr<Job> job)
	: _asset(make_shared<dcp::SoundAsset>(content->path(0)))
{
	auto const labelled = labelled_channels(_asset);
	auto const samples = channels_with_samples(_asset, job);

	bool any = false;
	for (int c = 0; c < _asset->channels(); ++c) {
		auto const sound = labelled.find(c) != labelled.end() || samples[c];
		_channels_with_sound.push_back(sound);
		any = any || sound;
	}

	if (!any) {
		_channels_with_sound.assign(_asset->channels(), true);
	}
}


int
SoundMXFExaminer::audio_channels() const
{
	return _asset->channels();
}


Frame
SoundMXFExaminer::audio_length() const
{
	auto const edit_rate = _asset->edit_rate();
	return _asset->intrinsic_duration() * _asset->sampling_rate() * edit_rate.denominator / edit_rate.numerator;
}


int
SoundMXFExaminer::audio_frame_rate() const
{
	return _asset->sampling_rate();
}


