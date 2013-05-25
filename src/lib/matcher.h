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

#include <boost/optional.hpp>
#include "processor.h"

class Matcher : public Processor, public TimedAudioSink, public TimedVideoSink, public AudioSource, public VideoSource 
{
public:
	Matcher (boost::shared_ptr<Log> log, int sample_rate, float frames_per_second);
	void process_video (boost::shared_ptr<const Image> i, bool, boost::shared_ptr<Subtitle> s, double);
	void process_audio (boost::shared_ptr<const AudioBuffers>, double);
	void process_end ();

private:
	void fix_start ();
	void match (double);
	void repeat_last_video ();
	
	int _sample_rate;
	float _frames_per_second;
	int _video_frames;
	int64_t _audio_frames;
	boost::optional<AVPixelFormat> _pixel_format;
	boost::optional<libdcp::Size> _size;
	boost::optional<int> _channels;

	struct VideoRecord {
		VideoRecord (boost::shared_ptr<const Image> i, bool s, boost::shared_ptr<Subtitle> u, double t)
			: image (i)
			, same (s)
			, subtitle (u)
			, time (t)
		{}

		boost::shared_ptr<const Image> image;
		bool same;
		boost::shared_ptr<Subtitle> subtitle;
		double time;
	};

	struct AudioRecord {
		AudioRecord (boost::shared_ptr<const AudioBuffers> a, double t)
			: audio (a)
			, time (t)
		{}
		
		boost::shared_ptr<const AudioBuffers> audio;
		double time;
	};

	std::list<VideoRecord> _pending_video;
	std::list<AudioRecord> _pending_audio;

	boost::optional<double> _first_input;
	boost::shared_ptr<const Image> _last_image;
	boost::shared_ptr<Subtitle> _last_subtitle;

	bool _had_first_video;
	bool _had_first_audio;
};
