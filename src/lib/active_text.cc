/*
    Copyright (C) 2017-2018 Carl Hetherington <cth@carlh.net>

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


#include "active_text.h"
#include "text_content.h"


using std::list;
using std::pair;
using std::make_pair;
using std::weak_ptr;
using std::shared_ptr;
using boost::optional;
using namespace dcpomatic;


/** Get the open captions that should be burnt into a given period.
 *  @param period Period of interest.
 *  @param always_burn_captions Always burn captions even if their content is not set to burn.
 */
list<PlayerText>
ActiveText::get_burnt (DCPTimePeriod period, bool always_burn_captions) const
{
	boost::mutex::scoped_lock lm (_mutex);

	list<PlayerText> ps;

	for (auto const& i: _data) {

		auto caption = i.first.lock ();
		if (!caption) {
			continue;
		}

		if (!caption->use() || (!always_burn_captions && !caption->burn())) {
			/* Not burning this content */
			continue;
		}

		for (auto j: i.second) {
			DCPTimePeriod test (j.from, j.to.get_value_or(DCPTime::max()));
			auto overlap = period.overlap (test);
			if (overlap && overlap->duration() > DCPTime(period.duration().get() / 2)) {
				ps.push_back (j.subs);
			}
		}
	}

	return ps;
}


/** Remove subtitles that finish before a given time from our list.
 *  @param time Time to remove before.
 */
void
ActiveText::clear_before (DCPTime time)
{
	boost::mutex::scoped_lock lm (_mutex);

	Map updated;
	for (auto const& i: _data) {
		list<Period> as;
		for (auto j: i.second) {
			if (!j.to || j.to.get() >= time) {
				as.push_back (j);
			}
		}
		if (!as.empty ()) {
			updated[i.first] = as;
		}
	}
	_data = updated;
}


/** Add a new subtitle with a from time.
 *  @param content Content that the subtitle is from.
 *  @param ps Subtitles.
 *  @param from From time for these subtitles.
 */
void
ActiveText::add_from (weak_ptr<const TextContent> content, PlayerText ps, DCPTime from)
{
	boost::mutex::scoped_lock lm (_mutex);

	if (_data.find(content) == _data.end()) {
		_data[content] = list<Period>();
	}
	_data[content].push_back (Period (ps, from));
}


/** Add the to time for the last subtitle added from a piece of content.
 *  @param content Content that the subtitle is from.
 *  @param to To time for the last subtitle submitted to add_from for this content.
 *  @return Return the corresponding subtitles and their from time.
 */
pair<PlayerText, DCPTime>
ActiveText::add_to (weak_ptr<const TextContent> content, DCPTime to)
{
	boost::mutex::scoped_lock lm (_mutex);

	DCPOMATIC_ASSERT (_data.find(content) != _data.end());

	_data[content].back().to = to;

	for (auto& i: _data[content].back().subs.string) {
		i.set_out (dcp::Time(to.seconds(), 1000));
	}

	return make_pair (_data[content].back().subs, _data[content].back().from);
}


/** @param content Some content.
 *  @return true if we have any active subtitles from this content.
 */
bool
ActiveText::have (weak_ptr<const TextContent> content) const
{
	boost::mutex::scoped_lock lm (_mutex);

	auto i = _data.find(content);
	if (i == _data.end()) {
		return false;
	}

	return !i->second.empty();
}


void
ActiveText::clear ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_data.clear ();
}
