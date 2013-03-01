/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file src/filter.cc
 *  @brief A class to describe one of FFmpeg's video or post-processing filters.
 */

#include "filter.h"
extern "C" {
#include <libavfilter/avfilter.h>
#include <libpostproc/postprocess.h>
}

#include "i18n.h"

using namespace std;

vector<Filter const *> Filter::_filters;

/** @param i Our id.
 *  @param n User-visible name.
 *  @param c User-visible category.
 *  @param v String for a FFmpeg video filter descriptor, or "".
 *  @param p String for a FFmpeg post-processing descriptor, or "".
 */
Filter::Filter (string i, string n, string c, string v, string p)
	: _id (i)
	, _name (n)
	, _category (c)
	, _vf (v)
	, _pp (p)
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
	   
	maybe_add (N_("pphb"),      _("Horizontal deblocking filter"),                _("De-blocking"),     N_(""),          N_("hb"));
	maybe_add (N_("ppvb"),      _("Vertical deblocking filter"),                  _("De-blocking"),     N_(""),          N_("vb"));
	maybe_add (N_("ppha"),      _("Horizontal deblocking filter A"),              _("De-blocking"),     N_(""),          N_("ha"));
	maybe_add (N_("ppva"),      _("Vertical deblocking filter A"),                _("De-blocking"),     N_(""),          N_("va"));
	maybe_add (N_("pph1"),      _("Experimental horizontal deblocking filter 1"), _("De-blocking"),     N_(""),          N_("h1"));
	maybe_add (N_("pphv"),      _("Experimental vertical deblocking filter 1"),   _("De-blocking"),     N_(""),          N_("v1"));
	maybe_add (N_("ppdr"),      _("Deringing filter"),                            _("Misc"),            N_(""),          N_("dr"));
	maybe_add (N_("pplb"),      _("Linear blend deinterlacer"),                   _("De-interlacing"),  N_(""),          N_("lb"));
	maybe_add (N_("ppli"),      _("Linear interpolating deinterlacer"),           _("De-interlacing"),  N_(""),          N_("li"));
	maybe_add (N_("ppci"),      _("Cubic interpolating deinterlacer"),            _("De-interlacing"),  N_(""),          N_("ci"));
	maybe_add (N_("ppmd"),      _("Median deinterlacer"),                         _("De-interlacing"),  N_(""),          N_("md"));
	maybe_add (N_("ppfd"),      _("FFMPEG deinterlacer"),                         _("De-interlacing"),  N_(""),          N_("fd"));
	maybe_add (N_("ppl5"),      _("FIR low-pass deinterlacer"),                   _("De-interlacing"),  N_(""),          N_("l5"));
	maybe_add (N_("mcdeint"),   _("Motion compensating deinterlacer"),            _("De-interlacing"),  N_("mcdeint"),   N_(""));
	maybe_add (N_("kerndeint"), _("Kernel deinterlacer"),                         _("De-interlacing"),  N_("kerndeint"), N_(""));
	maybe_add (N_("yadif"),     _("Yet Another Deinterlacing Filter"),            _("De-interlacing"),  N_("yadif"),     N_(""));
	maybe_add (N_("pptn"),      _("Temporal noise reducer"),                      _("Noise reduction"), N_(""),          N_("tn"));
	maybe_add (N_("ppfq"),      _("Force quantizer"),                             _("Misc"),            N_(""),          N_("fq"));
	maybe_add (N_("gradfun"),   _("Gradient debander"),                           _("Misc"),            N_("gradfun"),   N_(""));
	maybe_add (N_("unsharp"),   _("Unsharp mask and Gaussian blur"),              _("Misc"),            N_("unsharp"),   N_(""));
	maybe_add (N_("denoise3d"), _("3D denoiser"),                                 _("Noise reduction"), N_("denoise3d"), N_(""));
	maybe_add (N_("hqdn3d"),    _("High quality 3D denoiser"),                    _("Noise reduction"), N_("hqdn3d"),    N_(""));
	maybe_add (N_("telecine"),  _("Telecine filter"),                             _("Misc"),            N_("telecine"),  N_(""));
	maybe_add (N_("ow"),        _("Overcomplete wavelet denoiser"),               _("Noise reduction"), N_("mp=ow"),     N_(""));
}

void
Filter::maybe_add (string i, string n, string c, string v, string p)
{
	if (!v.empty ()) {
		if (avfilter_get_by_name (i.c_str())) {
			_filters.push_back (new Filter (i, n, c, v, p));
		}
	} else if (!p.empty ()) {
		pp_mode* m = pp_get_mode_by_name_and_quality (p.c_str(), PP_QUALITY_MAX);
		if (m) {
			_filters.push_back (new Filter (i, n, c, v, p));
			pp_free_mode (m);
		}
	}
}

/** @param filters Set of filters.
 *  @return A pair; .first is a string to pass to FFmpeg for the video filters,
 *  .second is a string to pass for the post-processors.
 */
pair<string, string>
Filter::ffmpeg_strings (vector<Filter const *> const & filters)
{
	string vf;
	string pp;

	for (vector<Filter const *>::const_iterator i = filters.begin(); i != filters.end(); ++i) {
		if (!(*i)->vf().empty ()) {
			if (!vf.empty ()) {
				vf += N_(",");
			}
			vf += (*i)->vf ();
		}
		
		if (!(*i)->pp().empty ()) {
			if (!pp.empty()) {
				pp += N_(",");
			}
			pp += (*i)->pp ();
		}
	}

	return make_pair (vf, pp);
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

