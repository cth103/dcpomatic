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

#ifndef DCPOMATIC_DCP_CONTENT_H
#define DCPOMATIC_DCP_CONTENT_H

/** @file  src/lib/dcp_content.h
 *  @brief DCPContent class.
 */

#include "content.h"
#include <libcxml/cxml.h>
#include <dcp/encrypted_kdm.h>

class DCPContentProperty
{
public:
	static int const NEEDS_KDM;
	static int const NEEDS_ASSETS;
	static int const REFERENCE_VIDEO;
	static int const REFERENCE_AUDIO;
	static int const REFERENCE_CAPTION;
	static int const NAME;
	static int const CAPTIONS;
};

class ContentPart;

/** @class DCPContent
 *  @brief An existing DCP used as input.
 */
class DCPContent : public Content
{
public:
	DCPContent (boost::shared_ptr<const Film>, boost::filesystem::path p);
	DCPContent (boost::shared_ptr<const Film>, cxml::ConstNodePtr, int version);

	boost::shared_ptr<DCPContent> shared_from_this () {
		return boost::dynamic_pointer_cast<DCPContent> (Content::shared_from_this ());
	}

	boost::shared_ptr<const DCPContent> shared_from_this () const {
		return boost::dynamic_pointer_cast<const DCPContent> (Content::shared_from_this ());
	}

	DCPTime full_length () const;

	void examine (boost::shared_ptr<Job>);
	std::string summary () const;
	std::string technical_summary () const;
	void as_xml (xmlpp::Node *, bool with_paths) const;
	std::string identifier () const;
	void take_settings_from (boost::shared_ptr<const Content> c);

	void set_default_colour_conversion ();
	std::list<DCPTime> reel_split_points () const;

	std::vector<boost::filesystem::path> directories () const;

	bool encrypted () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _encrypted;
	}

	void add_kdm (dcp::EncryptedKDM);
	void add_ov (boost::filesystem::path ov);

	boost::optional<dcp::EncryptedKDM> kdm () const {
		return _kdm;
	}

	bool can_be_played () const;
	bool needs_kdm () const;
	bool needs_assets () const;

	void set_reference_video (bool r);

	bool reference_video () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _reference_video;
	}

	bool can_reference_video (std::string &) const;

	void set_reference_audio (bool r);

	bool reference_audio () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _reference_audio;
	}

	bool can_reference_audio (std::string &) const;

	void set_reference_caption (TextType type, bool r);

	/** @param type Original type of captions in the DCP.
	 *  @return true if these captions are to be referenced.
	 */
	bool reference_caption (TextType type) const {
		boost::mutex::scoped_lock lm (_mutex);
		return _reference_caption[type];
	}

	bool can_reference_caption (TextType type, std::string &) const;

	void set_cpl (std::string id);

	boost::optional<std::string> cpl () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _cpl;
	}

	std::string name () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _name;
	}

	bool three_d () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _three_d;
	}

private:
	friend class reels_test5;

	void add_properties (std::list<UserProperty>& p) const;

	void read_directory (boost::filesystem::path);
	std::list<DCPTimePeriod> reels () const;
	bool can_reference (
		boost::function <bool (boost::shared_ptr<const Content>)>,
		std::string overlapping,
		std::string& why_not
		) const;

	std::string _name;
	/** true if our DCP is encrypted */
	bool _encrypted;
	/** true if this DCP needs more assets before it can be played */
	bool _needs_assets;
	boost::optional<dcp::EncryptedKDM> _kdm;
	/** true if _kdm successfully decrypts the first frame of our DCP */
	bool _kdm_valid;
	/** true if the video in this DCP should be included in the output by reference
	 *  rather than by rewrapping.
	 */
	bool _reference_video;
	/** true if the audio in this DCP should be included in the output by reference
	 *  rather than by rewrapping.
	 */
	bool _reference_audio;
	/** true if the captions in this DCP should be included in the output by reference
	 *  rather than by rewrapping.  The types here are the original caption types,
	 *  not what they are being used for.
	 */
	bool _reference_caption[CAPTION_COUNT];

	boost::optional<dcp::Standard> _standard;
	bool _three_d;
	/** ID of the CPL to use; older metadata might not specify this: in that case
	 *  just use the only CPL.
	 */
	boost::optional<std::string> _cpl;
	/** List of the lengths of the reels in this DCP */
	std::list<int64_t> _reel_lengths;
};

#endif
