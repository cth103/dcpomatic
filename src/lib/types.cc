/*
    Copyright (C) 2013-2019 Carl Hetherington <cth@carlh.net>

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

#include "types.h"
#include "compose.hpp"
#include "dcpomatic_assert.h"
#include "warnings.h"
#include <dcp/raw_convert.h>
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel_file_asset.h>
#include <dcp/reel_asset.h>
DCPOMATIC_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
DCPOMATIC_ENABLE_WARNINGS
#include <libcxml/cxml.h>

#include "i18n.h"

using std::max;
using std::min;
using std::string;
using std::list;
using std::shared_ptr;
using std::vector;
using dcp::raw_convert;

bool operator== (Crop const & a, Crop const & b)
{
	return (a.left == b.left && a.right == b.right && a.top == b.top && a.bottom == b.bottom);
}

bool operator!= (Crop const & a, Crop const & b)
{
	return !(a == b);
}

/** @param r Resolution.
 *  @return Untranslated string representation.
 */
string
resolution_to_string (Resolution r)
{
	switch (r) {
	case Resolution::TWO_K:
		return "2K";
	case Resolution::FOUR_K:
		return "4K";
	}

	DCPOMATIC_ASSERT (false);
	return "";
}


Resolution
string_to_resolution (string s)
{
	if (s == "2K") {
		return Resolution::TWO_K;
	}

	if (s == "4K") {
		return Resolution::FOUR_K;
	}

	DCPOMATIC_ASSERT (false);
	return Resolution::TWO_K;
}

Crop::Crop (shared_ptr<cxml::Node> node)
{
	left = node->number_child<int> ("LeftCrop");
	right = node->number_child<int> ("RightCrop");
	top = node->number_child<int> ("TopCrop");
	bottom = node->number_child<int> ("BottomCrop");
}

void
Crop::as_xml (xmlpp::Node* node) const
{
	node->add_child("LeftCrop")->add_child_text (raw_convert<string> (left));
	node->add_child("RightCrop")->add_child_text (raw_convert<string> (right));
	node->add_child("TopCrop")->add_child_text (raw_convert<string> (top));
	node->add_child("BottomCrop")->add_child_text (raw_convert<string> (bottom));
}

TextType
string_to_text_type (string s)
{
	if (s == "unknown") {
		return TextType::UNKNOWN;
	} else if (s == "open-subtitle") {
		return TextType::OPEN_SUBTITLE;
	} else if (s == "closed-caption") {
		return TextType::CLOSED_CAPTION;
	} else {
		throw MetadataError (String::compose ("Unknown text type %1", s));
	}
}

string
text_type_to_string (TextType t)
{
	switch (t) {
	case TextType::UNKNOWN:
		return "unknown";
	case TextType::OPEN_SUBTITLE:
		return "open-subtitle";
	case TextType::CLOSED_CAPTION:
		return "closed-caption";
	default:
		DCPOMATIC_ASSERT (false);
	}
}

string
text_type_to_name (TextType t)
{
	switch (t) {
	case TextType::UNKNOWN:
		return _("Timed text");
	case TextType::OPEN_SUBTITLE:
		return _("Open subtitles");
	case TextType::CLOSED_CAPTION:
		return _("Closed captions");
	default:
		DCPOMATIC_ASSERT (false);
	}
}

string
video_frame_type_to_string (VideoFrameType t)
{
	switch (t) {
	case VideoFrameType::TWO_D:
		return "2d";
	case VideoFrameType::THREE_D:
		return "3d";
	case VideoFrameType::THREE_D_LEFT_RIGHT:
		return "3d-left-right";
	case VideoFrameType::THREE_D_TOP_BOTTOM:
		return "3d-top-bottom";
	case VideoFrameType::THREE_D_ALTERNATE:
		return "3d-alternate";
	case VideoFrameType::THREE_D_LEFT:
		return "3d-left";
	case VideoFrameType::THREE_D_RIGHT:
		return "3d-right";
	default:
		DCPOMATIC_ASSERT (false);
	}

	DCPOMATIC_ASSERT (false);
}

VideoFrameType
string_to_video_frame_type (string s)
{
	if (s == "2d") {
		return VideoFrameType::TWO_D;
	} else if (s == "3d") {
		return VideoFrameType::THREE_D;
	} else if (s == "3d-left-right") {
		return VideoFrameType::THREE_D_LEFT_RIGHT;
	} else if (s == "3d-top-bottom") {
		return VideoFrameType::THREE_D_TOP_BOTTOM;
	} else if (s == "3d-alternate") {
		return VideoFrameType::THREE_D_ALTERNATE;
	} else if (s == "3d-left") {
		return VideoFrameType::THREE_D_LEFT;
	} else if (s == "3d-right") {
		return VideoFrameType::THREE_D_RIGHT;
	}

	DCPOMATIC_ASSERT (false);
}

CPLSummary::CPLSummary (boost::filesystem::path p)
	: dcp_directory (p.leaf().string())
{
	dcp::DCP dcp (p);

	vector<dcp::VerificationNote> notes;
	dcp.read (&notes);
	for (auto i: notes) {
		if (i.code() != dcp::VerificationNote::Code::EXTERNAL_ASSET) {
			/* It's not just a warning about this DCP being a VF */
			throw dcp::ReadError(dcp::note_to_string(i));
		}
	}

	cpl_id = dcp.cpls().front()->id();
	cpl_annotation_text = dcp.cpls().front()->annotation_text();
	cpl_file = dcp.cpls().front()->file().get();

	encrypted = false;
	for (auto j: dcp.cpls()) {
		for (auto k: j->reel_file_assets()) {
			if (k->encrypted()) {
				encrypted = true;
			}
		}
	}

	boost::system::error_code ec;
	auto last_write = boost::filesystem::last_write_time (p, ec);
	last_write_time = ec ? 0 : last_write;
}


bool operator== (NamedChannel const& a, NamedChannel const& b)
{
	return a.name == b.name && a.index == b.index;
}


string
video_range_to_string (VideoRange r)
{
	switch (r) {
	case VideoRange::FULL:
		return "full";
	case VideoRange::VIDEO:
		return "video";
	default:
		DCPOMATIC_ASSERT (false);
	}
}


VideoRange
string_to_video_range (string s)
{
	if (s == "full") {
		return VideoRange::FULL;
	} else if (s == "video") {
		return VideoRange::VIDEO;
	}

	DCPOMATIC_ASSERT (false);
	return VideoRange::FULL;
}

