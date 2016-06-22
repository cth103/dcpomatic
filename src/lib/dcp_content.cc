/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

#include "dcp_content.h"
#include "video_content.h"
#include "audio_content.h"
#include "dcp_examiner.h"
#include "job.h"
#include "film.h"
#include "config.h"
#include "overlaps.h"
#include "compose.hpp"
#include "dcp_decoder.h"
#include "subtitle_content.h"
#include <dcp/dcp.h>
#include <dcp/exceptions.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel.h>
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>
#include <iterator>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using std::distance;
using std::pair;
using std::vector;
using std::list;
using boost::shared_ptr;
using boost::scoped_ptr;
using boost::optional;
using boost::function;

int const DCPContentProperty::CAN_BE_PLAYED      = 600;
int const DCPContentProperty::REFERENCE_VIDEO    = 601;
int const DCPContentProperty::REFERENCE_AUDIO    = 602;
int const DCPContentProperty::REFERENCE_SUBTITLE = 603;

DCPContent::DCPContent (shared_ptr<const Film> film, boost::filesystem::path p)
	: Content (film)
	, _encrypted (false)
	, _kdm_valid (false)
	, _reference_video (false)
	, _reference_audio (false)
	, _reference_subtitle (false)
	, _three_d (false)
{
	video.reset (new VideoContent (this));
	audio.reset (new AudioContent (this));

	read_directory (p);
	set_default_colour_conversion ();
}

DCPContent::DCPContent (shared_ptr<const Film> film, cxml::ConstNodePtr node, int version)
	: Content (film, node)
{
	video = VideoContent::from_xml (this, node, version);
	audio = AudioContent::from_xml (this, node, version);
	subtitle = SubtitleContent::from_xml (this, node, version);

	audio->set_stream (
		AudioStreamPtr (
			new AudioStream (
				node->number_child<int> ("AudioFrameRate"),
				node->number_child<Frame> ("AudioLength"),
				AudioMapping (node->node_child ("AudioMapping"), version)
				)
			)
		);

	_name = node->string_child ("Name");

	_encrypted = node->bool_child ("Encrypted");
	if (node->optional_node_child ("KDM")) {
		_kdm = dcp::EncryptedKDM (node->string_child ("KDM"));
	}
	_kdm_valid = node->bool_child ("KDMValid");
	_reference_video = node->optional_bool_child ("ReferenceVideo").get_value_or (false);
	_reference_audio = node->optional_bool_child ("ReferenceAudio").get_value_or (false);
	_reference_subtitle = node->optional_bool_child ("ReferenceSubtitle").get_value_or (false);
	if (node->optional_string_child("Standard")) {
		string const s = node->optional_string_child("Standard").get();
		if (s == "Interop") {
			_standard = dcp::INTEROP;
		} else if (s == "SMPTE") {
			_standard = dcp::SMPTE;
		} else {
			DCPOMATIC_ASSERT (false);
		}
	}
	_three_d = node->optional_bool_child("ThreeD").get_value_or (false);
}

void
DCPContent::read_directory (boost::filesystem::path p)
{
	for (boost::filesystem::directory_iterator i(p); i != boost::filesystem::directory_iterator(); ++i) {
		if (boost::filesystem::is_regular_file (i->path ())) {
			_paths.push_back (i->path ());
		} else if (boost::filesystem::is_directory (i->path ())) {
			read_directory (i->path ());
		}
	}
}

