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


#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <RtAudio.h>
LIBDCP_ENABLE_WARNINGS



class AudioBackend
{
public:
	AudioBackend();

	AudioBackend(AudioBackend const&) = delete;
	AudioBackend& operator=(AudioBackend const&) = delete;

	RtAudio& rtaudio() {
		return _rtaudio;
	}

#if (RTAUDIO_VERSION_MAJOR >= 6)
	std::string last_rtaudio_error() const;
#endif

	static AudioBackend* instance();

private:
	static AudioBackend* _instance;

	RtAudio _rtaudio;

#if (RTAUDIO_VERSION_MAJOR >= 6)
	void rtaudio_error_callback(std::string const& error);
	mutable boost::mutex _last_rtaudio_error_mutex;
	std::string _last_rtaudio_error;
#endif
};

