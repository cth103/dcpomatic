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
#include <fontconfig/fontconfig.h>
#include <libxml++/libxml++.h>
#include <fmt/format.h>
#include <iostream>


#include "i18n.h"


using std::cout;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using boost::optional;
using namespace dcpomatic;


StringTextFileContent::StringTextFileContent (boost::filesystem::path path)
	: Content (path)
{
	text.push_back (make_shared<TextContent>(this, TextType::OPEN_SUBTITLE, TextType::UNKNOWN));
}


StringTextFileContent::StringTextFileContent(cxml::ConstNodePtr node, boost::optional<boost::filesystem::path> film_directory, int version, list<string>& notes)
	: Content (node, film_directory)
	, _length (node->number_child<ContentTime::Type>("Length"))
{
	text = TextContent::from_xml (this, node, version, notes);
}


static
std::set<string>
font_names(StringTextFile const& string_text_file)
{
	std::set<string> names;

	for (auto const& subtitle: string_text_file.subtitles()) {
		for (auto const& line: subtitle.lines) {
			for (auto const& block: line.blocks) {
				names.insert(block.font.get_value_or(""));
			}
		}
	}

	return names;
}


void
StringTextFileContent::examine(shared_ptr<Job> job, bool tolerant)
{
	Content::examine(job, tolerant);
	StringTextFile file (shared_from_this());

	only_text()->clear_fonts();

	/* Default to turning these subtitles on */
	only_text()->set_use (true);

	for (auto name: font_names(file)) {
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
StringTextFileContent::as_xml(xmlpp::Element* element, bool with_paths, PathBehaviour path_behaviour, optional<boost::filesystem::path> film_directory) const
{
	cxml::add_text_child(element, "Type", "TextSubtitle");
	Content::as_xml(element, with_paths, path_behaviour, film_directory);

	if (only_text()) {
		only_text()->as_xml(element);
	}

	cxml::add_text_child(element, "Length", fmt::to_string(_length.get()));
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


/** In 5a820bb8fae34591be5ac6d19a73461b9dab532a there were some changes to subtitle font management.
 *
 *  With StringTextFileContent we used to write a <Font> tag to the metadata with the id "font".  Users
 *  could then set a font file that content should use, and (with some luck) it would end up in the DCP
 *  that way.
 *
 *  After the changes we write a <Font> tag for every different font "id" (i.e. name) found in the source
 *  file (including a <Font> with id "" in the .srt case where there are no font names).
 *
 *  However, this meant that making DCPs from old projects would fail, as the new code would see a font name
 *  in the source, then lookup a Font object for it from the Content, and fail in doing so (since the content
 *  only contains a font called "font").
 *
 *  To put it another way: after the changes, the code expects that any font ID (i.e. name) used in some content
 *  will have a <Font> in the metadata and so a Font object in the TextContent.  Without that, making DCPs fails.
 *
 *  To work around this problem, this check_font_ids() is called for all subtitle content written by DoM versions
 *  before 2.16.14.  We find all the font IDs in the content and map them all to the "legacy" font name (if there
 *  is one).  This is more-or-less a re-examine()-ation, except that we try to preserve any settings that
 *  the user had previously set up.
 *
 *  See #2271.
 */
void
StringTextFileContent::check_font_ids()
{
	StringTextFile file (shared_from_this());
	auto names = font_names(file);

	auto content = only_text();
	optional<boost::filesystem::path> legacy_font_file;
	if (auto legacy_font = content->get_font("font")) {
		legacy_font_file = legacy_font->file();
	}

	for (auto name: names) {
		if (!content->get_font(name)) {
			if (legacy_font_file) {
				content->add_font(make_shared<Font>(name, *legacy_font_file));
			} else {
				content->add_font(make_shared<Font>(name));
			}
		}
	}
}

