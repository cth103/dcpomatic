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


#include "audio_content.h"
#include "audio_mapping.h"
#include "content.h"
#include "content_factory.h"
#include "dcdm_audio_layout.h"
#include "dcdm_import.h"
#include "dcp_text_track.h"
#include "examine_content_job.h"
#include "film.h"
#include "image_content.h"
#include "job_manager.h"
#include "ratio.h"
#include "text_content.h"
#include "util.h"
#include "video_content.h"
#include <dcp/file.h>
#include <dcp/filesystem.h>
#include <dcp/language_tag.h>
#include <libcxml/cxml.h>
#include <fmt/format.h>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <map>
#include <stdexcept>

#include "i18n.h"


using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;
using boost::optional;
using namespace dcpomatic;


static string const info_file = "DCDM_INFO.txt";


bool
DCDMImport::valid(boost::filesystem::path directory)
{
	return dcp::filesystem::is_directory(directory) && dcp::filesystem::is_regular_file(directory / info_file);
}


/** @return the channel that DCDMFilmEncoder writes to a file with this label (e.g. "Lrs"), if any */
static
optional<dcp::Channel>
channel_from_label(string label)
{
	vector<pair<dcp::Channel, string>> labels;
	auto const names = dcdm_channel_labels(DCDMLayout::SEVEN_ONE);
	auto const channels = dcdm_channels(DCDMLayout::SEVEN_ONE);
	for (size_t i = 0; i < names.size(); ++i) {
		labels.push_back({channels[i], names[i]});
	}
	for (auto const& extra: dcdm_extra_channels({dcp::Channel::HI, dcp::Channel::VI, dcp::Channel::SIGN_LANGUAGE})) {
		labels.push_back(extra);
	}

	for (auto const& i: labels) {
		if (i.second == label) {
			return i.first;
		}
	}

	return {};
}


/** @return the files in @param directory, sorted, ignoring hidden files and folders */
static
vector<boost::filesystem::path>
files_in(boost::filesystem::path directory)
{
	vector<boost::filesystem::path> files;
	if (!dcp::filesystem::is_directory(directory)) {
		return files;
	}
	for (auto const& i: dcp::filesystem::directory_iterator(directory)) {
		if (dcp::filesystem::is_regular_file(i.path()) && !boost::starts_with(i.path().filename().string(), ".")) {
			files.push_back(i.path());
		}
	}
	std::sort(files.begin(), files.end());
	return files;
}


DCDMImport::DCDMImport(boost::filesystem::path directory)
{
	if (!valid(directory)) {
		throw std::runtime_error(fmt::format(_("{} is not a DCDM written by DCP-o-matic: it has no {}."), directory.string(), info_file));
	}

	/* The parts of DCDM_INFO.txt that the files do not say themselves */
	std::map<string, string> track_names;
	vector<string> lines;
	auto const info = dcp::file_to_string(directory / info_file);
	boost::split(lines, info, boost::is_any_of("\n"));
	for (auto line: lines) {
		boost::trim(line);
		if (boost::starts_with(line, "Frame rate:")) {
			try {
				_frame_rate = std::stoi(line.substr(11));
			} catch (...) {}
		} else if (boost::starts_with(line, "Stereoscopy:")) {
			_three_d = boost::starts_with(boost::trim_copy(line.substr(12)), "3D");
		} else if (boost::starts_with(line, "Text format:")) {
			_interop = boost::starts_with(boost::trim_copy(line.substr(12)), "Interop");
		} else if (boost::starts_with(line, "Track:")) {
			/* Track: <file name> = <track name> */
			auto const equals = line.find(" = ");
			if (equals != string::npos) {
				track_names[boost::trim_copy(line.substr(6, equals - 6))] = line.substr(equals + 3);
			}
		}
	}

	/* One reel is written in the folder itself, more in reel_01, reel_02 ... */
	vector<boost::filesystem::path> reel_directories;
	for (int i = 1; dcp::filesystem::is_directory(directory / fmt::format("reel_{:02d}", i)); ++i) {
		reel_directories.push_back(directory / fmt::format("reel_{:02d}", i));
	}
	if (reel_directories.empty()) {
		reel_directories.push_back(directory);
	}

	for (auto const& dir: reel_directories) {
		Reel reel;

		/* Lossless JPEG 2000 pictures are in picture/, DPX in the reel folder itself */
		auto picture = dir;
		if (dcp::filesystem::is_directory(dir / "picture")) {
			picture = dir / "picture";
			_j2k = true;
		}
		if (_three_d) {
			for (auto eye: { "left", "right" }) {
				if (!dcp::filesystem::is_directory(picture / eye)) {
					throw std::runtime_error(fmt::format(_("The 3D DCDM reel {} has no {} folder."), dir.string(), eye));
				}
				reel.pictures.push_back(make_shared<ImageContent>(picture / eye));
			}
		} else {
			reel.pictures.push_back(make_shared<ImageContent>(picture));
		}

		for (auto const& wav: files_in(dir / "audio")) {
			if (auto channel = channel_from_label(wav.stem().string())) {
				reel.sound.push_back({content_factory(wav)[0], *channel});
			}
		}

		if (dcp::filesystem::is_regular_file(dir / "atmos.mxf")) {
			reel.atmos = content_factory(dir / "atmos.mxf")[0];
		}

		if (dcp::filesystem::is_regular_file(dir / "subtitles" / "subtitles.xml")) {
			reel.subtitles = content_factory(dir / "subtitles" / "subtitles.xml")[0];
		}

		/* Each caption track is in NN_<track name>.xml, with characters other than letters, digits, - and _
		 * written as _; DCDM_INFO.txt gives the whole name, except in DCDMs written before it did.
		 */
		for (auto const& xml: files_in(dir / "captions")) {
			if (xml.extension() != ".xml") {
				continue;
			}
			string name;
			auto const known = track_names.find(xml.filename().string());
			if (known != track_names.end()) {
				name = known->second;
			} else {
				name = xml.stem().string();
				auto const underscore = name.find('_');
				if (underscore != string::npos) {
					name = name.substr(underscore + 1);
				}
				boost::replace_all(name, "_", " ");
			}
			/* SMPTE and Interop XML both give the language in <Language> */
			optional<dcp::LanguageTag> language;
			try {
				cxml::Document doc;
				doc.read_file(dcp::filesystem::fix_long_path(xml));
				if (auto tag = doc.optional_string_child("Language")) {
					language = dcp::LanguageTag(*tag);
				}
			} catch (...) {
				/* No language that we can use */
			}
			reel.captions.push_back({content_factory(xml)[0], name, language});
		}

		_reels.push_back(reel);
	}
}


