/*
    Copyright (C) 2014-2023 Carl Hetherington <cth@carlh.net>

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


#include "atmos_content.h"
#include "audio_content.h"
#include "compose.hpp"
#include "config.h"
#include "dcp_content.h"
#include "dcp_decoder.h"
#include "dcp_examiner.h"
#include "dcpomatic_log.h"
#include "film.h"
#include "job.h"
#include "log.h"
#include "overlaps.h"
#include "scope_guard.h"
#include "text_content.h"
#include "video_content.h"
#include <dcp/dcp.h>
#include <dcp/raw_convert.h>
#include <dcp/exceptions.h>
#include <dcp/reel_closed_caption_asset.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/reel.h>
#include <libxml++/libxml++.h>
#include <iterator>
#include <iostream>

#include "i18n.h"


using std::cout;
using std::dynamic_pointer_cast;
using std::exception;
using std::function;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using boost::scoped_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::raw_convert;
using namespace dcpomatic;


int const DCPContentProperty::NEEDS_ASSETS       = 600;
int const DCPContentProperty::NEEDS_KDM          = 601;
int const DCPContentProperty::REFERENCE_VIDEO    = 602;
int const DCPContentProperty::REFERENCE_AUDIO    = 603;
int const DCPContentProperty::REFERENCE_TEXT     = 604;
int const DCPContentProperty::NAME               = 605;
int const DCPContentProperty::TEXTS              = 606;
int const DCPContentProperty::CPL                = 607;


DCPContent::DCPContent (boost::filesystem::path p)
	: _encrypted (false)
	, _needs_assets (false)
	, _kdm_valid (false)
	, _reference_video (false)
	, _reference_audio (false)
	, _three_d (false)
{
	LOG_GENERAL ("Creating DCP content from %1", p.string());

	read_directory (p);
	set_default_colour_conversion ();
}

DCPContent::DCPContent (cxml::ConstNodePtr node, int version)
	: Content (node)
{
	video = VideoContent::from_xml (this, node, version, VideoRange::FULL);
	audio = AudioContent::from_xml (this, node, version);
	list<string> notes;
	text = TextContent::from_xml (this, node, version, notes);
	atmos = AtmosContent::from_xml (this, node);

	if (video && audio) {
		audio->set_stream (
			make_shared<AudioStream> (
				node->number_child<int> ("AudioFrameRate"),
				/* AudioLength was not present in some old metadata versions */
				node->optional_number_child<Frame>("AudioLength").get_value_or (
					video->length() * node->number_child<int>("AudioFrameRate") / video_frame_rate().get()
					),
				AudioMapping(node->node_child("AudioMapping"), version),
				24
				)
			);
	}

	_name = node->string_child ("Name");
	_encrypted = node->bool_child ("Encrypted");
	_needs_assets = node->optional_bool_child("NeedsAssets").get_value_or (false);
	if (node->optional_node_child ("KDM")) {
		_kdm = dcp::EncryptedKDM (node->string_child ("KDM"));
	}
	_kdm_valid = node->bool_child ("KDMValid");
	_reference_video = node->optional_bool_child ("ReferenceVideo").get_value_or (false);
	_reference_audio = node->optional_bool_child ("ReferenceAudio").get_value_or (false);
	if (version >= 37) {
		_reference_text[TextType::OPEN_SUBTITLE] = node->optional_bool_child("ReferenceOpenSubtitle").get_value_or(false);
		_reference_text[TextType::CLOSED_CAPTION] = node->optional_bool_child("ReferenceClosedCaption").get_value_or(false);
	} else {
		_reference_text[TextType::OPEN_SUBTITLE] = node->optional_bool_child("ReferenceSubtitle").get_value_or(false);
		_reference_text[TextType::CLOSED_CAPTION] = false;
	}
	if (node->optional_string_child("Standard")) {
		auto const s = node->optional_string_child("Standard").get();
		if (s == "Interop") {
			_standard = dcp::Standard::INTEROP;
		} else if (s == "SMPTE") {
			_standard = dcp::Standard::SMPTE;
		} else {
			DCPOMATIC_ASSERT (false);
		}
	}
	_three_d = node->optional_bool_child("ThreeD").get_value_or (false);

	auto ck = node->optional_string_child("ContentKind");
	if (ck) {
		_content_kind = dcp::ContentKind::from_name(*ck);
	}
	_cpl = node->optional_string_child("CPL");
	for (auto i: node->node_children("ReelLength")) {
		_reel_lengths.push_back (raw_convert<int64_t> (i->content ()));
	}

	for (auto i: node->node_children("Marker")) {
		_markers[dcp::marker_from_string(i->string_attribute("type"))] = ContentTime(raw_convert<int64_t>(i->content()));
	}

	for (auto i: node->node_children("Rating")) {
		_ratings.push_back (dcp::Rating(i));
	}

	for (auto i: node->node_children("ContentVersion")) {
		_content_versions.push_back (i->content());
	}

	_active_audio_channels = node->optional_number_child<int>("ActiveAudioChannels");
}