void
DCPContent::examine (shared_ptr<Job> job)
{
	bool const could_be_played = can_be_played ();

	job->set_progress_unknown ();
	Content::examine (job);

	shared_ptr<DCPExaminer> examiner (new DCPExaminer (shared_from_this ()));
	video->take_from_examiner (examiner);
	set_default_colour_conversion ();

	{
		boost::mutex::scoped_lock lm (_mutex);

		AudioStreamPtr as (new AudioStream (examiner->audio_frame_rate(), examiner->audio_length(), examiner->audio_channels()));
		audio->set_stream (as);
		AudioMapping m = as->mapping ();
		film()->make_audio_mapping_default (m);
		as->set_mapping (m);
	}

	signal_changed (AudioContentProperty::STREAMS);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_name = examiner->name ();
		if (examiner->has_subtitles ()) {
			subtitle.reset (new SubtitleContent (this));
		}
		_encrypted = examiner->encrypted ();
		_kdm_valid = examiner->kdm_valid ();
		_standard = examiner->standard ();
		_three_d = examiner->three_d ();
	}

	if (could_be_played != can_be_played ()) {
		signal_changed (DCPContentProperty::CAN_BE_PLAYED);
	}

	video->set_frame_type (_three_d ? VIDEO_FRAME_TYPE_3D : VIDEO_FRAME_TYPE_2D);
}

string
DCPContent::summary () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return String::compose (_("%1 [DCP]"), _name);
}

string
DCPContent::technical_summary () const
{
	return Content::technical_summary() + " - "
		+ video->technical_summary() + " - "
		+ audio->technical_summary() + " - ";
}

void
DCPContent::as_xml (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text ("DCP");

	Content::as_xml (node);

	if (video) {
		video->as_xml (node);
	}

	if (audio) {
		audio->as_xml (node);
		node->add_child("AudioFrameRate")->add_child_text (raw_convert<string> (audio->stream()->frame_rate()));
		node->add_child("AudioLength")->add_child_text (raw_convert<string> (audio->stream()->length()));
		audio->stream()->mapping().as_xml (node->add_child("AudioMapping"));
	}

	if (subtitle) {
		subtitle->as_xml (node);
	}

	boost::mutex::scoped_lock lm (_mutex);
	node->add_child("Name")->add_child_text (_name);
	node->add_child("Encrypted")->add_child_text (_encrypted ? "1" : "0");
	if (_kdm) {
		node->add_child("KDM")->add_child_text (_kdm->as_xml ());
	}
	node->add_child("KDMValid")->add_child_text (_kdm_valid ? "1" : "0");
	node->add_child("ReferenceVideo")->add_child_text (_reference_video ? "1" : "0");
	node->add_child("ReferenceAudio")->add_child_text (_reference_audio ? "1" : "0");
	node->add_child("ReferenceSubtitle")->add_child_text (_reference_subtitle ? "1" : "0");
	if (_standard) {
		switch (_standard.get ()) {
		case dcp::INTEROP:
			node->add_child("Standard")->add_child_text ("Interop");
			break;
		case dcp::SMPTE:
			node->add_child("Standard")->add_child_text ("SMPTE");
			break;
		default:
			DCPOMATIC_ASSERT (false);
		}
	}
	node->add_child("ThreeD")->add_child_text (_three_d ? "1" : "0");
}

DCPTime
DCPContent::full_length () const
{
	FrameRateChange const frc (active_video_frame_rate (), film()->video_frame_rate ());
	return DCPTime::from_frames (llrint (video->length () * frc.factor ()), film()->video_frame_rate ());
}

string
DCPContent::identifier () const
{
	SafeStringStream s;
	s << Content::identifier() << "_" << video->identifier() << "_";
	if (subtitle) {
		s << subtitle->identifier () << " ";
	}
	s << (_reference_video ? "1" : "0")
	  << (_reference_subtitle ? "1" : "0");
	return s.str ();
}

void
DCPContent::add_kdm (dcp::EncryptedKDM k)
{
	_kdm = k;
}

bool
DCPContent::can_be_played () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return !_encrypted || _kdm_valid;
}

boost::filesystem::path
DCPContent::directory () const
{
	optional<size_t> smallest;
	boost::filesystem::path dir;
	for (size_t i = 0; i < number_of_paths(); ++i) {
		boost::filesystem::path const p = path (i).parent_path ();
		size_t const d = distance (p.begin(), p.end());
		if (!smallest || d < smallest.get ()) {
			dir = p;
		}
	}

	return dir;
}

void
DCPContent::add_properties (list<UserProperty>& p) const
{
	Content::add_properties (p);
	video->add_properties (p);
	audio->add_properties (p);
}

