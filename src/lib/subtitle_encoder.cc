/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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
#include "job.h"
#include "player.h"
#include "subtitle_encoder.h"
#include <dcp/interop_subtitle_asset.h>
#include <dcp/raw_convert.h>
#include <dcp/smpte_subtitle_asset.h>
#include <boost/filesystem.hpp>
#include <boost/bind/bind.hpp>

#include "i18n.h"


using std::make_pair;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::raw_convert;


/** @param output Directory, if there will be multiple output files, or a filename.
 *  @param initial_name Hint that may be used to create filenames, if @ref output is a directory.
 *  @param include_font true to refer to and export any font file (for Interop; ignored for SMPTE).
 */
SubtitleEncoder::SubtitleEncoder (shared_ptr<const Film> film, shared_ptr<Job> job, boost::filesystem::path output, string initial_name, bool split_reels, bool include_font)
	: Encoder (film, job)
	, _split_reels (split_reels)
	, _include_font (include_font)
	, _reel_index (0)
	, _length (film->length())
{
	_player->set_play_referenced ();
	_player->set_ignore_video ();
	_player->set_ignore_audio ();
	_player->Text.connect (boost::bind(&SubtitleEncoder::text, this, _1, _2, _3, _4));

	string const extension = film->interop() ? ".xml" : ".mxf";

	int const files = split_reels ? film->reels().size() : 1;
	for (int i = 0; i < files; ++i) {

		boost::filesystem::path filename = output;
		if (boost::filesystem::is_directory(filename)) {
			if (files > 1) {
				/// TRANSLATORS: _reel%1 here is to be added to an export filename to indicate
				/// which reel it is.  Preserve the %1; it will be replaced with the reel number.
				filename /= String::compose("%1_reel%2", initial_name, i + 1);
			} else {
				filename /= initial_name;
			}
		}

		_assets.push_back (make_pair(shared_ptr<dcp::SubtitleAsset>(), boost::filesystem::change_extension(filename, extension)));
	}

	for (auto i: film->reels()) {
		_reels.push_back (i);
	}

	_default_font = dcp::ArrayData (default_font_file());
}


void
SubtitleEncoder::go ()
{
	{
		shared_ptr<Job> job = _job.lock ();
		DCPOMATIC_ASSERT (job);
		job->sub (_("Extracting"));
	}

	_reel_index = 0;

	while (!_player->pass()) {}

	int reel = 0;
	for (auto& i: _assets) {
		if (!i.first) {
			/* No subtitles arrived for this asset; make an empty one so we write something to the output */
			if (_film->interop()) {
				auto s = make_shared<dcp::InteropSubtitleAsset>();
				s->set_movie_title (_film->name());
				s->set_reel_number (raw_convert<string>(reel + 1));
				i.first = s;
			} else {
				auto s = make_shared<dcp::SMPTESubtitleAsset>();
				s->set_content_title_text (_film->name());
				s->set_reel_number (reel + 1);
				i.first = s;
			}
		}

		if (!_film->interop() || _include_font) {
			for (auto j: _player->get_subtitle_fonts()) {
				i.first->add_font(j->id(), j->data().get_value_or(_default_font));
			}
		}

		i.first->write (i.second);
		++reel;
	}
}


void
SubtitleEncoder::text (PlayerText subs, TextType type, optional<DCPTextTrack> track, dcpomatic::DCPTimePeriod period)
{
	if (type != TextType::OPEN_SUBTITLE) {
		return;
	}

	if (!_assets[_reel_index].first) {
		shared_ptr<dcp::SubtitleAsset> asset;
		auto lang = _film->subtitle_languages ();
		if (_film->interop ()) {
			auto s = make_shared<dcp::InteropSubtitleAsset>();
			s->set_movie_title (_film->name());
			if (lang.first) {
				s->set_language (lang.first->to_string());
			}
			s->set_reel_number (raw_convert<string>(_reel_index + 1));
			_assets[_reel_index].first = s;
		} else {
			auto s = make_shared<dcp::SMPTESubtitleAsset>();
			s->set_content_title_text (_film->name());
			if (lang.first) {
				s->set_language (*lang.first);
			} else if (track->language) {
				s->set_language (track->language.get());
			}
			s->set_edit_rate (dcp::Fraction (_film->video_frame_rate(), 1));
			s->set_reel_number (_reel_index + 1);
			s->set_time_code_rate (_film->video_frame_rate());
			s->set_start_time (dcp::Time());
			if (_film->encrypted ()) {
				s->set_key (_film->key ());
			}
			_assets[_reel_index].first = s;
		}
	}

	for (auto i: subs.string) {
		/* XXX: couldn't / shouldn't we use period here rather than getting time from the subtitle? */
		i.set_in  (i.in());
		i.set_out (i.out());
		if (_film->interop() && !_include_font) {
			i.unset_font ();
		}
		_assets[_reel_index].first->add (make_shared<dcp::SubtitleString>(i));
	}

	if (_split_reels && (_reel_index < int(_reels.size()) - 1) && period.from > _reels[_reel_index].from) {
		++_reel_index;
	}

	_last = period.from;

	auto job = _job.lock ();
	if (job) {
		job->set_progress (float(period.from.get()) / _length.get());
	}
}


Frame
SubtitleEncoder::frames_done () const
{
	if (!_last) {
		return 0;
	}

	/* XXX: assuming 24fps here but I don't think it matters */
	return _last->seconds() * 24;
}
