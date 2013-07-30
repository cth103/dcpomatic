/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_FFMPEG_CONTENT_H
#define DCPOMATIC_FFMPEG_CONTENT_H

#include <boost/enable_shared_from_this.hpp>
#include "video_content.h"
#include "audio_content.h"
#include "subtitle_content.h"
#include "audio_mapping.h"

class Filter;
class ffmpeg_pts_offset_test;

class FFmpegAudioStream
{
public:
	FFmpegAudioStream (std::string n, int i, int f, int c)
		: name (n)
		, id (i)
		, frame_rate (f)
		, channels (c)
		, mapping (c)
	{
		mapping.make_default ();
	}

	FFmpegAudioStream (boost::shared_ptr<const cxml::Node>);

	void as_xml (xmlpp::Node *) const;
	
	std::string name;
	int id;
	int frame_rate;
	int channels;
	AudioMapping mapping;
	boost::optional<double> first_audio;

private:
	friend class ffmpeg_pts_offset_test;

	/* Constructor for tests */
	FFmpegAudioStream ()
		: mapping (1)
	{}
};

extern bool operator== (FFmpegAudioStream const & a, FFmpegAudioStream const & b);

class FFmpegSubtitleStream
{
public:
	FFmpegSubtitleStream (std::string n, int i)
		: name (n)
		, id (i)
	{}
	
	FFmpegSubtitleStream (boost::shared_ptr<const cxml::Node>);

	void as_xml (xmlpp::Node *) const;
	
	std::string name;
	int id;
};

extern bool operator== (FFmpegSubtitleStream const & a, FFmpegSubtitleStream const & b);

class FFmpegContentProperty : public VideoContentProperty
{
public:
	static int const SUBTITLE_STREAMS;
	static int const SUBTITLE_STREAM;
	static int const AUDIO_STREAMS;
	static int const AUDIO_STREAM;
	static int const FILTERS;
};

class FFmpegContent : public VideoContent, public AudioContent, public SubtitleContent
{
public:
	FFmpegContent (boost::shared_ptr<const Film>, boost::filesystem::path);
	FFmpegContent (boost::shared_ptr<const Film>, boost::shared_ptr<const cxml::Node>);

	boost::shared_ptr<FFmpegContent> shared_from_this () {
		return boost::dynamic_pointer_cast<FFmpegContent> (Content::shared_from_this ());
	}
	
	void examine (boost::shared_ptr<Job>);
	std::string summary () const;
	std::string technical_summary () const;
	std::string information () const;
	void as_xml (xmlpp::Node *) const;
	Time full_length () const;

	std::string identifier () const;
	
	/* AudioContent */
	int audio_channels () const;
	AudioContent::Frame audio_length () const;
	int content_audio_frame_rate () const;
	int output_audio_frame_rate () const;
	AudioMapping audio_mapping () const;
	void set_audio_mapping (AudioMapping);

	void set_filters (std::vector<Filter const *> const &);
	
	std::vector<boost::shared_ptr<FFmpegSubtitleStream> > subtitle_streams () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _subtitle_streams;
	}

	boost::shared_ptr<FFmpegSubtitleStream> subtitle_stream () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _subtitle_stream;
	}

	std::vector<boost::shared_ptr<FFmpegAudioStream> > audio_streams () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _audio_streams;
	}
	
	boost::shared_ptr<FFmpegAudioStream> audio_stream () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _audio_stream;
	}

	std::vector<Filter const *> filters () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _filters;
	}

	void set_subtitle_stream (boost::shared_ptr<FFmpegSubtitleStream>);
	void set_audio_stream (boost::shared_ptr<FFmpegAudioStream>);

	boost::optional<double> first_video () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _first_video;
	}
	
private:
	friend class ffmpeg_pts_offset_test;
	
	std::vector<boost::shared_ptr<FFmpegSubtitleStream> > _subtitle_streams;
	boost::shared_ptr<FFmpegSubtitleStream> _subtitle_stream;
	std::vector<boost::shared_ptr<FFmpegAudioStream> > _audio_streams;
	boost::shared_ptr<FFmpegAudioStream> _audio_stream;
	boost::optional<double> _first_video;
	/** Video filters that should be used when generating DCPs */
	std::vector<Filter const *> _filters;
};

#endif
