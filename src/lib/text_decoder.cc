/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "compose.hpp"
#include "log.h"
#include "text_content.h"
#include "text_decoder.h"
#include "util.h"
#include <sub/subtitle.h>
#include <boost/algorithm/string.hpp>
#include <iostream>


using std::cout;
using std::max;
using std::min;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using namespace dcpomatic;


TextDecoder::TextDecoder (
	Decoder* parent,
	shared_ptr<const TextContent> content
	)
	: DecoderPart (parent)
	, _content (content)
{

}


/** Called by subclasses when an image subtitle is starting.
 *  @param from From time of the subtitle.
 *  @param image Subtitle image.
 *  @param rect Area expressed as a fraction of the video frame that this subtitle
 *  is for (e.g. a width of 0.5 means the width of the subtitle is half the width
 *  of the video frame)
 */
void
TextDecoder::emit_bitmap_start (ContentBitmapText const& bitmap)
{
	BitmapStart (bitmap);
	maybe_set_position(bitmap.from());
}


static
string
escape_text (string text)
{
	/* We must escape some things, otherwise they might confuse our subtitle
	   renderer (which uses entities and some HTML-esque markup to do bold/italic etc.)
	*/
	boost::algorithm::replace_all(text, "&", "&amp;");
	boost::algorithm::replace_all(text, "<", "&lt;");
	boost::algorithm::replace_all(text, ">", "&gt;");
	return text;
}


static
void
set_forced_appearance(shared_ptr<const TextContent> content, StringText& subtitle)
{
	if (content->colour()) {
		subtitle.set_colour(*content->colour());
	}
	if (content->effect_colour()) {
		subtitle.set_effect_colour(*content->effect_colour());
	}
	if (content->effect()) {
		subtitle.set_effect(*content->effect());
	}
	if (content->fade_in()) {
		subtitle.set_fade_up_time(dcp::Time(content->fade_in()->seconds(), 1000));
	}
	if (content->fade_out()) {
		subtitle.set_fade_down_time (dcp::Time(content->fade_out()->seconds(), 1000));
	}
}


void
TextDecoder::emit_plain_start (ContentTime from, vector<dcp::SubtitleString> subtitles, dcp::Standard valign_standard)
{
	vector<StringText> string_texts;

	for (auto& subtitle: subtitles) {
		auto string_text = StringText(
			subtitle,
			content()->outline_width(),
			subtitle.font() ? content()->get_font(*subtitle.font()) : shared_ptr<Font>(),
			valign_standard
			);
		string_text.set_text(escape_text(string_text.text()));
		set_forced_appearance(content(), string_text);
		string_texts.push_back(string_text);
	}

	PlainStart(ContentStringText(from, string_texts));
	maybe_set_position(from);
}


