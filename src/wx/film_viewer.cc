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


/** @file  src/film_viewer.cc
 *  @brief A wx widget to view a preview of a Film.
 */


#include "audio_backend.h"
#include "closed_captions_dialog.h"
#include "film_viewer.h"
#include "gl_video_view.h"
#include "nag_dialog.h"
#include "playhead_to_frame_dialog.h"
#include "playhead_to_timecode_dialog.h"
#include "simple_video_view.h"
#include "wx_util.h"
#include "lib/butler.h"
#include "lib/compose.hpp"
#include "lib/config.h"
#include "lib/dcp_content.h"
#include "lib/dcpomatic_log.h"
#include "lib/examine_content_job.h"
#include "lib/exceptions.h"
#include "lib/film.h"
#include "lib/filter.h"
#include "lib/image.h"
#include "lib/job_manager.h"
#include "lib/log.h"
#include "lib/player.h"
#include "lib/player_video.h"
#include "lib/ratio.h"
#include "lib/text_content.h"
#include "lib/timer.h"
#include "lib/util.h"
#include "lib/video_content.h"
#include "lib/video_decoder.h"
#include <dcp/exceptions.h>
#include <dcp/warnings.h>
extern "C" {
#include <libavutil/pixfmt.h>
}
LIBDCP_DISABLE_WARNINGS
#include <wx/tglbtn.h>
LIBDCP_ENABLE_WARNINGS
#include <iomanip>


using std::bad_alloc;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::max;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::Size;
using namespace dcpomatic;


static
int
rtaudio_callback(void* out, void *, unsigned int frames, double, RtAudioStreamStatus, void* data)
{
	return reinterpret_cast<FilmViewer*>(data)->audio_callback(out, frames);
}


FilmViewer::FilmViewer(wxWindow* p)
	: _closed_captions_dialog(new ClosedCaptionsDialog(p, this))
{
#if wxCHECK_VERSION(3, 1, 0)
	switch (Config::instance()->video_view_type()) {
	case Config::VIDEO_VIEW_OPENGL:
		_video_view = std::make_shared<GLVideoView>(this, p);
		break;
	case Config::VIDEO_VIEW_SIMPLE:
		_video_view = std::make_shared<SimpleVideoView>(this, p);
		break;
	}
#else
	_video_view = std::make_shared<SimpleVideoView>(this, p);
#endif

	_video_view->Sized.connect(boost::bind(&FilmViewer::video_view_sized, this));
	_video_view->TooManyDropped.connect(boost::bind(boost::ref(TooManyDropped)));

	set_film(shared_ptr<Film>());

	_config_changed_connection = Config::instance()->Changed.connect(bind(&FilmViewer::config_changed, this, _1));
	config_changed(Config::SOUND_OUTPUT);
}


FilmViewer::~FilmViewer()
{
	stop();
}


/** Ask for ::idle_handler() to be called next time we are idle */
void
FilmViewer::request_idle_display_next_frame()
{
	if (_idle_get) {
		return;
	}

	_idle_get = true;
	DCPOMATIC_ASSERT(signal_manager);
	signal_manager->when_idle(boost::bind(&FilmViewer::idle_handler, this));
}


void
FilmViewer::idle_handler()
{
	if (!_idle_get) {
		return;
	}

	if (_video_view->display_next_frame(true) == VideoView::AGAIN) {
		/* get() could not complete quickly so we'll try again later */
		signal_manager->when_idle(boost::bind(&FilmViewer::idle_handler, this));
	} else {
		_idle_get = false;
	}
}


void
FilmViewer::set_film(shared_ptr<Film> film)
{
	if (_film == film) {
		return;
	}

	_film = film;

	_video_view->clear();
	_closed_captions_dialog->clear();

	destroy_butler();

	if (!_film) {
		_player = boost::none;
		resume();
		_video_view->update();
		return;
	}

	try {
		_player.emplace(_film, _optimisation == Optimisation::NONE ? Image::Alignment::PADDED : Image::Alignment::COMPACT, true);
		_player->set_fast();
		if (_dcp_decode_reduction) {
			_player->set_dcp_decode_reduction(_dcp_decode_reduction);
		}
	} catch (bad_alloc &) {
		error_dialog(_video_view->get(), _("There is not enough free memory to do that."));
		_film.reset();
		resume();
		return;
	}

	_player->set_always_burn_open_subtitles();
	_player->set_play_referenced();

	_film->Change.connect(boost::bind(&FilmViewer::film_change, this, _1, _2));
	_film->LengthChange.connect(boost::bind(&FilmViewer::film_length_change, this));
	_player->Change.connect(boost::bind(&FilmViewer::player_change, this, _1, _2, _3));

	film_change(ChangeType::DONE, FilmProperty::VIDEO_FRAME_RATE);
	film_change(ChangeType::DONE, FilmProperty::THREE_D);
	film_length_change();

	/* Keep about 1 second's worth of history samples */
	_latency_history_count = _film->audio_frame_rate() / _audio_block_size;

	_closed_captions_dialog->update_tracks(_film);

	create_butler();

	calculate_sizes();
	slow_refresh();
}


