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
#include "audio_content.h"
#include "audio_stream.h"
#include "film.h"
#include "frame_rate_change.h"
#include "job.h"
#include "audio_mxf_content.h"
#include <asdcp/KM_log.h>
#include <dcp/exceptions.h>
#include <dcp/scope_guard.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_reader.h>
#include <dcp/sound_frame.h>
#include <libxml++/libxml++.h>
#include <algorithm>
#include <climits>
#include <cstdlib>

#include "i18n.h"


using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using namespace dcpomatic;


AudioMXFContent::AudioMXFContent(boost::filesystem::path path)
	: Content(path)
{

}


AudioMXFContent::AudioMXFContent(cxml::ConstNodePtr node, boost::optional<boost::filesystem::path> film_directory, int version)
	: Content(node, film_directory)
{
	audio = AudioContent::from_xml(this, node, version);
	audio->set_stream(
		make_shared<AudioStream>(
			node->number_child<int>("AudioFrameRate"),
			node->number_child<Frame>("AudioLength"),
			AudioMapping(node->node_child("AudioMapping"), version),
			24
			)
		);

	if (auto with_sound = node->optional_string_child("ChannelsWithSound")) {
		for (auto c: *with_sound) {
			_channels_with_sound.push_back(c == '1');
		}
	}
}


bool
AudioMXFContent::valid_mxf(boost::filesystem::path path)
{
	Kumu::DefaultLogSink().UnsetFilterFlag(Kumu::LOG_ALLOW_ALL);
	dcp::ScopeGuard sg = []() {
		Kumu::DefaultLogSink().SetFilterFlag(Kumu::LOG_ALLOW_ALL);
	};

	try {
		dcp::SoundAsset sound(path);
		return true;
	} catch (dcp::MXFFileError& e) {

	} catch (dcp::ReadError& e) {

	}

	return false;
}


/** @return for each channel, true if any of its samples is not silent.  We stop reading when all have been found.
 *  @param job Job to report progress to, or 0.
 */
static vector<bool>
channels_with_samples(dcp::SoundAsset const& asset, shared_ptr<Job> job)
{
	int const channels = asset.channels();
	vector<bool> found(channels, false);

	/* Samples are compared as 64-bit integers, as the magnitude of a full scale negative sample does not fit in 32,
	 * with the sample in the top bits of 32.  A magnitude below 8192 is 32 steps of 24-bit sound (about -108dBFS): that
	 * is noise or dither, not a signal.
	 */
	int64_t const threshold = 8192;

	auto reader = asset.start_read();
	reader->set_check_hmac(false);

	auto const length = asset.intrinsic_duration();
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


void
AudioMXFContent::examine(shared_ptr<const Film> film, shared_ptr<Job> job, bool tolerant)
{
	job->set_progress_unknown();

	Content::examine(film, job, tolerant);

	dcp::SoundAsset asset(path(0));
	if (asset.encrypted()) {
		throw DecodeError(_("This sound MXF is encrypted; an encrypted sound MXF cannot be added on its own."));
	}

	{
		boost::mutex::scoped_lock lm(_mutex);
		audio = make_shared<AudioContent>(this);
	}

	auto const edit_rate = asset.edit_rate();
	auto stream = make_shared<AudioStream>(
		asset.sampling_rate(),
		asset.intrinsic_duration() * asset.sampling_rate() * edit_rate.denominator / edit_rate.numerator,
		asset.channels(),
		24
	);

	/* A channel has sound if the file labels it or if some of its samples are not silent; if no channel has sound
	 * we map them all, as before, so that a wrong guess does not lose a channel.
	 */
	auto const labelled = asset.labelled_channels();
	auto const samples = channels_with_samples(asset, job);
	vector<bool> with_sound;
	for (int c = 0; c < asset.channels(); ++c) {
		with_sound.push_back(labelled.find(c) != labelled.end() || samples[c]);
	}
	if (std::find(with_sound.begin(), with_sound.end(), true) == with_sound.end()) {
		with_sound.assign(asset.channels(), true);
	}

	{
		boost::mutex::scoped_lock lm(_mutex);
		_channels_with_sound = with_sound;
	}

	audio->set_stream(stream);
	auto mapping = stream->mapping();
	mapping.make_default(film ? film->audio_processor() : nullptr);
	/* Channels with no sound are left unmapped */
	for (size_t input = 0; input < with_sound.size(); ++input) {
		if (!with_sound[input]) {
			mapping.unmap_input(input);
		}
	}
	stream->set_mapping(mapping);
}


string
AudioMXFContent::summary() const
{
	return fmt::format(_("{} [audio]"), path_summary());
}


string
AudioMXFContent::technical_summary() const
{
	return Content::technical_summary() + " - " + audio->technical_summary();
}


void
AudioMXFContent::as_xml(xmlpp::Element* element, bool with_paths, PathBehaviour path_behaviour, optional<boost::filesystem::path> film_directory) const
{
	cxml::add_text_child(element, "Type", "AudioMXF");
	Content::as_xml(element, with_paths, path_behaviour, film_directory);
	audio->as_xml(element);
	cxml::add_text_child(element, "AudioFrameRate", fmt::to_string(audio->stream()->frame_rate()));
	cxml::add_text_child(element, "AudioLength", fmt::to_string(audio->stream()->length()));
	audio->stream()->mapping().as_xml(cxml::add_child(element, "AudioMapping"));

	string with_sound;
	for (auto c: channels_with_sound()) {
		with_sound += c ? '1' : '0';
	}
	cxml::add_text_child(element, "ChannelsWithSound", with_sound);
}


DCPTime
AudioMXFContent::full_length(shared_ptr<const Film> film) const
{
	FrameRateChange const frc(film, shared_from_this());
	auto const stream = audio->stream();
	return DCPTime::from_frames(llrint(stream->length() / frc.speed_up()), stream->frame_rate());
}


DCPTime
AudioMXFContent::approximate_length() const
{
	return DCPTime::from_frames(audio->stream()->length(), audio->stream()->frame_rate());
}


void
AudioMXFContent::add_properties(shared_ptr<const Film> film, list<UserProperty>& p) const
{
	Content::add_properties(film, p);
	audio->add_properties(film, p);
}
