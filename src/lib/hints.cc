/*
    Copyright (C) 2016-2018 Carl Hetherington <cth@carlh.net>

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

#include "hints.h"
#include "types.h"
#include "film.h"
#include "content.h"
#include "video_content.h"
#include "subtitle_content.h"
#include "audio_processor.h"
#include "font.h"
#include "ratio.h"
#include "audio_analysis.h"
#include "compose.hpp"
#include "util.h"
#include <dcp/raw_convert.h>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>

#include "i18n.h"

using std::vector;
using std::string;
using std::pair;
using std::min;
using std::max;
using std::cout;
using boost::shared_ptr;
using boost::optional;

vector<string>
get_hints (shared_ptr<const Film> film)
{
	vector<string> hints;

	ContentList content = film->content ();

	bool big_font_files = false;
	if (film->interop ()) {
		BOOST_FOREACH (shared_ptr<Content> i, content) {
			if (i->subtitle) {
				BOOST_FOREACH (shared_ptr<Font> j, i->subtitle->fonts ()) {
					for (int k = 0; k < FontFiles::VARIANTS; ++k) {
						optional<boost::filesystem::path> const p = j->file (static_cast<FontFiles::Variant> (k));
						if (p && boost::filesystem::file_size (p.get()) >= (640 * 1024)) {
							big_font_files = true;
						}
					}
				}
			}
		}
	}

	if (big_font_files) {
		hints.push_back (_("You have specified a font file which is larger than 640kB.  This is very likely to cause problems on playback."));
	}

	if (film->audio_channels() < 6) {
		hints.push_back (_("Your DCP has fewer than 6 audio channels.  This may cause problems on some projectors."));
	}

	AudioProcessor const * ap = film->audio_processor();
	if (ap && (ap->id() == "stereo-5.1-upmix-a" || ap->id() == "stereo-5.1-upmix-b")) {
		hints.push_back (_("You are using DCP-o-matic's stereo-to-5.1 upmixer.  This is experimental and may result in poor-quality audio.  If you continue, you should listen to the resulting DCP in a cinema to make sure that it sounds good."));
	}

	int flat_or_narrower = 0;
	int scope = 0;
	BOOST_FOREACH (shared_ptr<const Content> i, content) {
		if (i->video) {
			Ratio const * r = i->video->scale().ratio ();
			if (r && r->id() == "239") {
				++scope;
			} else if (r && r->id() != "239" && r->id() != "190") {
				++flat_or_narrower;
			}
		}
	}

	string const film_container = film->container()->id();

	if (scope && !flat_or_narrower && film_container == "185") {
		hints.push_back (_("All of your content is in Scope (2.39:1) but your DCP's container is Flat (1.85:1).  This will letter-box your content inside a Flat (1.85:1) frame.  You may prefer to set your DCP's container to Scope (2.39:1) in the \"DCP\" tab."));
	}

	if (!scope && flat_or_narrower && film_container == "239") {
		hints.push_back (_("All of your content is at 1.85:1 or narrower but your DCP's container is Scope (2.39:1).  This will pillar-box your content inside a Flat (1.85:1) frame.  You may prefer to set your DCP's container to Flat (1.85:1) in the \"DCP\" tab."));
	}

	if (film_container != "185" && film_container != "239" && film_container != "190") {
		hints.push_back (_("Your DCP uses an unusual container ratio.  This may cause problems on some projectors.  If possible, use Flat or Scope for the DCP container ratio"));
	}

	if (film->j2k_bandwidth() >= 245000000) {
		hints.push_back (_("A few projectors have problems playing back very high bit-rate DCPs.  It is a good idea to drop the JPEG2000 bandwidth down to about 200Mbit/s; this is unlikely to have any visible effect on the image."));
	}

	if (film->interop() && film->video_frame_rate() != 24 && film->video_frame_rate() != 48) {
		string base = _("You are set up for an Interop DCP at a frame rate which is not officially supported.  You are advised either to change the frame rate of your DCP or to make a SMPTE DCP instead.");
		base += "  ";
		pair<double, double> range24 = film->speed_up_range (24);
		pair<double, double> range48 = film->speed_up_range (48);
		pair<double, double> range (max (range24.first, range48.first), min (range24.second, range48.second));
		string h;
		if (range.second > (29.0/24)) {
			h = base;
			h += _("However, setting your DCP frame rate to 24 or 48 will cause a significant speed-up of your content, and SMPTE DCPs are not supported by all projectors.");
		} else if (range.first < (24.0/29)) {
			h = base;
			h += _("However, setting your DCP frame rate to 24 or 48 will cause a significant slowdown of your content, and SMPTE DCPs are not supported by all projectors.");
		} else {
			h = _("You are set up for an Interop DCP at a frame rate which is not officially supported.  You are advised either to change the frame rate of your DCP or to make a SMPTE DCP instead (although SMPTE DCPs are not supported by all projectors).");
		}

		hints.push_back (h);
	}

	int vob = 0;
	BOOST_FOREACH (shared_ptr<const Content> i, content) {
		if (boost::algorithm::starts_with (i->path(0).filename().string(), "VTS_")) {
			++vob;
		}
	}

	if (vob > 1) {
		hints.push_back (String::compose (_("You have %1 files that look like they are VOB files from DVD. You should join them to ensure smooth joins between the files."), vob));
	}

	int three_d = 0;
	BOOST_FOREACH (shared_ptr<const Content> i, content) {
		if (i->video && i->video->frame_type() != VIDEO_FRAME_TYPE_2D) {
			++three_d;
		}
	}

	if (three_d > 0 && !film->three_d()) {
		hints.push_back (_("You are using 3D content but your DCP is set to 2D.  Set the DCP to 3D if you want to play it back on a 3D system (e.g. Real-D, MasterImage etc.)"));
	}

	boost::filesystem::path path = film->audio_analysis_path (film->playlist ());
	if (boost::filesystem::exists (path)) {
		try {
			shared_ptr<AudioAnalysis> an (new AudioAnalysis (path));

			string ch;

			vector<AudioAnalysis::PeakTime> sample_peak = an->sample_peak ();
			vector<float> true_peak = an->true_peak ();

			for (size_t i = 0; i < sample_peak.size(); ++i) {
				float const peak = max (sample_peak[i].peak, true_peak.empty() ? 0 : true_peak[i]);
				float const peak_dB = 20 * log10 (peak) + an->gain_correction (film->playlist ());
				if (peak_dB > -3) {
					ch += dcp::raw_convert<string> (short_audio_channel_name (i)) + ", ";
				}
			}

			ch = ch.substr (0, ch.length() - 2);

			if (!ch.empty ()) {
				hints.push_back (
					String::compose (
						_("Your audio level is very high (on %1).  You should reduce the gain of your audio content."),
						ch
						)
					);
			}
		} catch (OldFormatError& e) {
			/* The audio analysis is too old to load in; just skip this hint as if
			   it had never been run.
			*/
		}
	}

	return hints;
}
