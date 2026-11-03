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
#include "overlaps.h"
#include "sound_mxf_content.h"
#include "sound_mxf_examiner.h"
#include "util.h"
#include <asdcp/KM_log.h>
#include <dcp/exceptions.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_reader.h>
#include <dcp/sound_frame.h>
#include <libxml++/libxml++.h>
#include <algorithm>
#include <climits>
#include <cmath>

#include "i18n.h"


using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using namespace dcpomatic;


SoundMXFContent::SoundMXFContent(boost::filesystem::path path)
	: Content(path)
{

}


SoundMXFContent::SoundMXFContent(cxml::ConstNodePtr node, boost::optional<boost::filesystem::path> film_directory, int version)
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

	for (auto c: node->string_child("ChannelsWithSound")) {
		_channels_with_sound.push_back(c == '1');
	}

	_edit_rate = dcp::Fraction(node->number_child<int>("EditRateNumerator"), node->number_child<int>("EditRateDenominator"));
	_standard = node->string_child("Standard") == "Interop" ? dcp::Standard::INTEROP : dcp::Standard::SMPTE;
	_asset_id = node->optional_string_child("AssetId");
}


bool
SoundMXFContent::valid_mxf(boost::filesystem::path path)
{
	Kumu::DefaultLogSink().UnsetFilterFlag(Kumu::LOG_ALLOW_ALL);

	try {
		dcp::SoundAsset sound(path);
		Kumu::DefaultLogSink().SetFilterFlag(Kumu::LOG_ALLOW_ALL);
		return true;
	} catch (dcp::MXFFileError& e) {

	} catch (dcp::ReadError& e) {

	}

	Kumu::DefaultLogSink().SetFilterFlag(Kumu::LOG_ALLOW_ALL);

	return false;
}


void
SoundMXFContent::examine(shared_ptr<const Film> film, shared_ptr<Job> job, bool tolerant)
{
	job->set_progress_unknown();

	Content::examine(film, job, tolerant);

	dcp::SoundAsset asset(path(0));
	if (asset.encrypted()) {
		throw DecodeError(_("This sound MXF is encrypted; an encrypted sound MXF cannot be added on its own."));
	}

	SoundMXFExaminer examiner(shared_from_this(), job);

	{
		boost::mutex::scoped_lock lm(_mutex);
		audio = make_shared<AudioContent>(this);
	}

	auto const with_sound = examiner.channels_with_sound();
	{
		boost::mutex::scoped_lock lm(_mutex);
		_channels_with_sound = with_sound;
		_edit_rate = examiner.edit_rate();
		_standard = asset.standard();
		_asset_id = asset.id();
	}

	auto stream = make_shared<AudioStream>(examiner.audio_frame_rate(), examiner.audio_length(), examiner.audio_channels(), 24);
	audio->set_stream(stream);
	auto mapping = stream->mapping();
	mapping.make_default(film ? film->audio_processor() : nullptr);
	/* Channels with no sound are left unmapped */
	for (size_t input = 0; input < with_sound.size(); ++input) {
		if (!with_sound[input]) {
			for (int output = 0; output < mapping.output_channels(); ++output) {
				mapping.set(input, output, 0);
			}
		}
	}
	stream->set_mapping(mapping);
}


string
SoundMXFContent::summary() const
{
	return fmt::format(_("{} [audio]"), path_summary());
}


string
SoundMXFContent::technical_summary() const
{
	return Content::technical_summary() + " - " + audio->technical_summary();
}


void
SoundMXFContent::as_xml(xmlpp::Element* element, bool with_paths, PathBehaviour path_behaviour, optional<boost::filesystem::path> film_directory) const
{
	cxml::add_text_child(element, "Type", "SoundMXF");
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

	boost::mutex::scoped_lock lm(_mutex);
	cxml::add_text_child(element, "EditRateNumerator", fmt::to_string(_edit_rate.numerator));
	cxml::add_text_child(element, "EditRateDenominator", fmt::to_string(_edit_rate.denominator));
	cxml::add_text_child(element, "Standard", _standard == dcp::Standard::INTEROP ? "Interop" : "SMPTE");
	if (_asset_id) {
		cxml::add_text_child(element, "AssetId", *_asset_id);
	}
}


DCPTime
SoundMXFContent::full_length(shared_ptr<const Film> film) const
{
	FrameRateChange const frc(film, shared_from_this());
	auto const stream = audio->stream();
	return DCPTime::from_frames(llrint(stream->length() / frc.speed_up()), stream->frame_rate());
}


DCPTime
SoundMXFContent::approximate_length() const
{
	return DCPTime::from_frames(audio->stream()->length(), audio->stream()->frame_rate());
}


