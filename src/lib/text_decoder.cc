/*
    Copyright (C) 2013-2017 Carl Hetherington <cth@carlh.net>

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

#include "text_decoder.h"
#include "text_content.h"
#include "util.h"
#include "log.h"
#include "compose.hpp"
#include <sub/subtitle.h>
#include <boost/algorithm/string.hpp>
#include <iostream>

using std::list;
using std::cout;
using std::string;
using std::min;
using std::max;
using std::shared_ptr;
using boost::optional;
using boost::function;
using namespace dcpomatic;

TextDecoder::TextDecoder (
	Decoder* parent,
	shared_ptr<const TextContent> c,
	ContentTime first
	)
	: DecoderPart (parent)
	, _content (c)
	, _position (first)
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
TextDecoder::emit_bitmap_start (ContentTime from, shared_ptr<Image> image, dcpomatic::Rect<double> rect)
{
	BitmapStart (ContentBitmapText (from, image, rect));
	_position = from;
}

void
TextDecoder::emit_plain_start (ContentTime from, list<dcp::SubtitleString> s)
{
	for (auto& i: s) {
		/* We must escape < and > in strings, otherwise they might confuse our subtitle
		   renderer (which uses some HTML-esque markup to do bold/italic etc.)
		*/
		string t = i.text ();
		boost::algorithm::replace_all (t, "<", "&lt;");
		boost::algorithm::replace_all (t, ">", "&gt;");
		i.set_text (t);

		/* Set any forced appearance */
		if (content()->colour()) {
			i.set_colour (*content()->colour());
		}
		if (content()->effect_colour()) {
			i.set_effect_colour (*content()->effect_colour());
		}
		if (content()->effect()) {
			i.set_effect (*content()->effect());
		}
		if (content()->fade_in()) {
			i.set_fade_up_time (dcp::Time(content()->fade_in()->seconds(), 1000));
		}
		if (content()->fade_out()) {
			i.set_fade_down_time (dcp::Time(content()->fade_out()->seconds(), 1000));
		}
	}

	PlainStart (ContentStringText (from, s));
	_position = from;
}

void
TextDecoder::emit_plain_start (ContentTime from, sub::Subtitle const & subtitle)
{
	/* See if our next subtitle needs to be vertically placed on screen by us */
	bool needs_placement = false;
	optional<int> bottom_line;
	for (auto i: subtitle.lines) {
		if (!i.vertical_position.reference || (i.vertical_position.line && !i.vertical_position.lines) || i.vertical_position.reference.get() == sub::TOP_OF_SUBTITLE) {
			needs_placement = true;
			if (!bottom_line || bottom_line.get() < i.vertical_position.line.get()) {
				bottom_line = i.vertical_position.line.get();
			}
		}
	}

	/* Find the lowest proportional position */
	optional<float> lowest_proportional;
	for (auto i: subtitle.lines) {
		if (i.vertical_position.proportional) {
			if (!lowest_proportional) {
				lowest_proportional = i.vertical_position.proportional;
			} else {
				lowest_proportional = min (lowest_proportional.get(), i.vertical_position.proportional.get());
			}
		}
	}

	list<dcp::SubtitleString> out;
	for (auto i: subtitle.lines) {
		for (auto j: i.blocks) {

			if (!j.font_size.specified()) {
				/* Fallback default font size if no other has been specified */
				j.font_size.set_points (48);
			}

			float v_position;
			dcp::VAlign v_align;
			if (needs_placement) {
				DCPOMATIC_ASSERT (i.vertical_position.line);
				double const multiplier = 1.2 * content()->line_spacing() * content()->y_scale() * j.font_size.proportional (72 * 11);
				switch (i.vertical_position.reference.get_value_or(sub::BOTTOM_OF_SCREEN)) {
				case sub::BOTTOM_OF_SCREEN:
				case sub::TOP_OF_SUBTITLE:
					/* This 1.015 is an arbitrary value to lift the bottom sub off the bottom
					   of the screen a bit to a pleasing degree.
					   */
					v_position = 1.015 -
						(1 + bottom_line.get() - i.vertical_position.line.get()) * multiplier;

					v_align = dcp::VAlign::TOP;
					break;
				case sub::TOP_OF_SCREEN:
					/* This 0.1 is another fudge factor to bring the top line away from the top of the screen a little */
					v_position = 0.12 + i.vertical_position.line.get() * multiplier;
					v_align = dcp::VAlign::TOP;
					break;
				case sub::VERTICAL_CENTRE_OF_SCREEN:
					v_position = i.vertical_position.line.get() * multiplier;
					v_align = dcp::VAlign::CENTER;
					break;
				}
			} else {
				DCPOMATIC_ASSERT (i.vertical_position.reference);
				if (i.vertical_position.proportional) {
					v_position = i.vertical_position.proportional.get();
				} else {
					DCPOMATIC_ASSERT (i.vertical_position.line);
					DCPOMATIC_ASSERT (i.vertical_position.lines);
					v_position = float(*i.vertical_position.line) / *i.vertical_position.lines;
				}

				if (lowest_proportional) {
					/* Adjust line spacing */
					v_position = ((v_position - lowest_proportional.get()) * content()->line_spacing()) + lowest_proportional.get();
				}

				switch (i.vertical_position.reference.get()) {
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
			float h_position = i.horizontal_position.proportional;
			switch (i.horizontal_position.reference) {
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

			out.push_back (
				dcp::SubtitleString (
					string(TEXT_FONT_ID),
					j.italic,
					j.bold,
					j.underline,
					j.colour.dcp(),
					j.font_size.points (72 * 11),
					1.0,
					dcp::Time (from.seconds(), 1000),
					/* XXX: hmm; this is a bit ugly (we don't know the to time yet) */
					dcp::Time (),
					h_position,
					h_align,
					v_position,
					v_align,
					dcp::Direction::LTR,
					j.text,
					dcp::Effect::NONE,
					j.effect_colour.get_value_or(sub::Colour(0, 0, 0)).dcp(),
					/* Hack: we should use subtitle.fade_up and subtitle.fade_down here
					   but the times of these often don't have a frame rate associated
					   with them so the sub::Time won't convert them to milliseconds without
					   throwing an exception.  Since only DCP subs fill those in (and we don't
					   use libsub for DCP subs) we can cheat by just putting 0 in here.
					*/
					dcp::Time (),
					dcp::Time ()
					)
				);
		}
	}

	emit_plain_start (from, out);
}

void
TextDecoder::emit_stop (ContentTime to)
{
	Stop (to);
}

void
TextDecoder::emit_plain (ContentTimePeriod period, list<dcp::SubtitleString> s)
{
	emit_plain_start (period.from, s);
	emit_stop (period.to);
}

void
TextDecoder::emit_plain (ContentTimePeriod period, sub::Subtitle const & s)
{
	emit_plain_start (period.from, s);
	emit_stop (period.to);
}

/*  @param rect Area expressed as a fraction of the video frame that this subtitle
 *  is for (e.g. a width of 0.5 means the width of the subtitle is half the width
 *  of the video frame)
 */
void
TextDecoder::emit_bitmap (ContentTimePeriod period, shared_ptr<Image> image, dcpomatic::Rect<double> rect)
{
	emit_bitmap_start (period.from, image, rect);
	emit_stop (period.to);
}

void
TextDecoder::seek ()
{
	_position = ContentTime ();
}