vector<shared_ptr<Content>>
DCDMImport::content() const
{
	vector<shared_ptr<Content>> all;
	for (auto const& reel: _reels) {
		for (auto picture: reel.pictures) {
			all.push_back(picture);
		}
		for (auto const& sound: reel.sound) {
			all.push_back(sound.content);
		}
		if (reel.atmos) {
			all.push_back(reel.atmos);
		}
		if (reel.subtitles) {
			all.push_back(reel.subtitles);
		}
		for (auto const& caption: reel.captions) {
			all.push_back(caption.content);
		}
	}
	return all;
}


void
DCDMImport::set_up(shared_ptr<Film> film) const
{
	/* The DCDM says where everything goes, so nothing must be moved to follow on */
	film->set_sequence(false);
	if (_frame_rate) {
		film->set_video_frame_rate(*_frame_rate);
	}
	film->set_three_d(_three_d);
	/* Interop text, as the DCP it came from had; Atmos needs SMPTE, which Film::add_content() has already set */
	if (_interop && !film->contains_atmos_content()) {
		film->set_interop(true);
	}
	/* Lossless JPEG 2000 must be re-encoded, not copied into the DCP as it is */
	film->set_reencode_j2k(_j2k);

	/* Enough channels for every sound file, and an even number of them */
	int channels = film->audio_channels();
	for (auto const& reel: _reels) {
		for (auto const& sound: reel.sound) {
			channels = std::max(channels, static_cast<int>(sound.channel) + 1);
		}
	}
	film->set_audio_channels(channels + (channels % 2));

	if (!_reels.empty() && !_reels.front().pictures.empty()) {
		if (auto size = _reels.front().pictures.front()->video->size()) {
			film->set_container(Ratio::from_id(size->ratio() > 2.3 ? "239" : "185"), false);
			film->set_resolution(size->width > 2048 || size->height > 1080 ? Resolution::FOUR_K : Resolution::TWO_K, false);
		}
	}

	DCPTime start;
	for (auto const& reel: _reels) {
		DCPTime length;
		for (size_t i = 0; i < reel.pictures.size(); ++i) {
			auto picture = reel.pictures[i];
			/* The pictures are X'Y'Z' already */
			picture->video->unset_colour_conversion();
			picture->set_video_frame_rate(film, film->video_frame_rate());
			if (_three_d) {
				picture->video->set_frame_type(i == 0 ? VideoFrameType::THREE_D_LEFT : VideoFrameType::THREE_D_RIGHT);
			}
			picture->set_position(film, start);
			length = std::max(length, picture->length_after_trim(film));
		}

		for (auto const& sound: reel.sound) {
			auto mapping = sound.content->audio->mapping();
			mapping.make_zero();
			mapping.set(0, sound.channel, 1);
			sound.content->audio->set_mapping(mapping);
			sound.content->set_position(film, start);
		}

		if (reel.atmos) {
			reel.atmos->set_position(film, start);
		}

		if (reel.subtitles) {
			auto text = reel.subtitles->only_text();
			text->set_type(TextType::OPEN_SUBTITLE);
			text->set_use(true);
			text->set_burn(false);
			reel.subtitles->set_position(film, start);
		}

		for (auto const& caption: reel.captions) {
			auto text = caption.content->only_text();
			text->set_type(TextType::CLOSED_CAPTION);
			text->set_dcp_track(DCPTextTrack(caption.track, caption.language ? caption.language : text->language()));
			text->set_use(true);
			caption.content->set_position(film, start);
		}

		start += length;
	}

	if (_reels.size() > 1) {
		film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	}
}


void
DCDMImport::examine_and_add(shared_ptr<Film> film) const
{
	auto const all = content();
	auto job = make_shared<ExamineContentJob>(film, all, false);
	weak_ptr<Film> weak_film = film;
	auto const self = *this;
	job->Finished.connect([weak_film, self, all](Job::Result result) {
		auto film = weak_film.lock();
		if (!film || result != Job::Result::RESULT_OK) {
			return;
		}
		film->add_content(all);
		self.set_up(film);
	});
	JobManager::instance()->add(job);
}
