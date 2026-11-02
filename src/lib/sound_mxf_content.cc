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
#include "sound_mxf_content.h"
#include "sound_mxf_examiner.h"
#include <asdcp/KM_log.h>
#include <dcp/exceptions.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_reader.h>
#include <dcp/sound_frame.h>
#include <libxml++/libxml++.h>
#include <climits>

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


void
SoundMXFContent::add_properties(shared_ptr<const Film> film, list<UserProperty>& p) const
{
	Content::add_properties(film, p);
	audio->add_properties(film, p);
}