void
DCPContent::read_directory (boost::filesystem::path p)
{
	using namespace boost::filesystem;

	bool have_assetmap = false;
	bool have_metadata = false;

	for (auto i: directory_iterator(p)) {
		if (i.path().filename() == "ASSETMAP" || i.path().filename() == "ASSETMAP.xml") {
			have_assetmap = true;
		} else if (i.path().filename() == "metadata.xml") {
			have_metadata = true;
		}
	}

	if (!have_assetmap) {
		if (!have_metadata) {
			throw DCPError ("No ASSETMAP or ASSETMAP.xml file found: is this a DCP?");
		} else {
			throw ProjectFolderError ();
		}
	}

	read_sub_directory (p);
}

void
DCPContent::read_sub_directory (boost::filesystem::path p)
{
	using namespace boost::filesystem;

	LOG_GENERAL ("DCPContent::read_sub_directory reads %1", p.string());
	try {
		for (auto i: directory_iterator(p)) {
			if (is_regular_file(i.path())) {
				LOG_GENERAL ("Inside there's regular file %1", i.path().string());
				add_path (i.path());
			} else if (is_directory(i.path()) && i.path().filename() != ".AppleDouble") {
				LOG_GENERAL ("Inside there's directory %1", i.path().string());
				read_sub_directory (i.path());
			} else {
				LOG_GENERAL("Ignoring %1 from inside: status is %2", i.path().string(), static_cast<int>(status(i.path()).type()));
			}
		}
	} catch (exception& e) {
		LOG_GENERAL("Failed to iterate over %1: %2", p.string(), e.what());
	}
}

