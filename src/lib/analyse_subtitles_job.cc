/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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
#include "bitmap_text.h"
#include "film.h"
#include "image.h"
#include "player.h"
#include "playlist.h"
#include "render_text.h"
#include "subtitle_analysis.h"
#include "text_content.h"
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


AnalyseSubtitlesJob::~AnalyseSubtitlesJob()
{
	stop_thread();
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

	auto player = make_shared<Player>(_film, playlist, false);
	player->set_ignore_audio ();
	player->set_fast ();
	player->set_play_referenced ();
	player->Text.connect (bind(&AnalyseSubtitlesJob::analyse, this, _1, _2));

	set_progress_unknown ();

	if (!content->text.empty()) {
		while (!player->pass ()) {
			boost::this_thread::interruption_point();
		}
	}

	SubtitleAnalysis analysis (_bounding_box, content->text.front()->x_offset(), content->text.front()->y_offset());
	analysis.write (_path);

	set_progress (1);
	set_state (FINISHED_OK);
}


void
AnalyseSubtitlesJob::analyse(PlayerText const& text, TextType type)
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

	if (text.string.empty()) {
		return;
	}

	/* We can provide dummy values for time and frame rate here as they are only used to calculate fades */
	dcp::Size const frame = _film->frame_size();
	std::vector<dcp::SubtitleStandard> override_standard;
	if (_film->interop()) {
		/* Since the film is Interop there is only one way the vpositions in the subs can be interpreted
		 * (we assume).
		 */
		override_standard.push_back(dcp::SubtitleStandard::INTEROP);
	} else {
		/* We're using the great new SMPTE standard, which means there are two different ways that vposition
		 * could be interpreted; we will write SMPTE-2014 standard assets, but if the projection system uses
		 * SMPTE 20{07,10} instead they won't be placed how we intended.  To show the user this, make the
		 * bounding rectangle enclose both possibilities.
		 */
		override_standard.push_back(dcp::SubtitleStandard::SMPTE_2007);
		override_standard.push_back(dcp::SubtitleStandard::SMPTE_2014);
	}

	for (auto standard: override_standard) {
		for (auto i: bounding_box(text.string, frame, standard)) {
			dcpomatic::Rect<double> rect (
				double(i.x) / frame.width, double(i.y) / frame.height,
				double(i.width) / frame.width, double(i.height) / frame.height
				);
			if (!_bounding_box) {
				_bounding_box = rect;
			} else {
				_bounding_box->extend (rect);
			}
		}
	}
}

