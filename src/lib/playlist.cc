/* -*- c-basic-offset: 8; default-tab-width: 8; -*- */

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
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include "playlist.h"
#include "sndfile_content.h"
#include "sndfile_decoder.h"
#include "video_content.h"
#include "ffmpeg_decoder.h"
#include "ffmpeg_content.h"
#include "imagemagick_decoder.h"
#include "imagemagick_content.h"
#include "job.h"
#include "config.h"
#include "util.h"

#include "i18n.h"

using std::list;
using std::cout;
using std::vector;
using std::min;
using std::max;
using std::string;
using std::stringstream;
using boost::optional;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::lexical_cast;

Playlist::Playlist ()
	: _loop (1)
{

}

Playlist::Playlist (shared_ptr<const Playlist> other)
	: _loop (other->_loop)
{
	for (RegionList::const_iterator i = other->_regions.begin(); i != other->_regions.end(); ++i) {
		_regions.push_back (shared_ptr<Region> (new Region ((*i)->content->clone(), (*i)->time, this)));
	}
}

void
Playlist::content_changed (weak_ptr<Content> c, int p)
{
	ContentChanged (c, p);
}

string
Playlist::audio_digest () const
{
	string t;
	
	for (RegionList::const_iterator i = _regions.begin(); i != _regions.end(); ++i) {
		if (!dynamic_pointer_cast<const AudioContent> ((*i)->content)) {
			continue;
		}
		
		t += (*i)->content->digest ();

		shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> ((*i)->content);
		if (fc) {
			t += lexical_cast<string> (fc->audio_stream()->id);
		}
	}

	t += lexical_cast<string> (_loop);

	return md5_digest (t.c_str(), t.length());
}

string
Playlist::video_digest () const
{
	string t;
	
	for (RegionList::const_iterator i = _regions.begin(); i != _regions.end(); ++i) {
		if (!dynamic_pointer_cast<const VideoContent> ((*i)->content)) {
			continue;
		}
		
		t += (*i)->content->digest ();
		shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> ((*i)->content);
		if (fc && fc->subtitle_stream()) {
			t += fc->subtitle_stream()->id;
		}
	}

	t += lexical_cast<string> (_loop);

	return md5_digest (t.c_str(), t.length());
}

void
Playlist::set_from_xml (shared_ptr<const cxml::Node> node)
{
	list<shared_ptr<cxml::Node> > c = node->node_children ("Region");
	for (list<shared_ptr<cxml::Node> >::iterator i = c.begin(); i != c.end(); ++i) {
		_regions.push_back (shared_ptr<Region> (new Region (*i, this)));
	}

	_loop = node->number_child<int> ("Loop");
}

void
Playlist::as_xml (xmlpp::Node* node)
{
	for (RegionList::iterator i = _regions.begin(); i != _regions.end(); ++i) {
		(*i)->as_xml (node->add_child ("Region"));
	}

	node->add_child("Loop")->add_child_text(lexical_cast<string> (_loop));
}

void
Playlist::add (shared_ptr<Content> c)
{
	_regions.push_back (shared_ptr<Region> (new Region (c, 0, this)));
	Changed ();
}

void
Playlist::remove (shared_ptr<Content> c)
{
	RegionList::iterator i = _regions.begin ();
	while (i != _regions.end() && (*i)->content != c) {
		++i;
	}
	
	if (i != _regions.end ()) {
		_regions.erase (i);
		Changed ();
	}
}

void
Playlist::set_loop (int l)
{
	_loop = l;
	Changed ();
}

bool
Playlist::has_subtitles () const
{
	for (RegionList::const_iterator i = _regions.begin(); i != _regions.end(); ++i) {
		shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> ((*i)->content);
		if (fc && !fc->subtitle_streams().empty()) {
			return true;
		}
	}

	return false;
}

Playlist::Region::Region (shared_ptr<Content> c, Time t, Playlist* p)
	: content (c)
	, time (t)
{
	connection = c->Changed.connect (bind (&Playlist::content_changed, p, _1, _2));
}

Playlist::Region::Region (shared_ptr<const cxml::Node> node, Playlist* p)
{
	shared_ptr<const cxml::Node> content_node = node->node_child ("Content");
	string const type = content_node->string_child ("Type");

	if (type == "FFmpeg") {
		content.reset (new FFmpegContent (content_node));
	} else if (type == "ImageMagick") {
		content.reset (new ImageMagickContent (content_node));
	} else if (type == "Sndfile") {
		content.reset (new SndfileContent (content_node));
	}

	time = node->number_child<Time> ("Time");
	audio_mapping = AudioMapping (node->node_child ("AudioMapping"));
	connection = content->Changed.connect (bind (&Playlist::content_changed, p, _1, _2));
}

void
Playlist::Region::as_xml (xmlpp::Node* node) const
{
	xmlpp::Node* sub = node->add_child ("Content");
	content->as_xml (sub);
	node->add_child ("Time")->add_child_text (lexical_cast<string> (time));
	audio_mapping.as_xml (node->add_child ("AudioMapping"));
}

class FrameRateCandidate
{
public:
	FrameRateCandidate (float source_, int dcp_)
		: source (source_)
		, dcp (dcp_)
	{}

	float source;
	int dcp;
};

int
Playlist::best_dcp_frame_rate () const
{
	list<int> const allowed_dcp_frame_rates = Config::instance()->allowed_dcp_frame_rates ();

	/* Work out what rates we could manage, including those achieved by using skip / repeat. */
	list<FrameRateCandidate> candidates;

	/* Start with the ones without skip / repeat so they will get matched in preference to skipped/repeated ones */
	for (list<int>::const_iterator i = allowed_dcp_frame_rates.begin(); i != allowed_dcp_frame_rates.end(); ++i) {
		candidates.push_back (FrameRateCandidate (*i, *i));
	}

	/* Then the skip/repeat ones */
	for (list<int>::const_iterator i = allowed_dcp_frame_rates.begin(); i != allowed_dcp_frame_rates.end(); ++i) {
		candidates.push_back (FrameRateCandidate (float (*i) / 2, *i));
		candidates.push_back (FrameRateCandidate (float (*i) * 2, *i));
	}

	/* Pick the best one, bailing early if we hit an exact match */
	float error = std::numeric_limits<float>::max ();
	optional<FrameRateCandidate> best;
	list<FrameRateCandidate>::iterator i = candidates.begin();
	while (i != candidates.end()) {

		float this_error = std::numeric_limits<float>::max ();
		for (RegionList::const_iterator j = _regions.begin(); j != _regions.end(); ++j) {
			shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> ((*j)->content);
			if (!vc) {
				continue;
			}

			this_error += fabs (i->source - vc->video_frame_rate ());
		}

		if (this_error < error) {
			error = this_error;
			best = *i;
		}

		++i;
	}

	if (!best) {
		return 24;
	}
	
	return best->dcp;
}

Time
Playlist::length (shared_ptr<const Film> film) const
{
	Time len = 0;
	for (RegionList::const_iterator i = _regions.begin(); i != _regions.end(); ++i) {
		Time const t = (*i)->time + (*i)->content->length (film);
		len = max (len, t);
	}

	return len;
}
