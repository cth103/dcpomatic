/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "content_properties_dialog.h"
#include "wx_util.h"
#include "lib/raw_convert.h"
#include "lib/content.h"
#include "lib/video_content.h"
#include "lib/audio_content.h"
#include "lib/single_stream_audio_content.h"
#include <boost/algorithm/string.hpp>

using std::string;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

ContentPropertiesDialog::ContentPropertiesDialog (wxWindow* parent, shared_ptr<Content> content)
	: TableDialog (parent, _("Content Properties"), 2, false)
{
	string n = content->path(0).string();
	boost::algorithm::replace_all (n, "&", "&&");
	add_property (_("Filename"), std_to_wx (n));

	shared_ptr<VideoContent> video = dynamic_pointer_cast<VideoContent> (content);
	if (video) {
		add_property (
			_("Video length"),
			std_to_wx (raw_convert<string> (video->video_length ())) + " " + _("video frames")
			);
		add_property (
			_("Video size"),
			std_to_wx (raw_convert<string> (video->video_size().width) + "x" + raw_convert<string> (video->video_size().height))
			);
		add_property (
			_("Video frame rate"),
			std_to_wx (raw_convert<string> (video->video_frame_rate())) + " " + _("frames per second")
			);
	}

	/* XXX: this could be better wrt audio streams */
	
	shared_ptr<SingleStreamAudioContent> single = dynamic_pointer_cast<SingleStreamAudioContent> (content);
	if (single) {
		add_property (
			_("Audio channels"),
			std_to_wx (raw_convert<string> (single->audio_stream()->channels ()))
			);
	}
	
	layout ();
}

void
ContentPropertiesDialog::add_property (wxString k, wxString v)
{
	add (k, true);
	add (new wxStaticText (this, wxID_ANY, v));
}
