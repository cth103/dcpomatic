/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

#include "analyse_subtitles_job.h"
#include "playlist.h"
#include "player.h"
#include "subtitle_analysis.h"
#include "bitmap_text.h"
#include "render_text.h"
#include "text_content.h"
#include "image.h"
#include <iostream>

#include "i18n.h"

using std::make_shared;
using std::shared_ptr;
using std::string;
using std::weak_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif

AnalyseSubtitlesJob::AnalyseSubtitlesJob (shared_ptr<const Film> film, shared_ptr<Content> content)
	: Job (film)
	, _content (content)
	, _path (_film->subtitle_analysis_path(content))
{
}


string
AnalyseSubtitlesJob::name () const
{
	return _("Analysing subtitles");
}


string
AnalyseSubtitlesJob::json_name () const
{
	return N_("analyse_subtitles");
}


void
AnalyseSubtitlesJob::run ()
{
	auto playlist = make_shared<Playlist>();
	auto content = _content.lock ();
	DCPOMATIC_ASSERT (content);
	playlist->add (_film, content);

	auto player = make_shared<Player>(_film, playlist);
	player->set_ignore_audio ();
	player->set_fast ();
	player->set_play_referenced ();
	player->Text.connect (bind(&AnalyseSubtitlesJob::analyse, this, _1, _2));

	set_progress_unknown ();

	if (!content->text.empty()) {
		while (!player->pass ()) {}
	}

	SubtitleAnalysis analysis (_bounding_box, content->text.front()->x_offset(), content->text.front()->y_offset());
	analysis.write (_path);

	set_progress (1);
	set_state (FINISHED_OK);
}


void
AnalyseSubtitlesJob::analyse (PlayerText text, TextType type)
{
	if (type != TextType::OPEN_SUBTITLE) {
		return;
	}

	for (auto const& i: text.bitmap) {
		if (!_bounding_box) {
			_bounding_box = i.rectangle;
		} else {
			_bounding_box->extend (i.rectangle);
		}
	}

	if (!text.string.empty()) {
		/* We can provide dummy values for time and frame rate here as they are only used to calculate fades */
		dcp::Size const frame = _film->frame_size();
		for (auto i: render_text(text.string, text.fonts, frame, dcpomatic::DCPTime(), 24)) {
			dcpomatic::Rect<double> rect (
					double(i.position.x) / frame.width, double(i.position.y) / frame.height,
					double(i.image->size().width) / frame.width, double(i.image->size().height) / frame.height
					);
			if (!_bounding_box) {
				_bounding_box = rect;
			} else {
				_bounding_box->extend (rect);
			}
		}
	}
}