void
FilmViewer::destroy_butler()
{
	suspend();
	_butler.reset();
}


void
FilmViewer::destroy_and_maybe_create_butler()
{
	destroy_butler();

	if (!_film) {
		resume();
		return;
	}

	create_butler();
}


void
FilmViewer::create_butler()
{
#if wxCHECK_VERSION(3, 1, 0)
	auto const opengl = dynamic_pointer_cast<GLVideoView>(_video_view);
#else
	auto const opengl = false;
#endif

	DCPOMATIC_ASSERT(_player);
	_player->seek(_video_view->position(), true);

	auto& audio = AudioBackend::instance()->rtaudio();

	_butler = std::make_shared<Butler>(
		_film,
		*_player,
		Config::instance()->audio_mapping(_audio_channels),
		_audio_channels,
		AV_PIX_FMT_RGB24,
		VideoRange::FULL,
		(opengl && _optimisation != Optimisation::NONE) ? Image::Alignment::COMPACT : Image::Alignment::PADDED,
		true,
		opengl && _optimisation == Optimisation::JPEG2000,
		(Config::instance()->sound() && audio.isStreamOpen()) ? Butler::Audio::ENABLED : Butler::Audio::DISABLED
		);

	_closed_captions_dialog->set_butler(_butler);

	resume();
}


void
FilmViewer::set_outline_content(bool o)
{
	_outline_content = o;
	_video_view->update();
}


void
FilmViewer::set_outline_subtitles(optional<dcpomatic::Rect<double>> rect)
{
	_outline_subtitles = rect;
	_video_view->update();
}


void
FilmViewer::set_eyes(Eyes e)
{
	_video_view->set_eyes(e);
	slow_refresh();
}


void
FilmViewer::video_view_sized()
{
	calculate_sizes();
	if (!quick_refresh()) {
		slow_refresh();
	}
}


void
FilmViewer::calculate_sizes()
{
	if (!_film || !_player) {
		return;
	}

	auto const container = _film->container();

	auto const scale = dpi_scale_factor(_video_view->get());
	int const video_view_width = std::round(_video_view->get()->GetSize().x * scale);
	int const video_view_height = std::round(_video_view->get()->GetSize().y * scale);

	auto const view_ratio = float(video_view_width) / video_view_height;
	auto const film_ratio = container.ratio();

	dcp::Size out_size;
	if (view_ratio < film_ratio) {
		/* panel is less widscreen than the film; clamp width */
		out_size.width = video_view_width;
		out_size.height = lrintf(out_size.width / film_ratio);
	} else {
		/* panel is more widescreen than the film; clamp height */
		out_size.height = video_view_height;
		out_size.width = lrintf(out_size.height * film_ratio);
	}

	/* Catch silly values */
	out_size.width = max(64, out_size.width);
	out_size.height = max(64, out_size.height);

	/* Make sure the video container sizes are always a multiple of 2 so that
	 * we don't get gaps with subsampled sources (e.g. YUV420)
	 */
	if (out_size.width % 2) {
		out_size.width++;
	}
	if (out_size.height % 2) {
		out_size.height++;
	}

	_player->set_video_container_size(out_size);
}


void
FilmViewer::suspend()
{
	++_suspended;
	AudioBackend::instance()->abort_stream_if_running();
}


void
FilmViewer::start_audio_stream_if_open()
{
	auto& audio = AudioBackend::instance()->rtaudio();

	if (audio.isStreamOpen()) {
		audio.setStreamTime(_video_view->position().seconds());
		auto error = AudioBackend::instance()->start_stream();
		if (error) {
			_audio_channels = 0;
			error_dialog(
				_video_view->get(),
				_("There was a problem starting audio playback.  Please try another audio output device in Preferences."), std_to_wx(*error)
				);
		}
	}
}


