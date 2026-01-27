/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "fcpxml.h"
#include "fcpxml_content.h"
#include "text_content.h"
#include <fmt/format.h>

#include "i18n.h"


using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using boost::optional;


FCPXMLContent::FCPXMLContent(boost::filesystem::path path)
	: Content(path)
{
	text.push_back(make_shared<TextContent>(this, TextType::OPEN_SUBTITLE, TextType::OPEN_SUBTITLE));
}


FCPXMLContent::FCPXMLContent(cxml::ConstNodePtr node, optional<boost::filesystem::path> film_directory, int version, list<string>& notes)
	: Content(node, film_directory)
{
	text = TextContent::from_xml(this, node, version, notes);

	_length = dcpomatic::ContentTime(node->number_child<dcpomatic::ContentTime::Type>("Length"));
}


void
FCPXMLContent::examine(shared_ptr<Job> job, bool tolerant)
{
	Content::examine( job, tolerant);

	auto sequence = dcpomatic::fcpxml::load(path(0));

	boost::mutex::scoped_lock lm(_mutex);
	only_text()->set_use(true);
	if (!sequence.video.empty()) {
		_length = sequence.video.back().period.to;
	}
}


dcpomatic::DCPTime
FCPXMLContent::full_length(shared_ptr<const Film> film) const
{
	FrameRateChange const frc(film, shared_from_this());
	return { _length, frc };
}


dcpomatic::DCPTime
FCPXMLContent::approximate_length() const
{
	return { _length, {} };
}


string
FCPXMLContent::summary() const
{
	return path_summary() + " " + _("[subtitles]");
}


string
FCPXMLContent::technical_summary() const
{
	return Content::technical_summary() + " - " + _("FCP XML subtitles");
}


void
FCPXMLContent::as_xml(xmlpp::Element* element, bool with_paths, PathBehaviour path_behaviour, optional<boost::filesystem::path> film_directory) const
{
	cxml::add_child(element, "Type", "FCPXML");
	Content::as_xml(element, with_paths, path_behaviour, film_directory);

	if (only_text()) {
		only_text()->as_xml(element);
	}

	cxml::add_child(element, "Length", fmt::to_string(_length.get()));
}

