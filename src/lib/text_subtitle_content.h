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

#include "subtitle_content.h"

class TextSubtitleContentProperty
{
public:
	static int const TEXT_SUBTITLE_COLOUR;
	static int const TEXT_SUBTITLE_OUTLINE;
	static int const TEXT_SUBTITLE_OUTLINE_COLOUR;
};

/** @class TextSubtitleContent
 *  @brief SubRip or SSA subtitles.
 */
class TextSubtitleContent : public SubtitleContent
{
public:
	TextSubtitleContent (boost::shared_ptr<const Film>, boost::filesystem::path);
	TextSubtitleContent (boost::shared_ptr<const Film>, cxml::ConstNodePtr, int);

	boost::shared_ptr<TextSubtitleContent> shared_from_this () {
		return boost::dynamic_pointer_cast<TextSubtitleContent> (Content::shared_from_this ());
	}

	/* Content */
	void examine (boost::shared_ptr<Job>);
	std::string summary () const;
	std::string technical_summary () const;
	void as_xml (xmlpp::Node *) const;
	DCPTime full_length () const;

	/* SubtitleContent */

	bool has_text_subtitles () const {
		return true;
	}

	bool has_image_subtitles () const {
		return false;
	}

	double subtitle_video_frame_rate () const;
	void set_subtitle_video_frame_rate (double r);

	void set_colour (dcp::Colour);

	dcp::Colour colour () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _colour;
	}

	void set_outline (bool);

	bool outline () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _outline;
	}

	void set_outline_colour (dcp::Colour);

	dcp::Colour outline_colour () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _outline_colour;
	}

	static std::string const font_id;

private:
	ContentTime _length;
	/** Video frame rate that this content has been prepared for, if known */
	boost::optional<double> _frame_rate;
	dcp::Colour _colour;
	bool _outline;
	dcp::Colour _outline_colour;
};
