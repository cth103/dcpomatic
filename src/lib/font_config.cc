/*
    Copyright (C) 2014-2023 Carl Hetherington <cth@carlh.net>

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
#include "font.h"
#include "font_config.h"
#include "util.h"
#include <dcp/filesystem.h>
#include <ttf/collection.h>
#include <ttf/font.h>
#include <ttf/name_table.h>
#include <fontconfig/fontconfig.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>


using std::shared_ptr;
using std::string;
using boost::optional;


FontConfig* FontConfig::_instance;


FontConfig::FontConfig()
{
	_config = FcInitLoadConfigAndFonts();
	FcConfigSetCurrent(_config);
}


FontConfig::~FontConfig()
{
	for (auto file: _temp_files) {
		boost::system::error_code ec;
		dcp::filesystem::remove(file, ec);
	}
}


/** Try to make the given Font available, returning a name to use with Pango, or
 *  boost::none if there was some problem.  If Font contains no filename and no
 *  data we will try to load default_font_file().
 */
optional<string>
FontConfig::make_font_available(shared_ptr<dcpomatic::Font> font)
{
	DCPOMATIC_ASSERT(font);

	auto existing = _available_fonts.find(font->content());
	if (existing != _available_fonts.end()) {
		return existing->second;
	}

	optional<ttf::Font> ttf_font;

	try {
		if (font->file()) {
			try {
				ttf_font = ttf::Font(*font->file());
			} catch (ttf::InvalidScalerType const&) {
				/* Perhaps this is actually a TTC */
				auto collection = ttf::Collection(*font->file());
				LOG_GENERAL("TTC found instead of TTF: contains {} fonts", collection.fonts().size());
				if (collection.fonts().empty()) {
					return boost::none;
				}
				ttf_font = collection.fonts()[0];
			}
		} else if (font->data()) {
			try {
				ttf_font = ttf::Font(font->data()->data(), font->data()->size());
			} catch (ttf::InvalidScalerType const&) {
				/* Perhaps this is actually a TTC */
				auto collection = ttf::Collection(font->data()->data(), font->data()->size());
				LOG_GENERAL("TTC found instead of TTF: contains {} fonts", collection.fonts().size());
				if (collection.fonts().empty()) {
					return boost::none;
				}
				ttf_font = collection.fonts()[0];
			}
		} else {
			ttf_font = ttf::Font(default_font_file());
		}
	} catch (std::exception& e) {
		LOG_ERROR("Failed to add font ({})", e.what());
		return boost::none;
	}

	DCPOMATIC_ASSERT(ttf_font->name_table());
	auto const new_name = fmt::format("dcpomatic_custom_{}", _index++);

	/* Rename the font to something that we can definitely get hold of by giving Pango a family */
	ttf_font->name_table()->set_names(ttf::NameTable::Identifier::FONT_FAMILY, new_name, std::wstring(new_name.begin(), new_name.end()));
	ttf_font->name_table()->set_names(ttf::NameTable::Identifier::PREFERRED_FAMILY, new_name, std::wstring(new_name.begin(), new_name.end()));
	auto font_file = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
	_temp_files.push_back(font_file);
	ttf_font->write(font_file);

	/* Make this font available to DCP-o-matic */
	optional<string> font_name;
	auto result = FcConfigAppFontAddFile(_config, reinterpret_cast<FcChar8 const *>(font_file.string().c_str()));
	if (result == FcFalse) {
		if (font->file()) {
			LOG_ERROR("Failed to add font {} for {}", font_file.string(), font->file()->string());
		} else {
			LOG_ERROR("Failed to add font {}", font_file.string());
		}
		return boost::none;
	}
	auto pattern = FcPatternBuild (
		0, FC_FILE, FcTypeString, font_file.string().c_str(), static_cast<char *>(0)
		);
	auto object_set = FcObjectSetBuild (FC_FAMILY, FC_STYLE, FC_LANG, FC_FILE, static_cast<char *> (0));
	if (auto font_set = FcFontList(_config, pattern, object_set)) {
		for (int i = 0; i < font_set->nfont; ++i) {
			FcPattern* font = font_set->fonts[i];
			FcChar8* family;
			if (FcPatternGetString(font, FC_FAMILY, 0, &family) == FcResultMatch) {
				font_name = reinterpret_cast<char const *>(family);
			}
		}

		FcFontSetDestroy (font_set);
	}

	FcObjectSetDestroy (object_set);
	FcPatternDestroy (pattern);

	if (font_name) {
		/* We need to use the font object as the key, as we may be passed the same shared_ptr to a modified
		 * Font object in the future and in that case we need to load the new font.
		 */
		_available_fonts[font->content()] = *font_name;
	} else {
		if (font->file()) {
			LOG_ERROR("Tried to add font {} for {} but it wasn't found in the list", font_file.string(), font->file()->string());
		} else {
			LOG_ERROR("Tried to add font {} but it wasn't found in the list", font_file.string());
		}
	}

	FcConfigBuildFonts(_config);
	return font_name;
}


optional<boost::filesystem::path>
FontConfig::system_font_with_name(string name)
{
	optional<boost::filesystem::path> path;

	LOG_GENERAL("Searching system for font {}", name);
	auto pattern = FcNameParse(reinterpret_cast<FcChar8 const*>(name.c_str()));
	auto object_set = FcObjectSetBuild(FC_FILE, nullptr);
	auto font_set = FcFontList(_config, pattern, object_set);
	if (font_set) {
		LOG_GENERAL("{} candidate fonts found", font_set->nfont);
		for (int i = 0; i < font_set->nfont; ++i) {
			auto font = font_set->fonts[i];
			FcChar8* file;
			if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch) {
				path = boost::filesystem::path(reinterpret_cast<char*>(file));
				LOG_GENERAL("Found {}", path->string());
				break;
			}
		}
		FcFontSetDestroy(font_set);
	} else {
		LOG_GENERAL("No candidate fonts found");
	}

	FcObjectSetDestroy(object_set);
	FcPatternDestroy(pattern);

	if (path) {
		LOG_GENERAL("Searched system for font {}, found {}", name, path->string());
	} else {
		LOG_GENERAL("Searched system for font {}; nothing found", name);
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


void
FontConfig::drop()
{
	delete _instance;
	_instance = nullptr;
}

