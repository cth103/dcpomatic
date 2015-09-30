/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "font.h"
#include "dcp_subtitle_content.h"
#include "raw_convert.h"
#include "film.h"
#include <dcp/interop_subtitle_asset.h>
#include <dcp/smpte_subtitle_asset.h>
#include <dcp/interop_load_font_node.h>
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>

#include "i18n.h"

using std::string;
using std::list;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

DCPSubtitleContent::DCPSubtitleContent (shared_ptr<const Film> film, boost::filesystem::path path)
	: Content (film, path)
	, SubtitleContent (film, path)
{

}

DCPSubtitleContent::DCPSubtitleContent (shared_ptr<const Film> film, cxml::ConstNodePtr node, int version)
	: Content (film, node)
	, SubtitleContent (film, node, version)
	, _length (node->number_child<ContentTime::Type> ("Length"))
	, _frame_rate (node->optional_number_child<int>("SubtitleFrameRate"))
{

}

void
DCPSubtitleContent::examine (shared_ptr<Job> job)
{
	Content::examine (job);

	shared_ptr<dcp::SubtitleAsset> sc = load (path (0));

	/* Default to turning these subtitles on */
	set_use_subtitles (true);

	boost::mutex::scoped_lock lm (_mutex);

	shared_ptr<dcp::InteropSubtitleAsset> iop = dynamic_pointer_cast<dcp::InteropSubtitleAsset> (sc);
	if (iop) {
		_subtitle_language = iop->language ();
	}
	shared_ptr<dcp::SMPTESubtitleAsset> smpte = dynamic_pointer_cast<dcp::SMPTESubtitleAsset> (sc);
	if (smpte) {
		_subtitle_language = smpte->language().get_value_or ("");
		_frame_rate = smpte->edit_rate().numerator;
	}

	_length = ContentTime::from_seconds (sc->latest_subtitle_out().as_seconds ());

	BOOST_FOREACH (shared_ptr<dcp::LoadFontNode> i, sc->load_font_nodes ()) {
		add_font (shared_ptr<Font> (new Font (i->id)));
	}
}

DCPTime
DCPSubtitleContent::full_length () const
{
	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	FrameRateChange const frc (subtitle_video_frame_rate(), film->video_frame_rate());
	return DCPTime (_length, frc);
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
DCPSubtitleContent::as_xml (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text ("DCPSubtitle");
	Content::as_xml (node);
	SubtitleContent::as_xml (node);
	node->add_child("Length")->add_child_text (raw_convert<string> (_length.get ()));
}

void
DCPSubtitleContent::set_subtitle_video_frame_rate (int r)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_frame_rate = r;
	}

	signal_changed (SubtitleContentProperty::SUBTITLE_VIDEO_FRAME_RATE);
}

double
DCPSubtitleContent::subtitle_video_frame_rate () const
{
	boost::mutex::scoped_lock lm (_mutex);
	if (_frame_rate) {
		return _frame_rate.get ();
	}

	/* No frame rate specified, so assume this content has been
	   prepared for any concurrent video content.
	*/
	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	return film->active_frame_rate_change(position()).source;
}
