/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


/** @file src/filter.cc
 *  @brief A class to describe one of FFmpeg's video or audio filters.
 */


#include "dcpomatic_assert.h"
#include "filter.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
extern "C" {
#include <libavfilter/avfilter.h>
}
LIBDCP_ENABLE_WARNINGS
#include <algorithm>

#include "i18n.h"


using std::string;
using std::vector;
using boost::optional;


vector<Filter> Filter::_filters;


/** @param id Our id.
 *  @param name User-visible name.
 *  @param category User-visible category.
 *  @param ffmpeg_string String for a FFmpeg filter descriptor.
 */
Filter::Filter(string id, string name, string category, string ffmpeg_string)
	: _id(id)
	, _name(name)
	, _category(category)
	, _ffmpeg(ffmpeg_string)
{

}


/** @return All available filters */
vector<Filter>
Filter::all()
{
	return _filters;
}


/** Set up the static _filters vector; must be called before from_*
 *  methods are used.
 */
void
Filter::setup_filters()
{
	/* Note: "none" is a magic id name, so don't use it here */

	auto maybe_add = [](string id, string name, string category, string ffmpeg)
	{
		string check_name = ffmpeg;
		size_t end = check_name.find("=");
		if (end != string::npos) {
			check_name = check_name.substr(0, end);
		}

		if (avfilter_get_by_name(check_name.c_str())) {
			_filters.push_back(Filter(id, name, category, ffmpeg));
		}
	};

	maybe_add(N_("vflip"),       _("Vertical flip"),                     _("Orientation"),     N_("vflip"));
	maybe_add(N_("hflip"),       _("Horizontal flip"),                   _("Orientation"),     N_("hflip"));
	maybe_add(N_("90clock"),     _("Rotate 90 degrees clockwise"),       _("Orientation"),     N_("transpose=dir=clock"));
	maybe_add(N_("90anticlock"), _("Rotate 90 degrees anti-clockwise"),  _("Orientation"),     N_("transpose=dir=cclock"));
	maybe_add(N_("mcdeint"),     _("Motion compensating deinterlacer"),  _("De-interlacing"),  N_("mcdeint"));
	maybe_add(N_("kerndeint"),   _("Kernel deinterlacer"),		     _("De-interlacing"),  N_("kerndeint"));
	maybe_add(N_("yadif"),	     _("Yet Another Deinterlacing Filter"),  _("De-interlacing"),  N_("yadif"));
	maybe_add(N_("bwdif"),	     _("Bob Weaver Deinterlacing Filter"),   _("De-interlacing"),  N_("bwdif"));
	maybe_add(N_("weave"),	     _("Weave filter"),                      _("De-interlacing"),  N_("weave"));
	maybe_add(N_("gradfun"),     _("Gradient debander"),	             _("Misc"),	           N_("gradfun"));
	maybe_add(N_("unsharp"),     _("Unsharp mask and Gaussian blur"),    _("Misc"),	           N_("unsharp"));
	maybe_add(N_("denoise3d"),   _("3D denoiser"),		             _("Noise reduction"), N_("denoise3d"));
	maybe_add(N_("hqdn3d"),      _("High quality 3D denoiser"),          _("Noise reduction"), N_("hqdn3d"));
	maybe_add(N_("telecine"),    _("Telecine filter"),	             _("Misc"),	           N_("telecine"));
	maybe_add(N_("ow"),	     _("Overcomplete wavelet denoiser"),     _("Noise reduction"), N_("mp=ow"));
	maybe_add(N_("premultiply"), _("Premultiply alpha channel"),         _("Misc"),            N_("premultiply=inplace=1"));
}


/** @param filters Set of filters.
 *  @return String to pass to FFmpeg for the video filters.
 */
string
Filter::ffmpeg_string(vector<Filter> const& filters)
{
	string ff;

	for (auto const& i: filters) {
		if (!ff.empty()) {
			ff += N_(",");
		}
		ff += i.ffmpeg();
	}

	return ff;
}


/** @param d Our id.
 *  @return Corresponding Filter, if found.
 */
optional<Filter>
Filter::from_id(string id)
{
	auto iter = std::find_if(_filters.begin(), _filters.end(), [id](Filter const& filter) { return filter.id() == id; });
	if (iter == _filters.end()) {
		return {};
	}
	return *iter;
}


bool
operator==(Filter const& a, Filter const& b)
{
	return a.id() == b.id() && a.name() == b.name() && a.category() == b.category() && a.ffmpeg() == b.ffmpeg();
}


bool
operator!=(Filter const& a, Filter const& b)
{
	return a.id() != b.id() || a.name() != b.name() || a.category() != b.category() || a.ffmpeg() != b.ffmpeg();
}


bool
operator<(Filter const& a, Filter const& b)
{
	if (a.id() != b.id()) {
		return a.id() < b.id();
	}

	if (a.name() != b.name()) {
		return a.name() < b.name();
	}

	if (a.category() != b.category()) {
		return a.category() < b.category();
	}

	return a.ffmpeg() < b.ffmpeg();
}