void
FilmViewer::resume()
{
	DCPOMATIC_ASSERT(_suspended > 0);
	--_suspended;
	if (_playing && !_suspended) {
		start_audio_stream_if_open();
		_video_view->start();
	}
}


void
FilmViewer::start ()
{
	if (!_film || _playing) {
		return;
	}

	auto v = PlaybackPermitted();
	if (v && !*v) {
		/* Computer says no */
		return;
	}

	/* We are about to set up the audio stream from the position of the video view.
	   If there is `lazy' seek in progress we need to wait for it to go through so that
	   _video_view->position() gives us a sensible answer.
	 */
	while (_idle_get) {
		idle_handler();
	}

	/* Take the video view's idea of position as our `playhead' and start the
	   audio stream (which is the timing reference) there.
         */
	start_audio_stream_if_open();

	_playing = true;
	/* Calling start() below may directly result in Stopped being emitted, and if that
	 * happens we want it to come after the Started signal, so do that first.
	 */
	Started();
	_video_view->start();
}


bool
FilmViewer::stop()
{
	AudioBackend::instance()->abort_stream_if_running();

	if (!_playing) {
		return false;
	}

	_playing = false;
	_video_view->stop();
	Stopped();

	_video_view->rethrow();
	return true;
}


void
FilmViewer::player_change(ChangeType type, int property, bool frequent)
{
	if (type != ChangeType::DONE || frequent) {
		return;
	}

	if (_coalesce_player_changes) {
		_pending_player_changes.push_back(property);
		return;
	}

	player_change({property});
}


void
FilmViewer::player_change(vector<int> properties)
{
	calculate_sizes();

	bool try_quick_refresh = false;
	bool update_ccap_tracks = false;

	for (auto i: properties) {
		switch (i) {
		case VideoContentProperty::CROP:
		case VideoContentProperty::CUSTOM_RATIO:
		case VideoContentProperty::CUSTOM_SIZE:
		case VideoContentProperty::FADE_IN:
		case VideoContentProperty::FADE_OUT:
		case VideoContentProperty::COLOUR_CONVERSION:
		case PlayerProperty::VIDEO_CONTAINER_SIZE:
		case PlayerProperty::FILM_CONTAINER:
			try_quick_refresh = true;
			break;
		case TextContentProperty::USE:
		case TextContentProperty::TYPE:
		case TextContentProperty::DCP_TRACK:
			update_ccap_tracks = true;
			break;
		}
	}

	if (!try_quick_refresh || !quick_refresh()) {
		slow_refresh();
	}

	if (update_ccap_tracks) {
		_closed_captions_dialog->update_tracks(_film);
	}
}


void
FilmViewer::film_change(ChangeType type, FilmProperty p)
{
	if (type != ChangeType::DONE) {
		return;
	}

	if (p == FilmProperty::AUDIO_CHANNELS) {
		destroy_and_maybe_create_butler();
	} else if (p == FilmProperty::VIDEO_FRAME_RATE) {
		_video_view->set_video_frame_rate(_film->video_frame_rate());
	} else if (p == FilmProperty::THREE_D) {
		_video_view->set_three_d(_film->three_d());
	} else if (p == FilmProperty::CONTENT) {
		_closed_captions_dialog->update_tracks(_film);
	}
}


void
FilmViewer::film_length_change()
{
	_video_view->set_length(_film->length());
}


/** Re-get the current frame slowly by seeking */
void
FilmViewer::slow_refresh()
{
	seek(_video_view->position(), true);
}


/** Try to re-get the current frame quickly by resetting the metadata
 *  in the PlayerVideo that we used last time.
 *  @return true if this was possible, false if not.
 */
bool
FilmViewer::quick_refresh()
{
	if (!_video_view || !_film || !_player) {
		return true;
	}
	return _video_view->reset_metadata(_film, _player->video_container_size());
}


void
FilmViewer::seek(shared_ptr<Content> content, ContentTime t, bool accurate)
{
	DCPOMATIC_ASSERT(_player);
	auto dt = _player->content_time_to_dcp(content, t);
	if (dt) {
		seek(*dt, accurate);
	}
}


