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


#include "dcp_subtitle_content.h"
#include "film.h"
#include "font.h"
#include "font_id_allocator.h"
#include "text_content.h"
#include <dcp/interop_load_font_node.h>
#include <dcp/interop_subtitle_asset.h>
#include <dcp/raw_convert.h>
#include <dcp/smpte_subtitle_asset.h>
#include <libxml++/libxml++.h>

#include "i18n.h"


using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using boost::optional;
using dcp::raw_convert;
using namespace dcpomatic;


DCPSubtitleContent::DCPSubtitleContent (boost::filesystem::path path)
	: Content (path)
{
	text.push_back (make_shared<TextContent>(this, TextType::OPEN_SUBTITLE, TextType::OPEN_SUBTITLE));
}

DCPSubtitleContent::DCPSubtitleContent (cxml::ConstNodePtr node, int version)
	: Content (node)
	, _length (node->number_child<ContentTime::Type> ("Length"))
{
	list<string> notes;
	text = TextContent::from_xml (this, node, version, notes);
}

void
DCPSubtitleContent::examine (shared_ptr<const Film> film, shared_ptr<Job> job)
{
	Content::examine (film, job);

	auto subtitle_asset = load(path(0));

	auto iop = dynamic_pointer_cast<dcp::InteropSubtitleAsset>(subtitle_asset);
	auto smpte = dynamic_pointer_cast<dcp::SMPTESubtitleAsset>(subtitle_asset);
	if (smpte) {
		set_video_frame_rate(film, smpte->edit_rate().numerator);
	}

	boost::mutex::scoped_lock lm (_mutex);

	/* Default to turning these subtitles on */
	only_text()->set_use (true);

	_length = ContentTime::from_seconds(subtitle_asset->latest_subtitle_out().as_seconds());

	subtitle_asset->fix_empty_font_ids();
	add_fonts(only_text(), subtitle_asset);
}


void
DCPSubtitleContent::add_fonts(shared_ptr<TextContent> content, shared_ptr<dcp::SubtitleAsset> subtitle_asset)
{
	FontIDAllocator font_id_allocator;

	for (auto node: subtitle_asset->load_font_nodes()) {
		font_id_allocator.add_font(0, subtitle_asset->id(), node->id);
	}
	font_id_allocator.allocate();

	auto font_data = subtitle_asset->font_data();
	for (auto node: subtitle_asset->load_font_nodes()) {
		auto data = font_data.find(node->id);
		shared_ptr<dcpomatic::Font> font;
		if (data != font_data.end()) {
			font = make_shared<Font>(
				font_id_allocator.font_id(0, subtitle_asset->id(), node->id),
				data->second
				);
		} else {
			font = make_shared<Font>(
				font_id_allocator.font_id(0, subtitle_asset->id(), node->id)
				);
		}
		content->add_font(font);
	}

	if (!font_id_allocator.has_default_font()) {
		content->add_font(make_shared<dcpomatic::Font>(font_id_allocator.default_font_id(), default_font_file()));
	}
}


DCPTime
DCPSubtitleContent::full_length (shared_ptr<const Film> film) const
{
	FrameRateChange const frc (film, shared_from_this());
	return DCPTime (_length, frc);
}

DCPTime
DCPSubtitleContent::approximate_length () const
{
	return DCPTime (_length, FrameRateChange());
}

string
DCPSubtitleContent::summary () const
{
	return path_summary() + " " + _("[subtitles]");
}

string
DCPSubtitleContent::technical_summary () const
{
	return Content::technical_summary() + " - " + _("DCP XML subtitles");
}

void
DCPSubtitleContent::as_xml(xmlpp::Element* element, bool with_paths, PathBehaviour path_behaviour, optional<boost::filesystem::path> film_directory) const
{
	cxml::add_text_child(element, "Type", "DCPSubtitle");
	Content::as_xml(element, with_paths, path_behaviour, film_directory);

	if (only_text()) {
		only_text()->as_xml(element);
	}

	cxml::add_text_child(element, "Length", raw_convert<string>(_length.get()));
}
