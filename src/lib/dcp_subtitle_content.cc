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

#include "font.h"
#include "dcp_subtitle_content.h"
#include "film.h"
#include "text_content.h"
#include <dcp/raw_convert.h>
#include <dcp/interop_subtitle_asset.h>
#include <dcp/smpte_subtitle_asset.h>
#include <dcp/interop_load_font_node.h>
#include <libxml++/libxml++.h>

#include "i18n.h"

using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
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

	auto sc = load (path(0));

	auto iop = dynamic_pointer_cast<dcp::InteropSubtitleAsset>(sc);
	auto smpte = dynamic_pointer_cast<dcp::SMPTESubtitleAsset>(sc);
	if (smpte) {
		set_video_frame_rate (smpte->edit_rate().numerator);
	}

	boost::mutex::scoped_lock lm (_mutex);

	/* Default to turning these subtitles on */
	only_text()->set_use (true);

	_length = ContentTime::from_seconds (sc->latest_subtitle_out().as_seconds());

	sc->fix_empty_font_ids ();

	auto font_data = sc->font_data();
	for (auto node: sc->load_font_nodes()) {
		auto data = font_data.find(node->id);
		if (data != font_data.end()) {
			only_text()->add_font(make_shared<Font>(node->id, data->second));
		} else {
			only_text()->add_font(make_shared<Font>(node->id));
		}
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
DCPSubtitleContent::as_xml (xmlpp::Node* node, bool with_paths) const
{
	node->add_child("Type")->add_child_text ("DCPSubtitle");
	Content::as_xml (node, with_paths);

	if (only_text()) {
		only_text()->as_xml (node);
	}

	node->add_child("Length")->add_child_text (raw_convert<string> (_length.get ()));
}
