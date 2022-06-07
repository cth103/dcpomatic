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


#include "film.h"
#include "font.h"
#include "font_config.h"
#include "string_text_file.h"
#include "string_text_file_content.h"
#include "text_content.h"
#include "util.h"
#include <dcp/raw_convert.h>
#include <fontconfig/fontconfig.h>
#include <libxml++/libxml++.h>
#include <iostream>


#include "i18n.h"


using std::cout;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using boost::optional;
using dcp::raw_convert;
using namespace dcpomatic;


StringTextFileContent::StringTextFileContent (boost::filesystem::path path)
	: Content (path)
{
	text.push_back (make_shared<TextContent>(this, TextType::OPEN_SUBTITLE, TextType::UNKNOWN));
}


StringTextFileContent::StringTextFileContent (cxml::ConstNodePtr node, int version, list<string>& notes)
	: Content (node)
	, _length (node->number_child<ContentTime::Type>("Length"))
{
	text = TextContent::from_xml (this, node, version, notes);
}


void
StringTextFileContent::examine (shared_ptr<const Film> film, shared_ptr<Job> job)
{
	Content::examine (film, job);
	StringTextFile file (shared_from_this());

	/* Default to turning these subtitles on */
	only_text()->set_use (true);

	std::set<string> names;
	for (auto const& subtitle: file.subtitles()) {
		for (auto const& line: subtitle.lines) {
			for (auto const& block: line.blocks) {
				names.insert(block.font.get_value_or(""));
			}
		}
	}

	for (auto name: names) {
		optional<boost::filesystem::path> path;
		if (!name.empty()) {
			path = FontConfig::instance()->system_font_with_name(name);
		}
		if (path) {
			only_text()->add_font(make_shared<Font>(name, *path));
		} else {
			only_text()->add_font(make_shared<Font>(name));
		}
	}

	boost::mutex::scoped_lock lm (_mutex);
	_length = file.length();
}


string
StringTextFileContent::summary () const
{
	return path_summary() + " " + _("[subtitles]");
}


string
StringTextFileContent::technical_summary () const
{
	return Content::technical_summary() + " - " + _("Text subtitles");

}


void
StringTextFileContent::as_xml (xmlpp::Node* node, bool with_paths) const
{
	node->add_child("Type")->add_child_text("TextSubtitle");
	Content::as_xml (node, with_paths);

	if (only_text()) {
		only_text()->as_xml(node);
	}

	node->add_child("Length")->add_child_text(raw_convert<string>(_length.get ()));
}


DCPTime
StringTextFileContent::full_length (shared_ptr<const Film> film) const
{
	FrameRateChange const frc (film, shared_from_this());
	return DCPTime (_length, frc);
}


DCPTime
StringTextFileContent::approximate_length () const
{
	return DCPTime (_length, FrameRateChange());
}


string
StringTextFileContent::identifier () const
{
	auto s = Content::identifier ();
	s += "_" + only_text()->identifier();
	return s;
}
