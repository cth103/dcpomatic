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


#include "font_comparator.h"
#include <fontconfig/fontconfig.h>
#include <boost/filesystem.hpp>
#include <map>
#include <string>


/** Wrapper for the fontconfig library */
class FontConfig
{
public:
	static FontConfig* instance();

	std::string make_font_available(std::shared_ptr<dcpomatic::Font> font);
	boost::optional<boost::filesystem::path> system_font_with_name(std::string name);

	static void drop();

private:
	FontConfig();
	~FontConfig();

	FcConfig* _config = nullptr;
	std::map<dcpomatic::Font::Content, std::string, FontComparator> _available_fonts;

	std::vector<boost::filesystem::path> _temp_files;

	static FontConfig* _instance;
};