/** @param film Film, or 0 */
void
DCPContent::examine (shared_ptr<const Film> film, shared_ptr<Job> job)
{
	bool const needed_assets = needs_assets ();
	bool const needed_kdm = needs_kdm ();
	string const old_name = name ();

	ContentChangeSignalDespatcher::instance()->suspend();
	ScopeGuard sg = []() {
		ContentChangeSignalDespatcher::instance()->resume();
	};

	ContentChangeSignaller cc_texts (this, DCPContentProperty::TEXTS);
	ContentChangeSignaller cc_assets (this, DCPContentProperty::NEEDS_ASSETS);
	ContentChangeSignaller cc_kdm (this, DCPContentProperty::NEEDS_KDM);
	ContentChangeSignaller cc_name (this, DCPContentProperty::NAME);

	if (job) {
		job->set_progress_unknown ();
	}
	Content::examine (film, job);

	auto examiner = make_shared<DCPExaminer>(shared_from_this(), film ? film->tolerant() : true);

	if (examiner->has_video()) {
		{
			boost::mutex::scoped_lock lm (_mutex);
			video = make_shared<VideoContent>(this);
		}
		video->take_from_examiner(film, examiner);
		set_default_colour_conversion ();
	}

	if (examiner->has_audio()) {
		{
			boost::mutex::scoped_lock lm (_mutex);
			audio = make_shared<AudioContent>(this);
		}
		auto as = make_shared<AudioStream>(examiner->audio_frame_rate(), examiner->audio_length(), examiner->audio_channels(), 24);
		audio->set_stream (as);
		auto m = as->mapping ();
		m.make_default (film ? film->audio_processor() : 0);
		as->set_mapping (m);

		_active_audio_channels = examiner->active_audio_channels();
	}

	if (examiner->has_atmos()) {
		{
			boost::mutex::scoped_lock lm (_mutex);
			atmos = make_shared<AtmosContent>(this);
		}
		/* Setting length will cause calculations to be made based on edit rate, so that must
		 * be set up first otherwise hard-to-spot exceptions will be thrown.
		 */
		atmos->set_edit_rate (examiner->atmos_edit_rate());
		atmos->set_length (examiner->atmos_length());
	}

	vector<shared_ptr<TextContent>> new_text;

	for (int i = 0; i < examiner->text_count(TextType::OPEN_SUBTITLE); ++i) {
		auto c = make_shared<TextContent>(this, TextType::OPEN_SUBTITLE, TextType::OPEN_SUBTITLE);
		c->set_language (examiner->open_subtitle_language());
		add_fonts_from_examiner(c, examiner->fonts());
		new_text.push_back (c);
	}

	for (int i = 0; i < examiner->text_count(TextType::CLOSED_CAPTION); ++i) {
		auto c = make_shared<TextContent>(this, TextType::CLOSED_CAPTION, TextType::CLOSED_CAPTION);
		c->set_dcp_track (examiner->dcp_text_track(i));
		add_fonts_from_examiner(c, examiner->fonts());
		new_text.push_back (c);
	}

	{
		boost::mutex::scoped_lock lm (_mutex);
		text = new_text;
		_name = examiner->name ();
		_encrypted = examiner->encrypted ();
		_needs_assets = examiner->needs_assets ();
		_kdm_valid = examiner->kdm_valid ();
		_standard = examiner->standard ();
		_three_d = examiner->three_d ();
		_content_kind = examiner->content_kind ();
		_cpl = examiner->cpl ();
		_reel_lengths = examiner->reel_lengths ();
		for (auto const& i: examiner->markers()) {
			_markers[i.first] = ContentTime(i.second.as_editable_units_ceil(DCPTime::HZ));
		}
		_ratings = examiner->ratings ();
		_content_versions = examiner->content_versions ();
	}

	if (needed_assets == needs_assets()) {
		cc_assets.abort ();
	}

	if (needed_kdm == needs_kdm()) {
		cc_kdm.abort ();
	}

	if (old_name == name()) {
		cc_name.abort ();
	}

	if (video) {
		video->set_frame_type (_three_d ? VideoFrameType::THREE_D : VideoFrameType::TWO_D);
	}
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
	string s = Content::technical_summary() + " - ";
	if (video) {
		s += video->technical_summary() + " - ";
	}
	if (audio) {
		s += audio->technical_summary() + " - ";
	}
	return s;
}

void
DCPContent::as_xml (xmlpp::Node* node, bool with_paths) const
{
	node->add_child("Type")->add_child_text ("DCP");

	Content::as_xml (node, with_paths);

	if (video) {
		video->as_xml (node);
	}

	if (audio) {
		audio->as_xml (node);
		node->add_child("AudioFrameRate")->add_child_text (raw_convert<string> (audio->stream()->frame_rate()));
		node->add_child("AudioLength")->add_child_text (raw_convert<string> (audio->stream()->length()));
		audio->stream()->mapping().as_xml (node->add_child("AudioMapping"));
	}

	for (auto i: text) {
		i->as_xml (node);
	}

	if (atmos) {
		atmos->as_xml (node);
	}

	boost::mutex::scoped_lock lm (_mutex);
	node->add_child("Name")->add_child_text (_name);
	node->add_child("Encrypted")->add_child_text (_encrypted ? "1" : "0");
	node->add_child("NeedsAssets")->add_child_text (_needs_assets ? "1" : "0");
	if (_kdm) {
		node->add_child("KDM")->add_child_text (_kdm->as_xml ());
	}
	node->add_child("KDMValid")->add_child_text (_kdm_valid ? "1" : "0");
	node->add_child("ReferenceVideo")->add_child_text (_reference_video ? "1" : "0");
	node->add_child("ReferenceAudio")->add_child_text (_reference_audio ? "1" : "0");
	node->add_child("ReferenceOpenSubtitle")->add_child_text(_reference_text[TextType::OPEN_SUBTITLE] ? "1" : "0");
	node->add_child("ReferenceClosedCaption")->add_child_text(_reference_text[TextType::CLOSED_CAPTION] ? "1" : "0");
	if (_standard) {
		switch (_standard.get ()) {
		case dcp::Standard::INTEROP:
			node->add_child("Standard")->add_child_text ("Interop");
			break;
		case dcp::Standard::SMPTE:
			node->add_child("Standard")->add_child_text ("SMPTE");
			break;
		default:
			DCPOMATIC_ASSERT (false);
		}
	}
	node->add_child("ThreeD")->add_child_text (_three_d ? "1" : "0");
	if (_content_kind) {
		node->add_child("ContentKind")->add_child_text(_content_kind->name());
	}
	if (_cpl) {
		node->add_child("CPL")->add_child_text (_cpl.get ());
	}
	for (auto i: _reel_lengths) {
		node->add_child("ReelLength")->add_child_text (raw_convert<string> (i));
	}

	for (auto const& i: _markers) {
		auto marker = node->add_child("Marker");
		marker->set_attribute("type", dcp::marker_to_string(i.first));
		marker->add_child_text(raw_convert<string>(i.second.get()));
	}

	for (auto i: _ratings) {
		auto rating = node->add_child("Rating");
		i.as_xml (rating);
	}

	for (auto i: _content_versions) {
		node->add_child("ContentVersion")->add_child_text(i);
	}

	if (_active_audio_channels) {
		node->add_child("ActiveAudioChannels")->add_child_text(raw_convert<string>(*_active_audio_channels));
	}
}

