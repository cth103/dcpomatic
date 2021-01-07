/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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
#include <boost/foreach.hpp>

#include "i18n.h"

using std::string;
using std::list;
using std::shared_ptr;
using std::dynamic_pointer_cast;
using dcp::raw_convert;
using namespace dcpomatic;

DCPSubtitleContent::DCPSubtitleContent (boost::filesystem::path path)
	: Content (path)
{
	text.push_back (shared_ptr<TextContent> (new TextContent (this, TEXT_OPEN_SUBTITLE, TEXT_OPEN_SUBTITLE)));
}

DCPSubtitleContent::DCPSubtitleContent (cxml::ConstNodePtr node, int version)
	: Content (node)
	, _length (node->number_child<ContentTime::Type> ("Length"))
{
	text = TextContent::from_xml (this, node, version);
}

void
DCPSubtitleContent::examine (shared_ptr<const Film> film, shared_ptr<Job> job)
{
	Content::examine (film, job);

	shared_ptr<dcp::SubtitleAsset> sc = load (path (0));

	shared_ptr<dcp::InteropSubtitleAsset> iop = dynamic_pointer_cast<dcp::InteropSubtitleAsset> (sc);
	shared_ptr<dcp::SMPTESubtitleAsset> smpte = dynamic_pointer_cast<dcp::SMPTESubtitleAsset> (sc);
	if (smpte) {
		set_video_frame_rate (smpte->edit_rate().numerator);
	}

	boost::mutex::scoped_lock lm (_mutex);

	/* Default to turning these subtitles on */
	only_text()->set_use (true);

	_length = ContentTime::from_seconds (sc->latest_subtitle_out().as_seconds ());

	sc->fix_empty_font_ids ();

	BOOST_FOREACH (shared_ptr<dcp::LoadFontNode> i, sc->load_font_nodes ()) {
		only_text()->add_font (shared_ptr<Font> (new Font (i->id)));
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