void
FilmViewer::set_coalesce_player_changes(bool c)
{
	_coalesce_player_changes = c;

	if (!c) {
		player_change(_pending_player_changes);
		_pending_player_changes.clear();
	}
}


void
FilmViewer::seek(DCPTime t, bool accurate)
{
	if (!_butler) {
		return;
	}

	if (t < DCPTime()) {
		t = DCPTime();
	}

	if (t >= _film->length()) {
		t = _film->length() - one_video_frame();
	}

	suspend();

	_closed_captions_dialog->clear();
	_butler->seek(t, accurate);

	if (!_playing) {
		/* We're not playing, so let the GUI thread get on and
		   come back later to get the next frame after the seek.
		*/
		request_idle_display_next_frame();
	} else {
		/* We're going to start playing again straight away
		   so wait for the seek to finish.
		*/
		while (_video_view->display_next_frame(false) == VideoView::AGAIN) {}
	}

	resume();
}


void
FilmViewer::config_changed(Config::Property p)
{
	if (p == Config::AUDIO_MAPPING) {
		destroy_and_maybe_create_butler();
		return;
	}

	if (p != Config::SOUND && p != Config::SOUND_OUTPUT) {
		return;
	}

	auto backend = AudioBackend::instance();
	auto& audio = backend->rtaudio();

	if (audio.isStreamOpen()) {
		audio.closeStream();
	}

	if (Config::instance()->sound() && audio.getDeviceCount() > 0) {
		optional<unsigned int> chosen_device_id;
#if (RTAUDIO_VERSION_MAJOR >= 6)
		if (Config::instance()->sound_output()) {
			for (auto device_id: audio.getDeviceIds()) {
				if (audio.getDeviceInfo(device_id).name == Config::instance()->sound_output().get()) {
					chosen_device_id = device_id;
					break;
				}
			}
		}

		if (!chosen_device_id) {
			chosen_device_id = audio.getDefaultOutputDevice();
		}
		_audio_channels = audio.getDeviceInfo(*chosen_device_id).outputChannels;
		RtAudio::StreamParameters sp;
		sp.deviceId = *chosen_device_id;
		sp.nChannels = _audio_channels;
		sp.firstChannel = 0;
		if (audio.openStream(&sp, 0, RTAUDIO_FLOAT32, 48000, &_audio_block_size, &rtaudio_callback, this) != RTAUDIO_NO_ERROR) {
			_audio_channels = 0;
			error_dialog(
				_video_view->get(),
				_("Could not set up audio output.  There will be no audio during the preview."), std_to_wx(backend->last_rtaudio_error())
				);
		}
#else
		unsigned int st = 0;
		if (Config::instance()->sound_output()) {
			while (st < audio.getDeviceCount()) {
				try {
					if (audio.getDeviceInfo(st).name == Config::instance()->sound_output().get()) {
						break;
					}
				} catch (RtAudioError&) {
					/* Something went wrong with that device so we don't want to use it anyway */
				}
				++st;
			}
			if (st == audio.getDeviceCount()) {
				try {
					st = audio.getDefaultOutputDevice();
				} catch (RtAudioError&) {
					/* Something went wrong with that device so we don't want to use it anyway */
				}
			}
		} else {
			try {
				st = audio.getDefaultOutputDevice();
			} catch (RtAudioError&) {
				/* Something went wrong with that device so we don't want to use it anyway */
			}
		}

		try {
			_audio_channels = audio.getDeviceInfo(st).outputChannels;
			RtAudio::StreamParameters sp;
			sp.deviceId = st;
			sp.nChannels = _audio_channels;
			sp.firstChannel = 0;
			audio.openStream(&sp, 0, RTAUDIO_FLOAT32, 48000, &_audio_block_size, &rtaudio_callback, this);
		} catch (RtAudioError& e) {
			_audio_channels = 0;
			error_dialog(
				_video_view->get(),
				_("Could not set up audio output.  There will be no audio during the preview."), std_to_wx(e.what())
				);
		}
#endif
		destroy_and_maybe_create_butler();

	} else {
		_audio_channels = 0;
		destroy_and_maybe_create_butler();
	}
}


DCPTime
FilmViewer::uncorrected_time() const
{
	auto& audio = AudioBackend::instance()->rtaudio();

	if (audio.isStreamRunning()) {
		return DCPTime::from_seconds(audio.getStreamTime());
	}

	return _video_view->position();
}