bool
SoundMXFContent::can_reference_audio(shared_ptr<const Film> film, string& why_not) const
{
	if (!audio || !audio->stream()) {
		why_not = "it has no audio";
		return false;
	}

	auto const stream = audio->stream();

	dcp::Fraction edit_rate;
	vector<bool> with_sound;
	dcp::Standard standard;
	{
		boost::mutex::scoped_lock lm(_mutex);
		edit_rate = _edit_rate;
		with_sound = _channels_with_sound;
		standard = _standard;
	}

	if (edit_rate.denominator != 1 || edit_rate.numerator != film->video_frame_rate()) {
		why_not = "its edit rate is not the film's frame rate";
		return false;
	}

	if (FrameRateChange(film, shared_from_this()).change_speed()) {
		why_not = "the picture it plays with changes its speed";
		return false;
	}

	/* A trim of audio can be any number of samples, but an entry point and a duration are whole frames */
	auto const rate = static_cast<double>(edit_rate.numerator);
	if (trim_start() != trim_start().round(rate) || trim_end() != trim_end().round(rate)) {
		why_not = "its trims are not whole frames";
		return false;
	}

	if (film->limit_to_smpte_bv20()) {
		/* Bv2.0 forbids the MCA sub-descriptors that the file may have */
		why_not = "the film is limited to SMPTE Bv2.0";
		return false;
	}

	if ((standard == dcp::Standard::INTEROP) != film->interop()) {
		why_not = standard == dcp::Standard::INTEROP ? "it is Interop and the film is SMPTE" : "it is SMPTE and the film is Interop";
		return false;
	}

	if (film->encrypt_sound()) {
		why_not = "the film encrypts its sound";
		return false;
	}

	if (stream->frame_rate() != film->audio_frame_rate()) {
		why_not = "its sampling rate is not the film's";
		return false;
	}

	if (stream->channels() != film->audio_channels()) {
		why_not = "its number of channels is not the film's";
		return false;
	}

	if (film->audio_processor()) {
		why_not = "the film has an audio processor";
		return false;
	}

	if (film->contains_atmos_content()) {
		/* The sound of a film with Atmos has the Atmos sync signal in channel 14, so it is written afresh */
		why_not = "the film has Atmos";
		return false;
	}

	if (audio->gain() != 0 || audio->delay() != 0 || audio->fade_in() != ContentTime() || audio->fade_out() != ContentTime()) {
		why_not = "its gain, delay or fades are set";
		return false;
	}

	/* The channels that have sound must go one to one to the DCP; the others must be silent either way */
	auto const mapping = stream->mapping();
	for (int input = 0; input < stream->channels(); ++input) {
		auto const has_sound = static_cast<size_t>(input) >= with_sound.size() || with_sound[input];
		bool one_to_one = true;
		bool none = true;
		for (int output = 0; output < mapping.output_channels(); ++output) {
			auto const gain = mapping.get(input, output);
			if (std::abs(gain - (input == output ? 1.0F : 0.0F)) > 1e-6) {
				one_to_one = false;
			}
			if (std::abs(gain) > 1e-6) {
				none = false;
			}
		}
		if (!one_to_one && (has_sound || !none)) {
			why_not = "its mapping is not one to one";
			return false;
		}
	}

	auto part = [](shared_ptr<const Content> c) {
		return static_cast<bool>(c->audio);
	};

	auto const others = dcpomatic::overlaps(film, film->content(), part, position(), end(film));
	if (others.size() != 1 || others.front().get() != this) {
		why_not = "other audio overlaps it";
		return false;
	}

	/* The film's reels must start and end where this content does, so that the file is one whole reel */
	auto const reels = film->reels();
	if (std::find(reels.begin(), reels.end(), DCPTimePeriod(position(), end(film))) == reels.end()) {
		why_not = "the film's reels are not the length of the content";
		return false;
	}

	return true;
}


shared_ptr<AudioBuffers>
sound_mxf_frame_audio(shared_ptr<const dcp::SoundFrame> sound_frame, int channels)
{
	auto from = sound_frame->data();
	int const frames = sound_frame->size() / (sound_frame->bits() * channels / 8);
	auto data = make_shared<AudioBuffers>(channels, frames);
	auto data_data = data->data();

	switch (sound_frame->bits()) {
	case 24:
		for (int i = 0; i < frames; ++i) {
			for (int j = 0; j < channels; ++j) {
				data_data[j][i] = static_cast<int>((from[0] << 8) | (from[1] << 16) | (from[2] << 24)) / static_cast<float>(INT_MAX - 256);
				from += 3;
			}
		}
		break;
	case 16:
		for (int i = 0; i < frames; ++i) {
			for (int j = 0; j < channels; ++j) {
				data_data[j][i] = static_cast<int>(from[0] << 16 | (from[1] << 24)) / static_cast<float>(INT_MAX - 256);
				from += 2;
			}
		}
		break;
	}

	return data;
}


