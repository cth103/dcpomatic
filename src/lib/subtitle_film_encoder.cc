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


#include "film.h"
#include "job.h"
#include "player.h"
#include "subtitle_film_encoder.h"
#include <dcp/filesystem.h>
#include <dcp/interop_text_asset.h>
#include <dcp/smpte_text_asset.h>
#include <fmt/format.h>
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


/** @param output Directory, if there will be multiple output files, or a filename.
 *  @param initial_name Hint that may be used to create filenames, if @ref output is a directory.
 *  @param include_font true to refer to and export any font file (for Interop; ignored for SMPTE).
 */
SubtitleFilmEncoder::SubtitleFilmEncoder(
	shared_ptr<const Film> film,
	shared_ptr<Job> job,
	boost::filesystem::path output,
	string initial_name,
	bool split_reels,
	bool include_font,
	dcp::Standard standard
	)
	: FilmEncoder(film, job)
	, _split_reels(split_reels)
	, _include_font(include_font)
	, _reel_index(0)
	, _length(film->length())
	, _standard(standard)
{
	_player.set_play_referenced();
	_player.set_ignore_video();
	_player.set_ignore_audio();
	_player.Text.connect(boost::bind(&SubtitleFilmEncoder::text, this, _1, _2, _3, _4));

	string const extension = standard == dcp::Standard::INTEROP ? ".xml" : ".mxf";

	int const files = split_reels ? film->reels().size() : 1;
	for (int i = 0; i < files; ++i) {

		boost::filesystem::path filename = output;
		if (dcp::filesystem::is_directory(filename)) {
			if (files > 1) {
				/// TRANSLATORS: _reel{} here is to be added to an export filename to indicate
				/// which reel it is.  Preserve the {}; it will be replaced with the reel number.
				filename /= fmt::format("{}_reel{}", initial_name, i + 1);
			} else {
				filename /= initial_name;
			}
		}

		_assets.push_back(make_pair(shared_ptr<dcp::TextAsset>(), dcp::filesystem::change_extension(filename, extension)));
	}

	for (auto i: film->reels()) {
		_reels.push_back(i);
	}

	_default_font = dcp::ArrayData(default_font_file());
}


void
SubtitleFilmEncoder::go()
{
	{
		shared_ptr<Job> job = _job.lock();
		DCPOMATIC_ASSERT(job);
		job->sub(_("Extracting"));
	}

	_reel_index = 0;

	while (!_player.pass()) {}

	int reel = 0;
	for (auto& i: _assets) {
		if (!i.first) {
			/* No subtitles arrived for this asset; make an empty one so we write something to the output */
			switch (_standard) {
			case dcp::Standard::INTEROP:
			{
				auto s = make_shared<dcp::InteropTextAsset>();
				s->set_movie_title(_film->name());
				s->set_reel_number(fmt::to_string(reel + 1));
				i.first = s;
				break;
			}
			case dcp::Standard::SMPTE:
			{
				auto s = make_shared<dcp::SMPTETextAsset>();
				s->set_content_title_text(_film->name());
				s->set_reel_number(reel + 1);
				i.first = s;
				break;
			}
			}
		}

		if (_standard == dcp::Standard::SMPTE || _include_font) {
			for (auto j: _player.get_subtitle_fonts()) {
				i.first->add_font(j->id(), j->data().get_value_or(_default_font));
			}
		}

		i.first->write(i.second);
		++reel;
	}
}


void
SubtitleFilmEncoder::text(PlayerText subs, TextType type, optional<DCPTextTrack> track, dcpomatic::DCPTimePeriod period)
{
	if (type != TextType::OPEN_SUBTITLE) {
		return;
	}

	if (!_assets[_reel_index].first) {
		shared_ptr<dcp::TextAsset> asset;
		auto const lang = _film->open_text_languages();
		switch (_standard) {
		case dcp::Standard::INTEROP:
		{
			auto s = make_shared<dcp::InteropTextAsset>();
			s->set_movie_title(_film->name());
			if (lang.first) {
				s->set_language(lang.first->as_string());
			}
			s->set_reel_number(fmt::to_string(_reel_index + 1));
			_assets[_reel_index].first = s;
			break;
		}
		case dcp::Standard::SMPTE:
		{
			auto s = make_shared<dcp::SMPTETextAsset>();
			s->set_content_title_text(_film->name());
			if (lang.first) {
				s->set_language(*lang.first);
			} else if (track->language) {
				s->set_language(track->language.get());
			}
			s->set_edit_rate(dcp::Fraction(_film->video_frame_rate(), 1));
			s->set_reel_number(_reel_index + 1);
			s->set_time_code_rate(_film->video_frame_rate());
			s->set_start_time(dcp::Time());
			if (_film->encrypted()) {
				s->set_key(_film->key());
			}
			_assets[_reel_index].first = s;
			break;
		}
		}
	}

	for (auto i: subs.string) {
		/* XXX: couldn't / shouldn't we use period here rather than getting time from the subtitle? */
		i.set_in (i.in());
		i.set_out(i.out());
		if (_standard == dcp::Standard::INTEROP && !_include_font) {
			i.unset_font();
		}
		_assets[_reel_index].first->add(make_shared<dcp::TextString>(i));
	}

	if (_split_reels && (_reel_index < int(_reels.size()) - 1) && period.from > _reels[_reel_index].from) {
		++_reel_index;
	}

	_last = period.from;

	if (auto job = _job.lock()) {
		job->set_progress(float(period.from.get()) / _length.get());
	}
}


Frame
SubtitleFilmEncoder::frames_done() const
{
	if (!_last) {
		return 0;
	}

	/* XXX: assuming 24fps here but I don't think it matters */
	return _last->seconds() * 24;
}
