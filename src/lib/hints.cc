/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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

#include "dcp_content_type.h"
#include "hints.h"
#include "types.h"
#include "film.h"
#include "content.h"
#include "video_content.h"
#include "text_content.h"
#include "audio_processor.h"
#include "font.h"
#include "font_data.h"
#include "ratio.h"
#include "audio_analysis.h"
#include "compose.hpp"
#include "util.h"
#include "cross.h"
#include "player.h"
#include "writer.h"
#include <dcp/cpl.h>
#include <dcp/raw_convert.h>
#include <dcp/reel.h>
#include <dcp/reel_closed_caption_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <boost/algorithm/string.hpp>
#include <iostream>

#include "i18n.h"

using std::cout;
using std::make_shared;
using std::max;
using std::min;
using std::pair;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;
using boost::optional;
using boost::bind;
using namespace dcpomatic;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


/* When checking to see if things are too big, we'll say they are if they
 * are more than the target size minus this "slack."
 */
#define SIZE_SLACK 4096


/* When writing hints:
 * - put quotation marks around the name of a GUI tab that you are referring to (e.g. "DCP" or "DCP→Video" tab)
 */


Hints::Hints (weak_ptr<const Film> film)
	: WeakConstFilm (film)
	, _writer (new Writer(film, weak_ptr<Job>(), true))
	, _long_ccap (false)
	, _overlap_ccap (false)
	, _too_many_ccap_lines (false)
	, _early_subtitle (false)
	, _short_subtitle (false)
	, _subtitles_too_close (false)
	, _too_many_subtitle_lines (false)
	, _long_subtitle (false)
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
Hints::check_few_audio_channels ()
{
	if (film()->audio_channels() < 6) {
		hint (_("Your DCP has fewer than 6 audio channels.  This may cause problems on some projectors.  You may want to set the DCP to have 6 channels.  It does not matter if your content has fewer channels, as DCP-o-matic will fill the extras with silence."));
	}
}


void
Hints::check_upmixers ()
{
	auto ap = film()->audio_processor();
	if (ap && (ap->id() == "stereo-5.1-upmix-a" || ap->id() == "stereo-5.1-upmix-b")) {
		hint (_("You are using DCP-o-matic's stereo-to-5.1 upmixer.  This is experimental and may result in poor-quality audio.  If you continue, you should listen to the resulting DCP in a cinema to make sure that it sounds good."));
	}
}


void
Hints::check_incorrect_container ()
{
	int narrower_than_scope = 0;
	int scope = 0;
	for (auto i: film()->content()) {
		if (i->video) {
			Ratio const * r = Ratio::nearest_from_ratio(i->video->scaled_size(film()->frame_size()).ratio());
			if (r && r->id() == "239") {
				++scope;
			} else if (r && r->id() != "239" && r->id() != "235" && r->id() != "190") {
				++narrower_than_scope;
			}
		}
	}

	string const film_container = film()->container()->id();

	if (scope && !narrower_than_scope && film_container == "185") {
		hint (_("All of your content is in Scope (2.39:1) but your DCP's container is Flat (1.85:1).  This will letter-box your content inside a Flat (1.85:1) frame.  You may prefer to set your DCP's container to Scope (2.39:1) in the \"DCP\" tab."));
	}

	if (!scope && narrower_than_scope && film_container == "239") {
		hint (_("All of your content narrower than 1.90:1 but your DCP's container is Scope (2.39:1).  This will pillar-box your content.  You may prefer to set your DCP's container to have the same ratio as your content."));
	}
}


void
Hints::check_unusual_container ()
{
	auto const film_container = film()->container()->id();
	if (film_container != "185" && film_container != "239") {
		hint (_("Your DCP uses an unusual container ratio.  This may cause problems on some projectors.  If possible, use Flat or Scope for the DCP container ratio"));
	}
}


