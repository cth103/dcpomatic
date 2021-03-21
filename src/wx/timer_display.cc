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


#include "timer_display.h"
#include "wx_util.h"
#include "lib/timer.h"
#include <dcp/locale_convert.h>
#include <list>


using std::map;
using std::list;
using std::pair;
using std::make_pair;
using std::string;


TimerDisplay::TimerDisplay (wxWindow* parent, StateTimer const & timer, int gets)
	: TableDialog (parent, std_to_wx(timer.name()), 4, 0, false)
{
	auto counts = timer.counts ();
	list<pair<string, StateTimer::Counts>> sorted;
	for (auto const& i: counts) {
		sorted.push_back (make_pair(i.first, i.second));
	}

	sorted.sort ([](pair<string, StateTimer::Counts> const& a, pair<string, StateTimer::Counts> const& b) {
		return a.second.total_time > b.second.total_time;
	});

	add (wxString("get() calls"), true);
	add (std_to_wx(dcp::locale_convert<string>(gets)), false);
	add_spacer ();
	add_spacer ();

	for (auto const& i: sorted) {
		add (std_to_wx(i.first), true);
		add (std_to_wx(dcp::locale_convert<string>(i.second.total_time)), false);
		add (std_to_wx(dcp::locale_convert<string>(i.second.number)), false);
		add (std_to_wx(dcp::locale_convert<string>(i.second.total_time / i.second.number)), false);
	}

	layout ();
}
