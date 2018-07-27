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
#include "text_content.h"
#include "audio_processor.h"
#include "font.h"
#include "ratio.h"
#include "audio_analysis.h"
#include "compose.hpp"
#include "util.h"
#include "cross.h"
#include "player.h"
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
using boost::weak_ptr;
using boost::optional;
using boost::bind;

Hints::Hints (weak_ptr<const Film> film)
	: _film (film)
	, _thread (0)
	, _long_ccap (false)
	, _overlap_ccap (false)
	, _too_many_ccap_lines (false)
{

}

void
Hints::stop_thread ()
{
	if (_thread) {
		try {
			_thread->interrupt ();
			_thread->join ();
		} catch (...) {

		}

		delete _thread;
		_thread = 0;
	}
}

Hints::~Hints ()
{
	stop_thread ();
}

void
Hints::start ()
{
	stop_thread ();
	_long_ccap = false;
	_overlap_ccap = false;
	_too_many_ccap_lines = false;
	_thread = new boost::thread (bind(&Hints::thread, this));
}

void
Hints::thread ()
{
	shared_ptr<const Film> film = _film.lock ();
	if (!film) {
		return;
	}

	ContentList content = film->content ();

	bool big_font_files = false;
	if (film->interop ()) {
		BOOST_FOREACH (shared_ptr<Content> i, content) {
			BOOST_FOREACH (shared_ptr<TextContent> j, i->text) {
				BOOST_FOREACH (shared_ptr<Font> k, j->fonts()) {
					for (int l = 0; l < FontFiles::VARIANTS; ++l) {
						optional<boost::filesystem::path> const p = k->file (static_cast<FontFiles::Variant>(l));
						if (p && boost::filesystem::file_size (p.get()) >= (640 * 1024)) {
							big_font_files = true;
						}
					}
				}
			}
		}
	}

	if (big_font_files) {
		hint (_("You have specified a font file which is larger than 640kB.  This is very likely to cause problems on playback."));
	}

	if (film->audio_channels() < 6) {
		hint (_("Your DCP has fewer than 6 audio channels.  This may cause problems on some projectors."));
	}

	AudioProcessor const * ap = film->audio_processor();
	if (ap && (ap->id() == "stereo-5.1-upmix-a" || ap->id() == "stereo-5.1-upmix-b")) {
		hint (_("You are using DCP-o-matic's stereo-to-5.1 upmixer.  This is experimental and may result in poor-quality audio.  If you continue, you should listen to the resulting DCP in a cinema to make sure that it sounds good."));
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
		hint (_("All of your content is in Scope (2.39:1) but your DCP's container is Flat (1.85:1).  This will letter-box your content inside a Flat (1.85:1) frame.  You may prefer to set your DCP's container to Scope (2.39:1) in the \"DCP\" tab."));
	}

	if (!scope && flat_or_narrower && film_container == "239") {
		hint (_("All of your content is at 1.85:1 or narrower but your DCP's container is Scope (2.39:1).  This will pillar-box your content inside a Flat (1.85:1) frame.  You may prefer to set your DCP's container to Flat (1.85:1) in the \"DCP\" tab."));
	}

	if (film_container != "185" && film_container != "239" && film_container != "190") {
		hint (_("Your DCP uses an unusual container ratio.  This may cause problems on some projectors.  If possible, use Flat or Scope for the DCP container ratio"));
	}

	if (film->j2k_bandwidth() >= 245000000) {
		hint (_("A few projectors have problems playing back very high bit-rate DCPs.  It is a good idea to drop the JPEG2000 bandwidth down to about 200Mbit/s; this is unlikely to have any visible effect on the image."));
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

		hint (h);
	}

	optional<double> lowest_speed_up;
	optional<double> highest_speed_up;
	BOOST_FOREACH (shared_ptr<const Content> i, content) {
		double spu = film->active_frame_rate_change(i->position()).speed_up;
		if (!lowest_speed_up || spu < *lowest_speed_up) {
			lowest_speed_up = spu;
		}
		if (!highest_speed_up || spu > *highest_speed_up) {
			highest_speed_up = spu;
		}
	}

	double worst_speed_up = 1;
	if (highest_speed_up) {
		worst_speed_up = *highest_speed_up;
	}
	if (lowest_speed_up) {
		worst_speed_up = max (worst_speed_up, 1 / *lowest_speed_up);
	}

	if (worst_speed_up > 25.5/24.0) {
		hint (_("There is a large difference between the frame rate of your DCP and that of some of your content.  This will cause your audio to play back at a much lower or higher pitch than it should.  You are advised to set your DCP frame rate to one closer to your content, provided that your target projection systems support your chosen DCP rate."));
	}

	int vob = 0;
	BOOST_FOREACH (shared_ptr<const Content> i, content) {
		if (boost::algorithm::starts_with (i->path(0).filename().string(), "VTS_")) {
			++vob;
		}
	}

	if (vob > 1) {
		hint (String::compose (_("You have %1 files that look like they are VOB files from DVD. You should join them to ensure smooth joins between the files."), vob));
	}

	int three_d = 0;
	BOOST_FOREACH (shared_ptr<const Content> i, content) {
		if (i->video && i->video->frame_type() != VIDEO_FRAME_TYPE_2D) {
			++three_d;
		}
	}

	if (three_d > 0 && !film->three_d()) {
		hint (_("You are using 3D content but your DCP is set to 2D.  Set the DCP to 3D if you want to play it back on a 3D system (e.g. Real-D, MasterImage etc.)"));
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
				hint (String::compose (
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

	emit (bind(boost::ref(Progress), _("Examining closed captions")));

	shared_ptr<Player> player (new Player (film, film->playlist ()));
	player->set_ignore_video ();
	player->set_ignore_audio ();
	player->Text.connect (bind(&Hints::text, this, _1, _2, _3));
	while (!player->pass ()) {
		bind (boost::ref(Pulse));
	}

	emit (bind(boost::ref(Finished)));
}

void
Hints::hint (string h)
{
	emit(bind(boost::ref(Hint), h));
}

void
Hints::text (PlayerText text, TextType type, DCPTimePeriod period)
{
	if (type != TEXT_CLOSED_CAPTION) {
		return;
	}

	int lines = text.text.size();
	BOOST_FOREACH (StringText i, text.text) {
		if (i.text().length() > CLOSED_CAPTION_LENGTH) {
			++lines;
			if (!_long_ccap) {
				_long_ccap = true;
				hint (String::compose(_("Some of your closed captions have lines longer than %1 characters, so they will probably be word-wrapped."), CLOSED_CAPTION_LENGTH));
			}
		}
	}

	if (!_too_many_ccap_lines && lines > CLOSED_CAPTION_LINES) {
		hint (String::compose(_("Some of your closed captions span more than %1 lines, so they will be truncated."), CLOSED_CAPTION_LINES));
		_too_many_ccap_lines = true;
	}

	if (!_overlap_ccap && _last && _last->overlap(period)) {
		_overlap_ccap = true;
		hint (_("You have overlapping closed captions, which are not allowed."));
	}

	_last = period;
}
