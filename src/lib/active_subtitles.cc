/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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

#include "active_subtitles.h"
#include "piece.h"
#include "subtitle_content.h"
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

using std::list;
using std::pair;
using std::make_pair;
using boost::weak_ptr;
using boost::shared_ptr;

list<PlayerSubtitles>
ActiveSubtitles::get (DCPTime time, bool always_burn_subtitles) const
{
	list<PlayerSubtitles> ps;

	for (Map::const_iterator i = _data.begin(); i != _data.end(); ++i) {

		shared_ptr<Piece> piece = i->first.lock ();
		if (!piece) {
			continue;
		}

		if (!piece->content->subtitle->use() || (!always_burn_subtitles && !piece->content->subtitle->burn())) {
			continue;
		}

		BOOST_FOREACH (Period j, i->second) {
			if (j.from <= time && (!j.to || j.to.get() > time)) {
				ps.push_back (j.subs);
			}
		}
	}

	return ps;
}

void
ActiveSubtitles::clear_before (DCPTime time)
{
	Map updated;
	for (Map::const_iterator i = _data.begin(); i != _data.end(); ++i) {
		list<Period> as;
		BOOST_FOREACH (Period j, i->second) {
			if (!j.to || j.to.get() >= time) {
				as.push_back (j);
			}
		}
		if (!as.empty ()) {
			updated[i->first] = as;
		}
	}
	_data = updated;
}

void
ActiveSubtitles::add_from (weak_ptr<Piece> piece, PlayerSubtitles ps, DCPTime from)
{
	if (_data.find(piece) == _data.end()) {
		_data[piece] = list<Period>();
	}
	_data[piece].push_back (Period (ps, from));
}

pair<PlayerSubtitles, DCPTime>
ActiveSubtitles::add_to (weak_ptr<Piece> piece, DCPTime to)
{
	DCPOMATIC_ASSERT (_data.find(piece) != _data.end());

	_data[piece].back().to = to;

	BOOST_FOREACH (SubtitleString& i, _data[piece].back().subs.text) {
		i.set_out (dcp::Time(to.seconds(), 1000));
	}

	return make_pair (_data[piece].back().subs, _data[piece].back().from);
}

bool
ActiveSubtitles::have (weak_ptr<Piece> piece) const
{
	Map::const_iterator i = _data.find(piece);
	if (i == _data.end()) {
		return false;
	}

	return !i->second.empty();
}

void
ActiveSubtitles::clear ()
{
	_data.clear ();
}
