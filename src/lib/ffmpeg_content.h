/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_FFMPEG_CONTENT_H
#define DCPOMATIC_FFMPEG_CONTENT_H

#ifdef DCPOMATIC_VARIANT_SWAROOP
#include "encrypted_ecinema_kdm.h"
#endif
#include "content.h"
#include "audio_stream.h"

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
	static int const KDM;
};

class FFmpegContent : public Content
{
public:
	FFmpegContent (boost::filesystem::path);
	FFmpegContent (cxml::ConstNodePtr, int version, std::list<std::string> &);
	FFmpegContent (std::vector<boost::shared_ptr<Content> >);

	boost::shared_ptr<FFmpegContent> shared_from_this () {
		return boost::dynamic_pointer_cast<FFmpegContent> (Content::shared_from_this ());
	}

	boost::shared_ptr<const FFmpegContent> shared_from_this () const {
		return boost::dynamic_pointer_cast<const FFmpegContent> (Content::shared_from_this ());
	}

	void examine (boost::shared_ptr<const Film> film, boost::shared_ptr<Job>);
	void take_settings_from (boost::shared_ptr<const Content> c);
	std::string summary () const;
	std::string technical_summary () const;
	void as_xml (xmlpp::Node *, bool with_paths) const;
	dcpomatic::DCPTime full_length (boost::shared_ptr<const Film> film) const;
	dcpomatic::DCPTime approximate_length () const;

	std::string identifier () const;

	void set_default_colour_conversion ();

	void set_filters (std::vector<Filter const *> const &);

	std::vector<boost::shared_ptr<FFmpegSubtitleStream> > subtitle_streams () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _subtitle_streams;
	}

	boost::shared_ptr<FFmpegSubtitleStream> subtitle_stream () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _subtitle_stream;
	}

	std::vector<boost::shared_ptr<FFmpegAudioStream> > ffmpeg_audio_streams () const;

	std::vector<Filter const *> filters () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _filters;
	}

	void set_subtitle_stream (boost::shared_ptr<FFmpegSubtitleStream>);

	boost::optional<dcpomatic::ContentTime> first_video () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _first_video;
	}

	void signal_subtitle_stream_changed ();

#ifdef DCPOMATIC_VARIANT_SWAROOP

	bool encrypted () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _encrypted;
	}

	void add_kdm (EncryptedECinemaKDM kdm);

	boost::optional<EncryptedECinemaKDM> kdm () const {
		return _kdm;
	}

	boost::optional<std::string> id () const {
		return _id;
	}

#endif

private:
	void add_properties (boost::shared_ptr<const Film> film, std::list<UserProperty> &) const;

	friend struct ffmpeg_pts_offset_test;
	friend struct audio_sampling_rate_test;

	std::vector<boost::shared_ptr<FFmpegSubtitleStream> > _subtitle_streams;
	boost::shared_ptr<FFmpegSubtitleStream> _subtitle_stream;
	boost::optional<dcpomatic::ContentTime> _first_video;
	/** Video filters that should be used when generating DCPs */
	std::vector<Filter const *> _filters;

	boost::optional<AVColorRange> _color_range;
	boost::optional<AVColorPrimaries> _color_primaries;
	boost::optional<AVColorTransferCharacteristic> _color_trc;
	boost::optional<AVColorSpace> _colorspace;
	boost::optional<int> _bits_per_pixel;
#ifdef DCPOMATIC_VARIANT_SWAROOP
	bool _encrypted;
	boost::optional<EncryptedECinemaKDM> _kdm;
	boost::optional<std::string> _id;
#endif
};

#endif
