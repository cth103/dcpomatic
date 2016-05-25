/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include "filter.h"
extern "C" {
#include <libavfilter/avfilter.h>
}
#include <boost/foreach.hpp>

#include "i18n.h"

using namespace std;

vector<Filter const *> Filter::_filters;

/** @param i Our id.
 *  @param n User-visible name.
 *  @param c User-visible category.
 *  @param f String for a FFmpeg filter descriptor.
 */
Filter::Filter (string i, string n, string c, string f)
	: _id (i)
	, _name (n)
	, _category (c)
	, _ffmpeg (f)
{

}

/** @return All available filters */
vector<Filter const *>
Filter::all ()
{
	return _filters;
}

/** Set up the static _filters vector; must be called before from_*
 *  methods are used.
 */
void
Filter::setup_filters ()
{
	/* Note: "none" is a magic id name, so don't use it here */

	maybe_add (N_("mcdeint"),   _("Motion compensating deinterlacer"), _("De-interlacing"),  N_("mcdeint"));
	maybe_add (N_("kerndeint"), _("Kernel deinterlacer"),		   _("De-interlacing"),  N_("kerndeint"));
	maybe_add (N_("yadif"),	    _("Yet Another Deinterlacing Filter"), _("De-interlacing"),  N_("yadif"));
	maybe_add (N_("gradfun"),   _("Gradient debander"),	           _("Misc"),	         N_("gradfun"));
	maybe_add (N_("unsharp"),   _("Unsharp mask and Gaussian blur"),   _("Misc"),	         N_("unsharp"));
	maybe_add (N_("denoise3d"), _("3D denoiser"),		           _("Noise reduction"), N_("denoise3d"));
	maybe_add (N_("hqdn3d"),    _("High quality 3D denoiser"),         _("Noise reduction"), N_("hqdn3d"));
	maybe_add (N_("telecine"),  _("Telecine filter"),	           _("Misc"),	         N_("telecine"));
	maybe_add (N_("ow"),	    _("Overcomplete wavelet denoiser"),	   _("Noise reduction"), N_("mp=ow"));
}

void
Filter::maybe_add (string i, string n, string c, string f)
{
	if (avfilter_get_by_name (i.c_str())) {
		_filters.push_back (new Filter (i, n, c, f));
	}
}

/** @param filters Set of filters.
 *  @return String to pass to FFmpeg for the video filters.
 */
string
Filter::ffmpeg_string (vector<Filter const *> const & filters)
{
	string ff;

	BOOST_FOREACH (Filter const * i, filters) {
		if (!ff.empty ()) {
			ff += N_(",");
		}
		ff += i->ffmpeg ();
	}

	return ff;
}

/** @param d Our id.
 *  @return Corresponding Filter, or 0.
 */
Filter const *
Filter::from_id (string d)
{
	vector<Filter const *>::iterator i = _filters.begin ();
	while (i != _filters.end() && (*i)->id() != d) {
		++i;
	}

	if (i == _filters.end ()) {
		return 0;
	}

	return *i;
}
