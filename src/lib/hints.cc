/*
    Copyright (C) 2016-2019 Carl Hetherington <cth@carlh.net>

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
using namespace dcpomatic;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif

Hints::Hints (weak_ptr<const Film> film)
	: _film (film)
	, _long_ccap (false)
	, _overlap_ccap (false)
	, _too_many_ccap_lines (false)
	, _stop (false)
{

}

void
Hints::start ()
{
	_thread = boost::thread (bind(&Hints::thread, this));
}

Hints::~Hints ()
{
	boost::this_thread::disable_interruption dis;

	try {
		_stop = true;
		_thread.interrupt ();
		_thread.join ();
	} catch (...) {}
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
					optional<boost::filesystem::path> const p = k->file ();
					if (p && boost::filesystem::file_size(p.get()) >= (640 * 1024)) {
						big_font_files = true;
					}
				}
			}
		}
	}

	if (big_font_files) {
		hint (_("You have specified a font file which is larger than 640kB.  This is very likely to cause problems on playback."));
	}

	if (film->audio_channels() < 6) {
		hint (_("Your DCP has fewer than 6 audio channels.  This may cause problems on some projectors.  You may want to set the DCP to have 6 channels.  It does not matter if your content has fewer channels, as DCP-o-matic will fill the extras with silence."));
	}

	AudioProcessor const * ap = film->audio_processor();
	if (ap && (ap->id() == "stereo-5.1-upmix-a" || ap->id() == "stereo-5.1-upmix-b")) {
		hint (_("You are using DCP-o-matic's stereo-to-5.1 upmixer.  This is experimental and may result in poor-quality audio.  If you continue, you should listen to the resulting DCP in a cinema to make sure that it sounds good."));
	}

	int narrower_than_scope = 0;
	int scope = 0;
	BOOST_FOREACH (shared_ptr<const Content> i, content) {
		if (i->video) {
			Ratio const * r = Ratio::nearest_from_ratio(i->video->scaled_size(film->frame_size()).ratio());
			if (r && r->id() == "239") {
				++scope;
			} else if (r && r->id() != "239" && r->id() != "235" && r->id() != "190") {
				++narrower_than_scope;
			}
		}
	}

	string const film_container = film->container()->id();

	if (scope && !narrower_than_scope && film_container == "185") {
		hint (_("All of your content is in Scope (2.39:1) but your DCP's container is Flat (1.85:1).  This will letter-box your content inside a Flat (1.85:1) frame.  You may prefer to set your DCP's container to Scope (2.39:1) in the \"DCP\" tab."));
	}

	if (!scope && narrower_than_scope && film_container == "239") {
		hint (_("All of your content narrower than 1.90:1 but your DCP's container is Scope (2.39:1).  This will pillar-box your content.  You may prefer to set your DCP's container to have the same ratio as your content."));
	}

	if (film_container != "185" && film_container != "239" && film_container != "190") {
		hint (_("Your DCP uses an unusual container ratio.  This may cause problems on some projectors.  If possible, use Flat or Scope for the DCP container ratio"));
	}

	if (film->j2k_bandwidth() >= 245000000) {
		hint (_("A few projectors have problems playing back very high bit-rate DCPs.  It is a good idea to drop the JPEG2000 bandwidth down to about 200Mbit/s; this is unlikely to have any visible effect on the image."));
	}

	switch (film->video_frame_rate()) {
	case 24:
		/* Fine */
		break;
	case 25:
	{
		/* You might want to go to 24 */
		string base = String::compose(_("You are set up for a DCP at a frame rate of %1 fps.  This frame rate is not supported by all projectors.  You may want to consider changing your frame rate to %2 fps."), 25, 24);
		if (film->interop()) {
			base += "  ";
			base += _("If you do use 25fps you should change your DCP standard to SMPTE.");
		}
		hint (base);
		break;
	}
	case 30:
		/* 30fps: we can't really offer any decent solutions */
		hint (_("You are set up for a DCP frame rate of 30fps, which is not supported by all projectors.  Be aware that you may have compatibility problems."));
		break;
	case 48:
	case 50:
	case 60:
		/* You almost certainly want to go to half frame rate */
		hint (String::compose(_("You are set up for a DCP at a frame rate of %1 fps.  This frame rate is not supported by all projectors.  You are advised to change the DCP frame rate to %2 fps."), film->video_frame_rate(), film->video_frame_rate() / 2));
		break;
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
				float const peak_dB = linear_to_db(peak) + an->gain_correction(film->playlist());
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

	shared_ptr<Player> player (new Player(film));
	player->set_ignore_video ();
	player->set_ignore_audio ();
	player->Text.connect (bind(&Hints::text, this, _1, _2, _4));

	struct timeval last_pulse;
	gettimeofday (&last_pulse, 0);

	try {
		while (!player->pass()) {

			struct timeval now;
			gettimeofday (&now, 0);
			if ((seconds(now) - seconds(last_pulse)) > 1) {
				if (_stop) {
					break;
				}
				emit (bind (boost::ref(Pulse)));
				last_pulse = now;
			}
		}
	} catch (...) {
		store_current ();
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

	int lines = text.string.size();
	BOOST_FOREACH (StringText i, text.string) {
		if (utf8_strlen(i.text()) > CLOSED_CAPTION_LENGTH) {
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

	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	/* XXX: maybe overlapping closed captions (i.e. different languages) are OK with Interop? */
	if (film->interop() && !_overlap_ccap && _last && _last->overlap(period)) {
		_overlap_ccap = true;
		hint (_("You have overlapping closed captions, which are not allowed in Interop DCPs.  Change your DCP standard to SMPTE."));
	}

	_last = period;
}