string
SoundMXFContent::sound_digest(shared_ptr<const Film> film) const
{
	dcp::Fraction edit_rate;
	optional<string> asset_id;
	{
		boost::mutex::scoped_lock lm(_mutex);
		edit_rate = _edit_rate;
		asset_id = _asset_id;
	}

	return fmt::format(
		"{}|{}/{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}",
		asset_id ? *asset_id : dcp::SoundAsset(path(0)).id(),
		edit_rate.numerator,
		edit_rate.denominator,
		trim_start().get(),
		trim_end().get(),
		audio->gain(),
		audio->delay(),
		audio->fade_in().get(),
		audio->fade_out().get(),
		audio->stream()->mapping().digest(),
		film->audio_channels(),
		film->audio_frame_rate(),
		film->video_frame_rate(),
		film->interop(),
		film->encrypt_sound() ? 1 : 0
		);
}


bool
SoundMXFContent::is_whole_reel(shared_ptr<const Film> film) const
{
	auto const period = DCPTimePeriod(position(), end(film));
	auto const reels = film->reels();
	auto has_audio = [](shared_ptr<const Content> c) { return static_cast<bool>(c->audio); };
	return std::find(reels.begin(), reels.end(), period) != reels.end()
		&& dcpomatic::overlaps(film, film->content(), has_audio, period.from, period.to).size() == 1;
}


shared_ptr<const SoundMXFContent>
SoundMXFContent::shared_sound_source(shared_ptr<const Film> film) const
{
	/* The Atmos sync signal changes from frame to frame, and an audio processor has state, so those reels differ */
	if (film->contains_atmos_content() || film->audio_processor()) {
		return {};
	}

	/* A delay moves sound across the reel's boundaries, and a speed change makes other sound of another length */
	if (audio->delay() != 0 || !frames_align_with(film)) {
		return {};
	}

	string why_not;
	if (!is_whole_reel(film) || can_reference_audio(film, why_not)) {
		return {};
	}

	auto const digest = sound_digest(film);
	shared_ptr<const SoundMXFContent> found;
	for (auto content: film->content()) {
		auto other = std::dynamic_pointer_cast<const SoundMXFContent>(content);
		if (!other || other.get() == this || !other->is_whole_reel(film)) {
			continue;
		}
		if (other->position() >= position() || other->can_reference_audio(film, why_not)) {
			continue;
		}
		if (other->sound_digest(film) == digest && (!found || other->position() < found->position())) {
			found = other;
		}
	}

	return found;
}


bool
SoundMXFContent::frames_align_with(shared_ptr<const Film> film) const
{
	dcp::Fraction edit_rate;
	{
		boost::mutex::scoped_lock lm(_mutex);
		edit_rate = _edit_rate;
	}

	return edit_rate.denominator == 1 && edit_rate.numerator == film->video_frame_rate() && !FrameRateChange(film, shared_from_this()).change_speed();
}


Frame
SoundMXFContent::head_frames() const
{
	dcp::Fraction edit_rate;
	{
		boost::mutex::scoped_lock lm(_mutex);
		edit_rate = _edit_rate;
	}
	return trim_start().frames_round(static_cast<double>(edit_rate.numerator) / edit_rate.denominator);
}


shared_ptr<const AudioBuffers>
SoundMXFContent::head_audio(shared_ptr<const Film> film) const
{
	auto const frames = head_frames();
	if (frames == 0 || audio->stream()->frame_rate() != film->audio_frame_rate()) {
		return {};
	}
	return frames_audio(film, 0, frames);
}


shared_ptr<const AudioBuffers>
SoundMXFContent::tail_audio(shared_ptr<const Film> film) const
{
	dcp::Fraction edit_rate;
	{
		boost::mutex::scoped_lock lm(_mutex);
		edit_rate = _edit_rate;
	}
	auto const frames = trim_end().frames_round(static_cast<double>(edit_rate.numerator) / edit_rate.denominator);
	if (frames == 0 || audio->stream()->frame_rate() != film->audio_frame_rate()) {
		return {};
	}
	dcp::SoundAsset asset(path(0));
	return frames_audio(film, asset.intrinsic_duration() - frames, asset.intrinsic_duration());
}


shared_ptr<const AudioBuffers>
SoundMXFContent::frames_audio(shared_ptr<const Film> film, Frame from, Frame to) const
{
	dcp::SoundAsset asset(path(0));
	auto reader = asset.start_read();
	reader->set_check_hmac(false);

	auto const mapping = audio->stream()->mapping();
	auto result = make_shared<AudioBuffers>(film->audio_channels(), 0);
	for (auto frame = from; frame < to; ++frame) {
		result->append(remap(sound_mxf_frame_audio(reader->get_frame(frame), asset.channels()), film->audio_channels(), mapping));
	}

	/* A change is made on the whole of the file, so the gain goes on these frames too */
	result->apply_gain(audio->gain());
	return result;
}


void
SoundMXFContent::add_properties(shared_ptr<const Film> film, list<UserProperty>& p) const
{
	Content::add_properties(film, p);
	audio->add_properties(film, p);
}