DCPTime
DCPContent::full_length (shared_ptr<const Film> film) const
{
	if (!video) {
		return {};
	}
	FrameRateChange const frc (film, shared_from_this());
	return DCPTime::from_frames (llrint(video->length() * frc.factor()), film->video_frame_rate());
}

DCPTime
DCPContent::approximate_length () const
{
	if (!video) {
		return {};
	}
	return DCPTime::from_frames (video->length(), 24);
}

string
DCPContent::identifier () const
{
	string s = Content::identifier() + "_";

	if (video) {
		s += video->identifier() + "_";
	}

	for (auto i: text) {
		s += i->identifier () + " ";
	}

	s += string (_reference_video ? "1" : "0");
	for (auto text: _reference_text) {
		s += string(text ? "1" : "0");
	}
	return s;
}

void
DCPContent::add_kdm (dcp::EncryptedKDM k)
{
	_kdm = k;
}

void
DCPContent::add_ov (boost::filesystem::path ov)
{
	read_directory (ov);
}

bool
DCPContent::can_be_played () const
{
	return !needs_kdm() && !needs_assets();
}

bool
DCPContent::needs_kdm () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _encrypted && !_kdm_valid;
}

bool
DCPContent::needs_assets () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _needs_assets;
}

vector<boost::filesystem::path>
DCPContent::directories () const
{
	return dcp::DCP::directories_from_files (paths());
}

void
DCPContent::add_properties (shared_ptr<const Film> film, list<UserProperty>& p) const
{
	Content::add_properties (film, p);
	if (video) {
		video->add_properties (p);
	}
	if (audio) {
		audio->add_properties (film, p);
	}
}

void
DCPContent::set_default_colour_conversion ()
{
	/* Default to no colour conversion for DCPs */
	if (video) {
		video->unset_colour_conversion ();
	}
}

void
DCPContent::set_reference_video (bool r)
{
	ContentChangeSignaller cc (this, DCPContentProperty::REFERENCE_VIDEO);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_reference_video = r;
	}
}

void
DCPContent::set_reference_audio (bool r)
{
	ContentChangeSignaller cc (this, DCPContentProperty::REFERENCE_AUDIO);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_reference_audio = r;
	}
}

void
DCPContent::set_reference_text (TextType type, bool r)
{
	ContentChangeSignaller cc (this, DCPContentProperty::REFERENCE_TEXT);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_reference_text[type] = r;
	}
}

