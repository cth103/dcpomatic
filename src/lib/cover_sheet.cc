/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "config.h"
#include "cover_sheet.h"
#include "dcp_content_type.h"
#include "film.h"
#include <dcp/file.h>
#include <dcp/filesystem.h>
#include <dcp/locale_convert.h>
#include <boost/algorithm/string.hpp>

#include "i18n.h"


using std::shared_ptr;
using std::string;
using std::vector;


void
dcpomatic::write_cover_sheet(shared_ptr<const Film> film, boost::filesystem::path dcp_directory, boost::filesystem::path output)
{
	dcp::File file(output, "w");
	if (!file) {
		throw OpenFileError(output, file.open_error(), OpenFileError::WRITE);
	}

	auto text = Config::instance()->cover_sheet();
	boost::algorithm::replace_all(text, "$CPL_NAME", film->name());
	auto cpls = film->cpls();
	if (!cpls.empty()) {
		boost::algorithm::replace_all(text, "$CPL_FILENAME", cpls[0].cpl_file.filename().string());
	}
	boost::algorithm::replace_all(text, "$TYPE", film->dcp_content_type()->pretty_name());
	boost::algorithm::replace_all(text, "$CONTAINER", film->container().container_nickname());

	if (auto audio_language = film->audio_language()) {
		boost::algorithm::replace_all(text, "$AUDIO_LANGUAGE", audio_language->description());
	} else {
		boost::algorithm::replace_all(text, "$AUDIO_LANGUAGE", _("None"));
	}

	auto const subtitle_languages = film->open_text_languages();
	if (subtitle_languages.first) {
		boost::algorithm::replace_all(text, "$SUBTITLE_LANGUAGE", subtitle_languages.first->description());
	} else {
		boost::algorithm::replace_all(text, "$SUBTITLE_LANGUAGE", _("None"));
	}

	boost::uintmax_t size = 0;
	for (
		auto i = dcp::filesystem::recursive_directory_iterator(dcp_directory);
		i != dcp::filesystem::recursive_directory_iterator();
		++i) {
		if (dcp::filesystem::is_regular_file(i->path())) {
			size += dcp::filesystem::file_size(i->path());
		}
	}

	if (size > (1000000000L)) {
		boost::algorithm::replace_all(text, "$SIZE", fmt::format("{}GB", dcp::locale_convert<string>(size / 1000000000.0, 1, true)));
	} else {
		boost::algorithm::replace_all(text, "$SIZE", fmt::format("{}MB", dcp::locale_convert<string>(size / 1000000.0, 1, true)));
	}

	auto ch = audio_channel_types(film->mapped_audio_channels(), film->audio_channels());
	auto description = fmt::format("{}.{}", ch.first, ch.second);

	if (description == "0.0") {
		description = _("None");
	} else if (description == "1.0") {
		description = _("Mono");
	} else if (description == "2.0") {
		description = _("Stereo");
	}
	boost::algorithm::replace_all(text, "$AUDIO", description);

	auto const hmsf = film->length().split(film->video_frame_rate());
	string length;
	if (hmsf.h == 0 && hmsf.m == 0) {
		length = fmt::format("{}s", hmsf.s);
	} else if (hmsf.h == 0 && hmsf.m > 0) {
		length = fmt::format("{}m{}s", hmsf.m, hmsf.s);
	} else if (hmsf.h > 0 && hmsf.m > 0) {
		length = fmt::format("{}h{}m{}s", hmsf.h, hmsf.m, hmsf.s);
	}

	boost::algorithm::replace_all(text, "$LENGTH", length);

	auto const markers = film->markers();

	auto marker = [&markers, &text, film](dcp::Marker marker) {
		auto iter = markers.find(marker);
		auto const tag = "$" + marker_to_string(marker);
		auto const tag_line = tag + "_LINE";

		if (iter != markers.end()) {
			auto const timecode = time_to_hmsf(iter->second, film->video_frame_rate());
			boost::algorithm::replace_all(text, tag_line, timecode);
			boost::algorithm::replace_all(text, tag, timecode);
		} else {
			vector<string> before_lines;
			vector<string> after_lines;
			boost::algorithm::split(before_lines, text, boost::is_any_of("\n"));
			if (!before_lines.empty()) {
				before_lines.pop_back();
			}
			for (auto& line: before_lines) {
				if (line.find(tag_line) == std::string::npos) {
					after_lines.push_back(line);
				}
			}
			text.clear();
			for (auto const& line: after_lines) {
				text += line + "\n";
			}

			boost::algorithm::replace_all(text, tag, _("Unknown"));
		}
	};

	marker(dcp::Marker::FFOC);
	marker(dcp::Marker::LFOC);
	marker(dcp::Marker::FFTC);
	marker(dcp::Marker::LFTC);
	marker(dcp::Marker::FFOI);
	marker(dcp::Marker::LFOI);
	marker(dcp::Marker::FFEC);
	marker(dcp::Marker::LFEC);
	marker(dcp::Marker::FFMC);
	marker(dcp::Marker::LFMC);
	marker(dcp::Marker::FFOB);
	marker(dcp::Marker::LFOB);

	file.checked_write(text.c_str(), text.length());
}

