/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "audio_content.h"
#include "subtitle_content.h"

struct AVFormatContext;
struct AVStream;

class Filter;
class FFmpegSubtitleStream;
class FFmpegAudioStream;
class VideoContent;
struct ffmpeg_pts_offset_test;
struct audio_sampling_rate_test;

class FFmpegContentProperty
{
public:
	static int const SUBTITLE_STREAMS;
	/** The chosen subtitle stream, or something about it */
	static int const SUBTITLE_STREAM;
	static int const FILTERS;
};

class FFmpegContent : public AudioContent, public SubtitleContent
{
public:
	FFmpegContent (boost::shared_ptr<const Film>, boost::filesystem::path);
	FFmpegContent (boost::shared_ptr<const Film>, cxml::ConstNodePtr, int version, std::list<std::string> &);
	FFmpegContent (boost::shared_ptr<const Film>, std::vector<boost::shared_ptr<Content> >);

	boost::shared_ptr<FFmpegContent> shared_from_this () {
		return boost::dynamic_pointer_cast<FFmpegContent> (Content::shared_from_this ());
	}

	void examine (boost::shared_ptr<Job>);
	std::string summary () const;
	std::string technical_summary () const;
	void as_xml (xmlpp::Node *) const;
	DCPTime full_length () const;

	std::string identifier () const;

	/* VideoContent */
	void set_default_colour_conversion ();

	/* AudioContent */
	std::vector<AudioStreamPtr> audio_streams () const;

	/* SubtitleContent */
	bool has_text_subtitles () const;
	bool has_image_subtitles () const;
	double subtitle_video_frame_rate () const;

	void set_filters (std::vector<Filter const *> const &);

	std::vector<boost::shared_ptr<FFmpegSubtitleStream> > subtitle_streams () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _subtitle_streams;
	}

	boost::shared_ptr<FFmpegSubtitleStream> subtitle_stream () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _subtitle_stream;
	}

	std::vector<boost::shared_ptr<FFmpegAudioStream> > ffmpeg_audio_streams () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _audio_streams;
	}

	std::vector<Filter const *> filters () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _filters;
	}

	void set_subtitle_stream (boost::shared_ptr<FFmpegSubtitleStream>);

	boost::optional<ContentTime> first_video () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _first_video;
	}

	std::list<ContentTimePeriod> image_subtitles_during (ContentTimePeriod, bool starting) const;
	std::list<ContentTimePeriod> text_subtitles_during (ContentTimePeriod, bool starting) const;

	void signal_subtitle_stream_changed ();

protected:
	void add_properties (std::list<UserProperty> &) const;

private:
	friend struct ffmpeg_pts_offset_test;
	friend struct audio_sampling_rate_test;

	std::vector<boost::shared_ptr<FFmpegSubtitleStream> > _subtitle_streams;
	boost::shared_ptr<FFmpegSubtitleStream> _subtitle_stream;
	std::vector<boost::shared_ptr<FFmpegAudioStream> > _audio_streams;
	boost::optional<ContentTime> _first_video;
	/** Video filters that should be used when generating DCPs */
	std::vector<Filter const *> _filters;

	AVColorRange _color_range;
	AVColorPrimaries _color_primaries;
	AVColorTransferCharacteristic _color_trc;
	AVColorSpace _colorspace;
	boost::optional<int> _bits_per_pixel;
};

#endif