void
DCPContent::set_default_colour_conversion ()
{
	/* Default to no colour conversion for DCPs */
	video->unset_colour_conversion ();
}

void
DCPContent::set_reference_video (bool r)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_reference_video = r;
	}

	signal_changed (DCPContentProperty::REFERENCE_VIDEO);
}

void
DCPContent::set_reference_audio (bool r)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_reference_audio = r;
	}

	signal_changed (DCPContentProperty::REFERENCE_AUDIO);
}

void
DCPContent::set_reference_subtitle (bool r)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_reference_subtitle = r;
	}

	signal_changed (DCPContentProperty::REFERENCE_SUBTITLE);
}

list<DCPTimePeriod>
DCPContent::reels () const
{
	list<DCPTimePeriod> p;
	scoped_ptr<DCPDecoder> decoder;
	try {
		decoder.reset (new DCPDecoder (shared_from_this(), film()->log()));
	} catch (...) {
		/* Could not load the DCP; guess reels */
		list<DCPTimePeriod> p;
		p.push_back (DCPTimePeriod (position(), end()));
		return p;
	}

	DCPTime from = position ();
	BOOST_FOREACH (shared_ptr<dcp::Reel> i, decoder->reels()) {
		DCPTime const to = from + DCPTime::from_frames (i->main_picture()->duration(), film()->video_frame_rate());
		p.push_back (DCPTimePeriod (from, to));
		from = to;
	}

	return p;
}

list<DCPTime>
DCPContent::reel_split_points () const
{
	list<DCPTime> s;
	BOOST_FOREACH (DCPTimePeriod i, reels()) {
		s.push_back (i.from);
	}
	return s;
}

bool
DCPContent::can_reference (function<shared_ptr<ContentPart> (shared_ptr<const Content>)> part, string overlapping, list<string>& why_not) const
{
	/* We must be using the same standard as the film */
	if (_standard) {
		if (_standard.get() == dcp::INTEROP && !film()->interop()) {
			why_not.push_back (_("The film is set to SMPTE and this DCP is Interop."));
			return false;
		} else if (_standard.get() == dcp::SMPTE && film()->interop()) {
			why_not.push_back (_("The film is set to Interop and this DCP is SMPTE."));
			return false;
		}
	}

	list<DCPTimePeriod> const fr = film()->reels ();
	/* fr must contain reels().  It can also contain other reels, but it must at
	   least contain reels().
	*/
	BOOST_FOREACH (DCPTimePeriod i, reels()) {
		if (find (fr.begin(), fr.end(), i) == fr.end ()) {
			why_not.push_back (_("Reel lengths in the film differ from those in the DCP; set the reel mode to 'split by video content'."));
			return false;
		}
	}

	ContentList a = overlaps (film()->content(), part, position(), end());
	if (a.size() != 1 || a.front().get() != this) {
		why_not.push_back (overlapping);
		return false;
	}

	return true;
}

bool
DCPContent::can_reference_video (list<string>& why_not) const
{
	return can_reference (bind (&Content::video, _1), _("There is other video content overlapping this DCP; remove it."), why_not);
}

bool
DCPContent::can_reference_audio (list<string>& why_not) const
{
        DCPDecoder decoder (shared_from_this(), film()->log());
        BOOST_FOREACH (shared_ptr<dcp::Reel> i, decoder.reels()) {
                if (!i->main_sound()) {
                        why_not.push_back (_("The DCP does not have sound in all reels."));
                        return false;
                }
        }

        return can_reference (bind (&Content::audio, _1),   _("There is other audio content overlapping this DCP; remove it."), why_not);
}

bool
DCPContent::can_reference_subtitle (list<string>& why_not) const
{
        DCPDecoder decoder (shared_from_this(), film()->log());
        BOOST_FOREACH (shared_ptr<dcp::Reel> i, decoder.reels()) {
                if (!i->main_subtitle()) {
                        why_not.push_back (_("The DCP does not have subtitles in all reels."));
                        return false;
                }
        }

        return can_reference (bind (&Content::subtitle, _1), _("There is other subtitle content overlapping this DCP; remove it."), why_not);
}
