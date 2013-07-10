/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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
#include "subtitle_content.h"

using std::string;
using boost::shared_ptr;
using boost::lexical_cast;

int const SubtitleContentProperty::SUBTITLE_OFFSET = 500;
int const SubtitleContentProperty::SUBTITLE_SCALE = 501;

SubtitleContent::SubtitleContent (shared_ptr<const Film> f, boost::filesystem::path p)
	: Content (f, p)
	, _subtitle_offset (0)
	, _subtitle_scale (1)
{

}

SubtitleContent::SubtitleContent (shared_ptr<const Film> f, shared_ptr<const cxml::Node> node)
	: Content (f, node)
	, _subtitle_offset (0)
	, _subtitle_scale (1)
{
	_subtitle_offset = node->number_child<float> ("SubtitleOffset");
	_subtitle_scale = node->number_child<float> ("SubtitleScale");
}

void
SubtitleContent::as_xml (xmlpp::Node* root) const
{
	root->add_child("SubtitleOffset")->add_child_text (lexical_cast<string> (_subtitle_offset));
	root->add_child("SubtitleScale")->add_child_text (lexical_cast<string> (_subtitle_scale));
}

void
SubtitleContent::set_subtitle_offset (int o)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_subtitle_offset = o;
	}
	signal_changed (SubtitleContentProperty::SUBTITLE_OFFSET);
}

void
SubtitleContent::set_subtitle_scale (float s)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_subtitle_scale = s;
	}
	signal_changed (SubtitleContentProperty::SUBTITLE_SCALE);
}
