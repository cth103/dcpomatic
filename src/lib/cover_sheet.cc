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

	auto audio_language = film->audio_language();
	if (audio_language) {
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

	file.checked_write(text.c_str(), text.length());
}

