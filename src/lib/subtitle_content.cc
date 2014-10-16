/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include <libcxml/cxml.h>
#include <libdcp/raw_convert.h>
#include "subtitle_content.h"
#include "util.h"
#include "exceptions.h"

#include "i18n.h"

using std::string;
using std::vector;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using libdcp::raw_convert;

int const SubtitleContentProperty::SUBTITLE_X_OFFSET = 500;
int const SubtitleContentProperty::SUBTITLE_Y_OFFSET = 501;
int const SubtitleContentProperty::SUBTITLE_X_SCALE = 502;
int const SubtitleContentProperty::SUBTITLE_Y_SCALE = 503;

SubtitleContent::SubtitleContent (shared_ptr<const Film> f, boost::filesystem::path p)
	: Content (f, p)
	, _subtitle_x_offset (0)
	, _subtitle_y_offset (0)
	, _subtitle_x_scale (1)
	, _subtitle_y_scale (1)
{

}

SubtitleContent::SubtitleContent (shared_ptr<const Film> f, shared_ptr<const cxml::Node> node, int version)
	: Content (f, node)
	, _subtitle_x_offset (0)
	, _subtitle_y_offset (0)
	, _subtitle_x_scale (1)
	, _subtitle_y_scale (1)
{
	if (version >= 7) {
		_subtitle_x_offset = node->number_child<float> ("SubtitleXOffset");
		_subtitle_y_offset = node->number_child<float> ("SubtitleYOffset");
	} else {
		_subtitle_y_offset = node->number_child<float> ("SubtitleOffset");
	}

	if (version >= 10) {
		_subtitle_x_scale = node->number_child<float> ("SubtitleXScale");
		_subtitle_y_scale = node->number_child<float> ("SubtitleYScale");
	} else {
		_subtitle_x_scale = _subtitle_y_scale = node->number_child<float> ("SubtitleScale");
	}
}

SubtitleContent::SubtitleContent (shared_ptr<const Film> f, vector<shared_ptr<Content> > c)
	: Content (f, c)
{
	shared_ptr<SubtitleContent> ref = dynamic_pointer_cast<SubtitleContent> (c[0]);
	assert (ref);
	
	for (size_t i = 0; i < c.size(); ++i) {
		shared_ptr<SubtitleContent> sc = dynamic_pointer_cast<SubtitleContent> (c[i]);

		if (sc->subtitle_x_offset() != ref->subtitle_x_offset()) {
			throw JoinError (_("Content to be joined must have the same subtitle X offset."));
		}
		
		if (sc->subtitle_y_offset() != ref->subtitle_y_offset()) {
			throw JoinError (_("Content to be joined must have the same subtitle Y offset."));
		}

		if (sc->subtitle_x_scale() != ref->subtitle_x_scale()) {
			throw JoinError (_("Content to be joined must have the same subtitle X scale."));
		}

		if (sc->subtitle_y_scale() != ref->subtitle_y_scale()) {
			throw JoinError (_("Content to be joined must have the same subtitle Y scale."));
		}
	}

	_subtitle_x_offset = ref->subtitle_x_offset ();
	_subtitle_y_offset = ref->subtitle_y_offset ();
	_subtitle_x_scale = ref->subtitle_x_scale ();
	_subtitle_y_scale = ref->subtitle_y_scale ();
}

void
SubtitleContent::as_xml (xmlpp::Node* root) const
{
	root->add_child("SubtitleXOffset")->add_child_text (raw_convert<string> (_subtitle_x_offset));
	root->add_child("SubtitleYOffset")->add_child_text (raw_convert<string> (_subtitle_y_offset));
	root->add_child("SubtitleXScale")->add_child_text (raw_convert<string> (_subtitle_x_scale));
	root->add_child("SubtitleYScale")->add_child_text (raw_convert<string> (_subtitle_y_scale));
}

void
SubtitleContent::set_subtitle_x_offset (double o)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_subtitle_x_offset = o;
	}
	signal_changed (SubtitleContentProperty::SUBTITLE_X_OFFSET);
}

void
SubtitleContent::set_subtitle_y_offset (double o)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_subtitle_y_offset = o;
	}
	signal_changed (SubtitleContentProperty::SUBTITLE_Y_OFFSET);
}

void
SubtitleContent::set_subtitle_x_scale (double s)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_subtitle_x_scale = s;
	}
	signal_changed (SubtitleContentProperty::SUBTITLE_X_SCALE);
}

void
SubtitleContent::set_subtitle_y_scale (double s)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_subtitle_y_scale = s;
	}
	signal_changed (SubtitleContentProperty::SUBTITLE_Y_SCALE);
}