void
Hints::check_high_j2k_bandwidth ()
{
	if (film()->j2k_bandwidth() >= 245000000) {
		hint (_("A few projectors have problems playing back very high bit-rate DCPs.  It is a good idea to drop the JPEG2000 bandwidth down to about 200Mbit/s; this is unlikely to have any visible effect on the image."));
	}
}


void
Hints::check_frame_rate ()
{
	auto f = film ();
	switch (f->video_frame_rate()) {
	case 24:
		/* Fine */
		break;
	case 25:
	{
		/* You might want to go to 24 */
		string base = String::compose(_("You are set up for a DCP at a frame rate of %1 fps.  This frame rate is not supported by all projectors.  You may want to consider changing your frame rate to %2 fps."), 25, 24);
		if (f->interop()) {
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
		hint (String::compose(_("You are set up for a DCP at a frame rate of %1 fps.  This frame rate is not supported by all projectors.  You are advised to change the DCP frame rate to %2 fps."), f->video_frame_rate(), f->video_frame_rate() / 2));
		break;
	}
}


void
Hints::check_4k_3d ()
{
	auto f = film();
	if (f->resolution() == Resolution::FOUR_K && f->three_d()) {
		hint (_("4K 3D is only supported by a very limited number of projectors.  Unless you know that you will play this DCP back on a capable projector, it is advisable to set the DCP to be 2K in the \"DCP→Video\" tab."));
	}
}


void
Hints::check_speed_up ()
{
	optional<double> lowest_speed_up;
	optional<double> highest_speed_up;
	for (auto i: film()->content()) {
		double spu = film()->active_frame_rate_change(i->position()).speed_up;
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

}


void
Hints::check_interop ()
{
	if (film()->interop()) {
		hint (_("In general it is now advisable to make SMPTE DCPs unless you have a particular reason to use Interop.  You are advised to set your DCP to use the SMPTE standard in the \"DCP\" tab."));
	}
}


void
Hints::check_big_font_files ()
{
	bool big_font_files = false;
	if (film()->interop ()) {
		for (auto i: film()->content()) {
			for (auto j: i->text) {
				for (auto k: j->fonts()) {
					optional<boost::filesystem::path> const p = k->file ();
					if (p && boost::filesystem::file_size(p.get()) >= (MAX_FONT_FILE_SIZE - SIZE_SLACK)) {
						big_font_files = true;
					}
				}
			}
		}
	}

	if (big_font_files) {
		hint (_("You have specified a font file which is larger than 640kB.  This is very likely to cause problems on playback."));
	}
}


void
Hints::check_vob ()
{
	int vob = 0;
	for (auto i: film()->content()) {
		if (boost::algorithm::starts_with (i->path(0).filename().string(), "VTS_")) {
			++vob;
		}
	}

	if (vob > 1) {
		hint (String::compose (_("You have %1 files that look like they are VOB files from DVD. You should join them to ensure smooth joins between the files."), vob));
	}
}


void
Hints::check_3d_in_2d ()
{
	int three_d = 0;
	for (auto i: film()->content()) {
		if (i->video && i->video->frame_type() != VideoFrameType::TWO_D) {
			++three_d;
		}
	}

	if (three_d > 0 && !film()->three_d()) {
		hint (_("You are using 3D content but your DCP is set to 2D.  Set the DCP to 3D if you want to play it back on a 3D system (e.g. Real-D, MasterImage etc.)"));
	}
}


void
Hints::check_loudness ()
{
	auto path = film()->audio_analysis_path(film()->playlist());
	if (boost::filesystem::exists (path)) {
		try {
			auto an = make_shared<AudioAnalysis>(path);

			string ch;

			auto sample_peak = an->sample_peak ();
			auto true_peak = an->true_peak ();

			for (size_t i = 0; i < sample_peak.size(); ++i) {
				float const peak = max (sample_peak[i].peak, true_peak.empty() ? 0 : true_peak[i]);
				float const peak_dB = linear_to_db(peak) + an->gain_correction(film()->playlist());
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
}


static
bool
subtitle_mxf_too_big (shared_ptr<dcp::SubtitleAsset> asset)
{
	return asset && asset->file() && boost::filesystem::file_size(*asset->file()) >= (MAX_TEXT_MXF_SIZE - SIZE_SLACK);
}


void
Hints::check_out_of_range_markers ()
{
	auto const length = film()->length();
	for (auto const& i: film()->markers()) {
		if (i.second >= length) {
			hint (_("At least one marker comes after the end of the project and will be ignored."));
		}
	}
}


void
Hints::thread ()
{
	auto film = _film.lock ();
	if (!film) {
		return;
	}

	auto content = film->content ();

	check_interop ();
	check_big_font_files ();
	check_few_audio_channels ();
	check_upmixers ();
	check_incorrect_container ();
	check_unusual_container ();
	check_high_j2k_bandwidth ();
	check_frame_rate ();
	check_4k_3d ();
	check_speed_up ();
	check_vob ();
	check_3d_in_2d ();
	check_loudness ();
	check_ffec_and_ffmc_in_smpte_feature ();
	check_out_of_range_markers ();
	check_text_languages ();

	emit (bind(boost::ref(Progress), _("Examining closed captions")));

	auto player = make_shared<Player>(film);
	player->set_ignore_video ();
	player->set_ignore_audio ();
	player->Text.connect (bind(&Hints::text, this, _1, _2, _3, _4));

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

	_writer->write (player->get_subtitle_fonts());

	bool ccap_xml_too_big = false;
	bool ccap_mxf_too_big = false;
	bool subs_mxf_too_big = false;

	boost::filesystem::path dcp_dir = film->dir("hints") / dcpomatic::get_process_id();
	boost::filesystem::remove_all (dcp_dir);

	try {
		_writer->finish (film->dir("hints") / dcpomatic::get_process_id());
	} catch (...) {
		store_current ();
		emit (bind(boost::ref(Finished)));
		return;
	}

	dcp::DCP dcp (dcp_dir);
	dcp.read ();
	DCPOMATIC_ASSERT (dcp.cpls().size() == 1);
	for (auto reel: dcp.cpls()[0]->reels()) {
		for (auto ccap: reel->closed_captions()) {
			if (ccap->asset() && ccap->asset()->xml_as_string().length() > static_cast<size_t>(MAX_CLOSED_CAPTION_XML_SIZE - SIZE_SLACK) && !ccap_xml_too_big) {
				hint (_(
						"At least one of your closed caption files' XML part is larger than " MAX_CLOSED_CAPTION_XML_SIZE_TEXT
						".  You should divide the DCP into shorter reels."
				       ));
				ccap_xml_too_big = true;
			}
			if (subtitle_mxf_too_big(ccap->asset()) && !ccap_mxf_too_big) {
				hint (_(
						"At least one of your closed caption files is larger than " MAX_TEXT_MXF_SIZE_TEXT
						" in total.  You should divide the DCP into shorter reels."
				       ));
				ccap_mxf_too_big = true;
			}
		}
		if (reel->main_subtitle() && subtitle_mxf_too_big(reel->main_subtitle()->asset()) && !subs_mxf_too_big) {
			hint (_(
					"At least one of your subtitle files is larger than " MAX_TEXT_MXF_SIZE_TEXT " in total.  "
					"You should divide the DCP into shorter reels."
			       ));
			subs_mxf_too_big = true;
		}
	}
	boost::filesystem::remove_all (dcp_dir);

	emit (bind(boost::ref(Finished)));
}

void
Hints::hint (string h)
{
	emit(bind(boost::ref(Hint), h));
}

void
Hints::text (PlayerText text, TextType type, optional<DCPTextTrack> track, DCPTimePeriod period)
{
	_writer->write (text, type, track, period);

	switch (type) {
	case TextType::CLOSED_CAPTION:
		closed_caption (text, period);
		break;
	case TextType::OPEN_SUBTITLE:
		open_subtitle (text, period);
		break;
	default:
		break;
	}
}


void
Hints::closed_caption (PlayerText text, DCPTimePeriod period)
{
	int lines = text.string.size();
	for (auto i: text.string) {
		if (utf8_strlen(i.text()) > MAX_CLOSED_CAPTION_LENGTH) {
			++lines;
			if (!_long_ccap) {
				_long_ccap = true;
				hint (
					String::compose(
						"At least one of your closed caption lines has more than %1 characters.  "
						"It is advisable to make each line %1 characters at most in length.",
						MAX_CLOSED_CAPTION_LENGTH,
						MAX_CLOSED_CAPTION_LENGTH)
				     );
			}
		}
	}

	if (!_too_many_ccap_lines && lines > MAX_CLOSED_CAPTION_LINES) {
		hint (String::compose(_("Some of your closed captions span more than %1 lines, so they will be truncated."), MAX_CLOSED_CAPTION_LINES));
		_too_many_ccap_lines = true;
	}

	/* XXX: maybe overlapping closed captions (i.e. different languages) are OK with Interop? */
	if (film()->interop() && !_overlap_ccap && _last_ccap && _last_ccap->overlap(period)) {
		_overlap_ccap = true;
		hint (_("You have overlapping closed captions, which are not allowed in Interop DCPs.  Change your DCP standard to SMPTE."));
	}

	_last_ccap = period;
}


void
Hints::open_subtitle (PlayerText text, DCPTimePeriod period)
{
	if (period.from < DCPTime::from_seconds(4) && !_early_subtitle) {
		_early_subtitle = true;
		hint (_("It is advisable to put your first subtitle at least 4 seconds after the start of the DCP to make sure it is seen."));
	}

	int const vfr = film()->video_frame_rate ();

	if (period.duration().frames_round(vfr) < 15 && !_short_subtitle) {
		_short_subtitle = true;
		hint (_("At least one of your subtitles lasts less than 15 frames.  It is advisable to make each subtitle at least 15 frames long."));
	}

	if (_last_subtitle && DCPTime(period.from - _last_subtitle->to).frames_round(vfr) < 2 && !_subtitles_too_close) {
		_subtitles_too_close = true;
		hint (_("At least one of your subtitles starts less than 2 frames after the previous one.  It is advisable to make the gap between subtitles at least 2 frames."));
	}

	if (text.string.size() > 3 && !_too_many_subtitle_lines) {
		_too_many_subtitle_lines = true;
		hint (_("At least one of your subtitles has more than 3 lines.  It is advisable to use no more than 3 lines."));
	}

	size_t longest_line = 0;
	for (auto const& i: text.string) {
		longest_line = max (longest_line, i.text().length());
	}

	if (longest_line > 52 && !_long_subtitle) {
		_long_subtitle = true;
		hint (_("At least one of your subtitle lines has more than 52 characters.  It is advisable to make each line 52 characters at most in length."));
	}

	_last_subtitle = period;
}


void
Hints::check_ffec_and_ffmc_in_smpte_feature ()
{
	auto f = film();
	if (!f->interop() && f->dcp_content_type()->libdcp_kind() == dcp::ContentKind::FEATURE && (!f->marker(dcp::Marker::FFEC) || !f->marker(dcp::Marker::FFMC))) {
		hint (_("SMPTE DCPs with the type FTR (feature) should have markers for the first frame of end credits (FFEC) and the first frame of moving credits (FFMC).  You should add these markers using the 'Markers' button in the \"DCP\" tab."));
	}
}


void
Hints::join ()
{
	_thread.join ();
}


void
Hints::check_text_languages ()
{
	for (auto i: film()->content()) {
		for (auto j: i->text) {
			if (j->use() && !j->language()) {
				hint (_("At least one piece of subtitle or closed caption content has no specified language.  "
					"It is advisable to set the language for each piece of subtitle or closed caption content in the \"Content→Timed Text\" tab"));
				return;
			}
		}
	}
}