void
TextDecoder::emit_plain_start (ContentTime from, sub::Subtitle const & sub_subtitle)
{
	/* See if our next subtitle needs to be vertically placed on screen by us */
	bool needs_placement = false;
	optional<int> bottom_line;
	for (auto line: sub_subtitle.lines) {
		if (!line.vertical_position.reference || (line.vertical_position.line && !line.vertical_position.lines) || line.vertical_position.reference.get() == sub::TOP_OF_SUBTITLE) {
			needs_placement = true;
			if (!bottom_line || bottom_line.get() < line.vertical_position.line.get()) {
				bottom_line = line.vertical_position.line.get();
			}
		}
	}

	/* Find the lowest proportional position */
	optional<float> lowest_proportional;
	for (auto line: sub_subtitle.lines) {
		if (line.vertical_position.proportional) {
			if (!lowest_proportional) {
				lowest_proportional = line.vertical_position.proportional;
			} else {
				lowest_proportional = min(lowest_proportional.get(), line.vertical_position.proportional.get());
			}
		}
	}

	vector<StringText> string_texts;
	for (auto line: sub_subtitle.lines) {
		for (auto block: line.blocks) {

			if (!block.font_size.specified()) {
				/* Fallback default font size if no other has been specified */
				block.font_size.set_points (48);
			}

			float v_position;
			dcp::VAlign v_align;
			if (needs_placement) {
				DCPOMATIC_ASSERT (line.vertical_position.line);
				double const multiplier = 1.2 * content()->line_spacing() * content()->y_scale() * block.font_size.proportional (72 * 11);
				switch (line.vertical_position.reference.get_value_or(sub::BOTTOM_OF_SCREEN)) {
				case sub::BOTTOM_OF_SCREEN:
				case sub::TOP_OF_SUBTITLE:
					/* This 0.9 is an arbitrary value to lift the bottom sub off the bottom
					   of the screen a bit to a pleasing degree.
					   */
					v_position = 0.9 -
						(1 + bottom_line.get() - line.vertical_position.line.get()) * multiplier;

					v_align = dcp::VAlign::TOP;
					break;
				case sub::TOP_OF_SCREEN:
					/* This 0.1 is another fudge factor to bring the top line away from the top of the screen a little */
					v_position = 0.12 + line.vertical_position.line.get() * multiplier;
					v_align = dcp::VAlign::TOP;
					break;
				case sub::VERTICAL_CENTRE_OF_SCREEN:
					v_position = line.vertical_position.line.get() * multiplier;
					v_align = dcp::VAlign::CENTER;
					break;
				}
			} else {
				DCPOMATIC_ASSERT (line.vertical_position.reference);
				if (line.vertical_position.proportional) {
					v_position = line.vertical_position.proportional.get();
				} else {
					DCPOMATIC_ASSERT (line.vertical_position.line);
					DCPOMATIC_ASSERT (line.vertical_position.lines);
					v_position = float(*line.vertical_position.line) / *line.vertical_position.lines;
				}

				if (lowest_proportional) {
					/* Adjust line spacing */
					v_position = ((v_position - lowest_proportional.get()) * content()->line_spacing()) + lowest_proportional.get();
				}

				switch (line.vertical_position.reference.get()) {
				case sub::TOP_OF_SCREEN:
					v_align = dcp::VAlign::TOP;
					break;
				case sub::VERTICAL_CENTRE_OF_SCREEN:
					v_align = dcp::VAlign::CENTER;
					break;
				case sub::BOTTOM_OF_SCREEN:
					v_align = dcp::VAlign::BOTTOM;
					break;
				default:
					v_align = dcp::VAlign::TOP;
					break;
				}
			}

			dcp::HAlign h_align;
			float h_position = line.horizontal_position.proportional;
			switch (line.horizontal_position.reference) {
			case sub::LEFT_OF_SCREEN:
				h_align = dcp::HAlign::LEFT;
				h_position = max(h_position, 0.05f);
				break;
			case sub::HORIZONTAL_CENTRE_OF_SCREEN:
				h_align = dcp::HAlign::CENTER;
				break;
			case sub::RIGHT_OF_SCREEN:
				h_align = dcp::HAlign::RIGHT;
				h_position = max(h_position, 0.05f);
				break;
			default:
				h_align = dcp::HAlign::CENTER;
				break;
			}

			/* The idea here (rightly or wrongly) is that we set the appearance based on the
			   values in the libsub objects, and these are overridden with values from the
			   content by the other emit_plain_start() above.
			*/

			auto dcp_subtitle = dcp::SubtitleString(
				optional<string>(),
				block.italic,
				block.bold,
				block.underline,
				block.colour.dcp(),
				block.font_size.points (72 * 11),
				1.0,
				dcp::Time (from.seconds(), 1000),
				/* XXX: hmm; this is a bit ugly (we don't know the to time yet) */
				dcp::Time (),
				h_position,
				h_align,
				v_position,
				v_align,
				dcp::Direction::LTR,
				escape_text(block.text),
				dcp::Effect::NONE,
				block.effect_colour.get_value_or(sub::Colour(0, 0, 0)).dcp(),
				/* Hack: we should use subtitle.fade_up and subtitle.fade_down here
				   but the times of these often don't have a frame rate associated
				   with them so the sub::Time won't convert them to milliseconds without
				   throwing an exception.  Since only DCP subs fill those in (and we don't
				   use libsub for DCP subs) we can cheat by just putting 0 in here.
				*/
				dcp::Time (),
				dcp::Time (),
				0
				);

			auto string_text = StringText(
				dcp_subtitle,
				content()->outline_width(),
				content()->get_font(block.font.get_value_or("")),
				dcp::Standard::SMPTE
				);
			set_forced_appearance(content(), string_text);
			string_texts.push_back(string_text);
		}
	}

	PlainStart(ContentStringText(from, string_texts));
	maybe_set_position(from);
}


void
TextDecoder::emit_stop (ContentTime to)
{
	Stop (to);
}


void
TextDecoder::emit_plain (ContentTimePeriod period, vector<dcp::SubtitleString> subtitles, dcp::Standard valign_standard)
{
	emit_plain_start (period.from, subtitles, valign_standard);
	emit_stop (period.to);
}


void
TextDecoder::emit_plain (ContentTimePeriod period, sub::Subtitle const& subtitles)
{
	emit_plain_start (period.from, subtitles);
	emit_stop (period.to);
}


/*  @param rect Area expressed as a fraction of the video frame that this subtitle
 *  is for (e.g. a width of 0.5 means the width of the subtitle is half the width
 *  of the video frame)
 */
void
TextDecoder::emit_bitmap (ContentTimePeriod period, shared_ptr<const Image> image, dcpomatic::Rect<double> rect)
{
	emit_bitmap_start ({ period.from, image, rect });
	emit_stop (period.to);
}


void
TextDecoder::seek ()
{
	_position = ContentTime ();
}


void
TextDecoder::maybe_set_position (dcpomatic::ContentTime position)
{
	if (!_position || position > *_position) {
		_position = position;
	}
}

