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


#include "dcpomatic_assert.h"
#include "dcpomatic_log.h"
#include "font_config.h"
#include <fontconfig/fontconfig.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>


using std::string;
using boost::optional;


FontConfig* FontConfig::_instance;


FontConfig::FontConfig()
{
	_config = FcInitLoadConfigAndFonts();
	FcConfigSetCurrent(_config);
}


string
FontConfig::make_font_available(boost::filesystem::path font_file)
{
	auto existing = _available_fonts.find(font_file);
	if (existing != _available_fonts.end()) {
		return existing->second;
	}

	/* Make this font available to DCP-o-matic */
	optional<string> font_name;
	FcConfigAppFontAddFile (_config, reinterpret_cast<FcChar8 const *>(font_file.string().c_str()));
	auto pattern = FcPatternBuild (
		0, FC_FILE, FcTypeString, font_file.string().c_str(), static_cast<char *>(0)
		);
	auto object_set = FcObjectSetBuild (FC_FAMILY, FC_STYLE, FC_LANG, FC_FILE, static_cast<char *> (0));
	auto font_set = FcFontList (_config, pattern, object_set);
	if (font_set) {
		for (int i = 0; i < font_set->nfont; ++i) {
			FcPattern* font = font_set->fonts[i];
			FcChar8* file;
			FcChar8* family;
			FcChar8* style;
			if (
				FcPatternGetString (font, FC_FILE, 0, &file) == FcResultMatch &&
				FcPatternGetString (font, FC_FAMILY, 0, &family) == FcResultMatch &&
				FcPatternGetString (font, FC_STYLE, 0, &style) == FcResultMatch
				) {
				font_name = reinterpret_cast<char const *> (family);
			}
		}

		FcFontSetDestroy (font_set);
	}

	FcObjectSetDestroy (object_set);
	FcPatternDestroy (pattern);

	DCPOMATIC_ASSERT(font_name);

	_available_fonts[font_file] = *font_name;

	FcConfigBuildFonts(_config);
	return *font_name;
}


optional<boost::filesystem::path>
FontConfig::system_font_with_name(string name)
{
	optional<boost::filesystem::path> path;

	LOG_GENERAL("Searching system for font %1", name);
	auto pattern = FcNameParse(reinterpret_cast<FcChar8 const*>(name.c_str()));
	auto object_set = FcObjectSetBuild(FC_FILE, nullptr);
	auto font_set = FcFontList(_config, pattern, object_set);
	if (font_set) {
		LOG_GENERAL("%1 candidate fonts found", font_set->nfont);
		for (int i = 0; i < font_set->nfont; ++i) {
			auto font = font_set->fonts[i];
			FcChar8* file;
			if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch) {
				path = boost::filesystem::path(reinterpret_cast<char*>(file));
				LOG_GENERAL("Found %1", *path);
				break;
			}
		}
		FcFontSetDestroy(font_set);
	} else {
		LOG_GENERAL_NC("No candidate fonts found");
	}

	FcObjectSetDestroy(object_set);
	FcPatternDestroy(pattern);

	if (path) {
		LOG_GENERAL("Searched system for font %1, found %2", name, *path);
	} else {
		LOG_GENERAL("Searched system for font %1; nothing found", name);
	}

	return path;
}


FontConfig *
FontConfig::instance()
{
	if (!_instance) {
		_instance = new FontConfig();
	}

	return _instance;
}