list<DCPTimePeriod>
DCPContent::reels (shared_ptr<const Film> film) const
{
	auto reel_lengths = _reel_lengths;
	if (reel_lengths.empty()) {
		/* Old metadata with no reel lengths; get them here instead */
		try {
			scoped_ptr<DCPExaminer> examiner (new DCPExaminer(shared_from_this(), film->tolerant()));
			reel_lengths = examiner->reel_lengths ();
		} catch (...) {
			/* Could not examine the DCP; guess reels */
			reel_lengths.push_back (length_after_trim(film).frames_round(film->video_frame_rate()));
		}
	}

	list<DCPTimePeriod> p;

	/* This content's frame rate must be the same as the output DCP rate, so we can
	   convert `directly' from ContentTime to DCPTime.
	*/

	/* The starting point of this content on the timeline */
	auto pos = position() - DCPTime (trim_start().get());

	for (auto i: reel_lengths) {
		/* This reel runs from `pos' to `to' */
		DCPTime const to = pos + DCPTime::from_frames (i, film->video_frame_rate());
		if (to > position()) {
			p.push_back (DCPTimePeriod(max(position(), pos), min(end(film), to)));
			if (to > end(film)) {
				break;
			}
		}
		pos = to;
	}

	return p;
}

list<DCPTime>
DCPContent::reel_split_points (shared_ptr<const Film> film) const
{
	list<DCPTime> s;
	for (auto i: reels(film)) {
		s.push_back (i.from);
	}
	return s;
}

bool
DCPContent::can_reference (shared_ptr<const Film> film, function<bool (shared_ptr<const Content>)> part, string overlapping, string& why_not) const
{
	/* We must be using the same standard as the film */
	if (_standard) {
		if (_standard.get() == dcp::Standard::INTEROP && !film->interop()) {
			/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
			why_not = _("it is Interop and the film is set to SMPTE.");
			return false;
		} else if (_standard.get() == dcp::Standard::SMPTE && film->interop()) {
			/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
			why_not = _("it is SMPTE and the film is set to Interop.");
			return false;
		}
	}

	/* And the same frame rate */
	if (!video_frame_rate() || (lrint(video_frame_rate().get()) != film->video_frame_rate())) {
		/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
		why_not = _("it has a different frame rate to the film.");
		return false;
	}

	auto const fr = film->reels ();

	list<DCPTimePeriod> reel_list;
	try {
		reel_list = reels (film);
	} catch (dcp::ReadError &) {
		/* We couldn't read the DCP; it's probably missing */
		return false;
	} catch (dcp::KDMDecryptionError &) {
		/* We have an incorrect KDM */
		return false;
	}

	/* fr must contain reels().  It can also contain other reels, but it must at
	   least contain reels().
	*/
	for (auto i: reel_list) {
		if (find (fr.begin(), fr.end(), i) == fr.end ()) {
			/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
			why_not = _("its reel lengths differ from those in the film; set the reel mode to 'split by video content'.");
			return false;
		}
	}

	auto a = overlaps (film, film->content(), part, position(), end(film));
	if (a.size() != 1 || a.front().get() != this) {
		why_not = overlapping;
		return false;
	}

	return true;
}


bool
DCPContent::can_reference_video (shared_ptr<const Film> film, string& why_not) const
{
	if (!video) {
		why_not = _("There is no video in this DCP");
		return false;
	}

	if (film->resolution() != resolution()) {
		if (resolution() == Resolution::FOUR_K) {
			/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
			why_not = _("it is 4K and the film is 2K.");
		} else {
			/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
			why_not = _("it is 2K and the film is 4K.");
		}
		return false;
	} else if (film->frame_size() != video->size()) {
		/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
		why_not = _("its video frame size differs from the film's.");
		return false;
	}

	/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
	return can_reference(
		film,
		[](shared_ptr<const Content> c) {
			return static_cast<bool>(c->video) && c->video->use();
		},
		_("it overlaps other video content; remove the other content."),
		why_not
	);
}


bool
DCPContent::can_reference_audio (shared_ptr<const Film> film, string& why_not) const
{
	shared_ptr<DCPDecoder> decoder;
	try {
		decoder = make_shared<DCPDecoder>(film, shared_from_this(), false, film->tolerant(), shared_ptr<DCPDecoder>());
	} catch (dcp::ReadError &) {
		/* We couldn't read the DCP, so it's probably missing */
		return false;
	} catch (DCPError &) {
		/* We couldn't read the DCP, so it's probably missing */
		return false;
	} catch (dcp::KDMDecryptionError &) {
		/* We have an incorrect KDM */
		return false;
	}

        for (auto i: decoder->reels()) {
                if (!i->main_sound()) {
			/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
                        why_not = _("it does not have sound in all its reels.");
                        return false;
                }
        }

	/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
	return can_reference(
		film, [](shared_ptr<const Content> c) {
			return static_cast<bool>(c->audio) && !c->audio->mapping().mapped_output_channels().empty();
		},
		_("it overlaps other audio content; remove the other content."),
		why_not
	);
}


