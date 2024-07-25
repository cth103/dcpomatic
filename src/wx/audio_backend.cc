/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "audio_backend.h"


AudioBackend* AudioBackend::_instance = nullptr;


#ifdef DCPOMATIC_LINUX
auto constexpr api = RtAudio::LINUX_PULSE;
#endif
#ifdef DCPOMATIC_WINDOWS
auto constexpr api = RtAudio::UNSPECIFIED;
#endif
#ifdef DCPOMATIC_OSX
auto constexpr api = RtAudio::MACOSX_CORE;
#endif


AudioBackend::AudioBackend()
#if (RTAUDIO_VERSION_MAJOR >= 6)
	: _rtaudio(api, boost::bind(&AudioBackend::rtaudio_error_callback, this, _2))
#else
	: _rtaudio(api)
#endif
{

}


#if (RTAUDIO_VERSION_MAJOR >= 6)
void
AudioBackend::rtaudio_error_callback(string const& error)
{
	boost::mutex::scoped_lock lm(_last_rtaudio_error_mutex);
	_last_rtaudio_error = error;
}

string
AudioBackend::last_rtaudio_error() const
{
	boost::mutex::scoped_lock lm(_last_rtaudio_error_mutex);
	return _last_rtaudio_error;
}
#endif


AudioBackend*
AudioBackend::instance()
{
	if (!_instance) {
		_instance = new AudioBackend();
	}

	return _instance;
}


void
AudioBackend::abort_stream_if_running()
{
	if (_rtaudio.isStreamRunning()) {
		_rtaudio.abortStream();
	}
}


optional<string>
AudioBackend::start_stream()
{
#if (RTAUDIO_VERSION_MAJOR >= 6)
	if (_rtaudio.startStream() != RTAUDIO_NO_ERROR) {
		return last_rtaudio_error();
	}
#else
	try {
		_rtaudio.startStream();
	} catch (RtAudioError& e) {
		return string(e.what());
	}
#endif

	return {};
}