optional<DCPTime>
FilmViewer::audio_time() const
{
	auto& audio = AudioBackend::instance()->rtaudio();

	if (!audio.isStreamRunning()) {
		return {};
	}

	return DCPTime::from_seconds(audio.getStreamTime()) -
		DCPTime::from_frames(average_latency(), _film->audio_frame_rate());
}


DCPTime
FilmViewer::time() const
{
	return audio_time().get_value_or(_video_view->position());
}


int
FilmViewer::audio_callback(void* out_p, unsigned int frames)
{
	while (true) {
		auto t = _butler->get_audio(Butler::Behaviour::NON_BLOCKING, reinterpret_cast<float*>(out_p), frames);
		if (!t || DCPTime(uncorrected_time() - *t) < one_video_frame()) {
			/* There was an underrun or this audio is on time; carry on */
			break;
		}
		/* The audio we just got was (very) late; drop it and get some more. */
	}

	auto& audio = AudioBackend::instance()->rtaudio();

	boost::mutex::scoped_lock lm(_latency_history_mutex, boost::try_to_lock);
	if (lm) {
		_latency_history.push_back(audio.getStreamLatency());
		if (_latency_history.size() > static_cast<size_t>(_latency_history_count)) {
			_latency_history.pop_front();
		}
	}

	return 0;
}


Frame
FilmViewer::average_latency() const
{
        boost::mutex::scoped_lock lm(_latency_history_mutex);
        if (_latency_history.empty()) {
                return 0;
        }

        Frame total = 0;
        for (auto i: _latency_history) {
                total += i;
        }

        return total / _latency_history.size();
}


void
FilmViewer::set_dcp_decode_reduction(optional<int> reduction)
{
	_dcp_decode_reduction = reduction;
	if (_player) {
		_player->set_dcp_decode_reduction(reduction);
	}
}


optional<int>
FilmViewer::dcp_decode_reduction() const
{
	return _dcp_decode_reduction;
}


optional<ContentTime>
FilmViewer::position_in_content(shared_ptr<const Content> content) const
{
	DCPOMATIC_ASSERT(_player);
	return _player->dcp_to_content_time(content, position());
}


DCPTime
FilmViewer::one_video_frame() const
{
	return DCPTime::from_frames(1, _film ? _film->video_frame_rate() : 24);
}


/** Open a dialog box showing our film's closed captions */
void
FilmViewer::show_closed_captions()
{
	_closed_captions_dialog->Show();
}


void
FilmViewer::seek_by(DCPTime by, bool accurate)
{
	seek(_video_view->position() + by, accurate);
}


void
FilmViewer::set_pad_black(bool p)
{
	_pad_black = p;
}


/** Called when a player has finished the current film.
 *  May be called from a non-UI thread.
 */
void
FilmViewer::finished()
{
	emit(boost::bind(&FilmViewer::ui_finished, this));
}


/** Called by finished() in the UI thread */
void
FilmViewer::ui_finished()
{
	stop();
	Finished();
}


int
FilmViewer::dropped() const
{
	return _video_view->dropped();
}


int
FilmViewer::errored() const
{
	return _video_view->errored();
}


int
FilmViewer::gets() const
{
	return _video_view->gets();
}


void
FilmViewer::image_changed(shared_ptr<PlayerVideo> pv)
{
	_last_image = pv;
	emit(boost::bind(boost::ref(ImageChanged)));
}


shared_ptr<const PlayerVideo>
FilmViewer::last_image() const
{
	return _last_image;
}


void
FilmViewer::set_optimisation(Optimisation o)
{
	_optimisation = o;
	_video_view->set_optimisation(o);
	destroy_and_maybe_create_butler();
}


void
FilmViewer::set_crop_guess(dcpomatic::Rect<float> crop)
{
	if (crop != _crop_guess) {
		_crop_guess = crop;
		_video_view->update();
	}
}


void
FilmViewer::unset_crop_guess()
{
	_crop_guess = boost::none;
	_video_view->update();
}

shared_ptr<DCPContent>
FilmViewer::dcp() const
{
	if (_film) {
		auto content = _film->content();
		if (content.size() == 1) {
			return dynamic_pointer_cast<DCPContent>(content.front());
		}
	}

	return {};
}