bool
DCPContent::can_reference_text (shared_ptr<const Film> film, TextType type, string& why_not) const
{
	shared_ptr<DCPDecoder> decoder;
	try {
		decoder = make_shared<DCPDecoder>(film, shared_from_this(), false, film->tolerant(), shared_ptr<DCPDecoder>());
	} catch (dcp::ReadError &) {
		/* We couldn't read the DCP, so it's probably missing */
		return false;
	} catch (dcp::KDMDecryptionError &) {
		/* We have an incorrect KDM */
		return false;
	}

        for (auto i: decoder->reels()) {
                if (type == TextType::OPEN_SUBTITLE) {
			if (!i->main_subtitle()) {
				/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
				why_not = _("it does not have open subtitles in all its reels.");
				return false;
			} else if (i->main_subtitle()->entry_point().get_value_or(0) != 0) {
				/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
				why_not = _("one of its subtitle reels has a non-zero entry point so it must be re-written.");
				return false;
			}
                }
		if (type == TextType::CLOSED_CAPTION) {
			if (i->closed_captions().empty()) {
				/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
				why_not = _("it does not have closed captions in all its reels.");
				return false;
			}
			for (auto j: i->closed_captions()) {
				if (j->entry_point().get_value_or(0) != 0) {
					/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
					why_not = _("one of its closed caption has a non-zero entry point so it must be re-written.");
					return false;
				}
			}
		}
        }

	if (trim_start() != dcpomatic::ContentTime()) {
		/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
		why_not = _("it has a start trim so its subtitles or closed captions must be re-written.");
		return false;
	}

	/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
	return can_reference(
		film,
		[](shared_ptr<const Content> c) {
			return !c->text.empty();
		},
		_("it overlaps other text content; remove the other content."),
		why_not
	);
}

void
DCPContent::take_settings_from (shared_ptr<const Content> c)
{
	auto dc = dynamic_pointer_cast<const DCPContent>(c);
	if (!dc) {
		return;
	}

	_reference_video = dc->_reference_video;
	_reference_audio = dc->_reference_audio;
	_reference_text = dc->_reference_text;
}

void
DCPContent::set_cpl (string id)
{
	ContentChangeSignaller cc (this, DCPContentProperty::CPL);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_cpl = id;
	}
}

bool
DCPContent::kdm_timing_window_valid () const
{
	if (!_kdm) {
		return true;
	}

	dcp::LocalTime now;
	return _kdm->not_valid_before() < now && now < _kdm->not_valid_after();
}


Resolution
DCPContent::resolution () const
{
	if (video->size().width > 2048 || video->size().height > 1080) {
		return Resolution::FOUR_K;
	}

	return Resolution::TWO_K;
}


void
add_fonts_from_examiner(shared_ptr<TextContent> text, vector<vector<shared_ptr<Font>>> const & all_fonts)
{
	int reel_number = 0;
	for (auto reel_fonts: all_fonts) {
		for (auto font: reel_fonts) {
			/* Each reel could have its own font with the same ID, so we disambiguate them here
			 * by prepending the reel number.  We do the same disambiguation when emitting the
			 * subtitles in the DCP decoder.
			 */
			auto font_copy = make_shared<dcpomatic::Font>(*font);
			font_copy->set_id(id_for_font_in_reel(font->id(), reel_number));
			text->add_font(font_copy);
		}
		++reel_number;
	}

}


string
id_for_font_in_reel(string id, int reel)
{
	return String::compose("%1_%2", reel, id);
}


void
DCPContent::check_font_ids()
{
	if (text.empty()) {
		return;
	}

	DCPExaminer examiner(shared_from_this(), true);
	add_fonts_from_examiner(text.front(), examiner.fonts());
}


int
DCPContent::active_audio_channels() const
{
	return _active_audio_channels.get_value_or(
		(audio && audio->stream()) ? audio->stream()->channels() : 0
		);
}

