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

#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "video_source.h"
#include "audio_source.h"
#include "video_sink.h"
#include "audio_sink.h"
#include "ffmpeg_content.h"

class Content;
class FFmpegContent;
class FFmpegDecoder;
class ImageMagickContent;
class ImageMagickDecoder;
class SndfileContent;
class SndfileDecoder;
class Job;
class Film;

class Playlist
{
public:
	Playlist ();

	void setup (ContentList);

	ContentAudioFrame audio_length () const;
	int audio_channels () const;
	int audio_frame_rate () const;
	int64_t audio_channel_layout () const;
	bool has_audio () const;
	
	float video_frame_rate () const;
	libdcp::Size video_size () const;
	ContentVideoFrame video_length () const;

	enum VideoFrom {
		VIDEO_NONE,
		VIDEO_FFMPEG,
		VIDEO_IMAGEMAGICK
	};

	enum AudioFrom {
		AUDIO_NONE,
		AUDIO_FFMPEG,
		AUDIO_SNDFILE
	};

	VideoFrom video_from () const {
		return _video_from;
	}

	AudioFrom audio_from () const {
		return _audio_from;
	}

	boost::shared_ptr<const FFmpegContent> ffmpeg () const {
		return _ffmpeg;
	}

	std::list<boost::shared_ptr<const ImageMagickContent> > imagemagick () const {
		return _imagemagick;
	}

	std::list<boost::shared_ptr<const SndfileContent> > sndfile () const {
		return _sndfile;
	}
	
private:
	VideoFrom _video_from;
	AudioFrom _audio_from;

	boost::shared_ptr<const FFmpegContent> _ffmpeg;
	std::list<boost::shared_ptr<const ImageMagickContent> > _imagemagick;
	std::list<boost::shared_ptr<const SndfileContent> > _sndfile;
};

class Player : public VideoSource, public AudioSource, public VideoSink, public AudioSink, public boost::enable_shared_from_this<Player>
{
public:
	Player (boost::shared_ptr<const Film>, boost::shared_ptr<const Playlist>);

	void disable_video ();
	void disable_audio ();
	void disable_subtitles ();
	void disable_video_sync ();

	bool pass ();
	void set_progress (boost::shared_ptr<Job>);
	bool seek (double);
	bool seek_to_last ();

	double last_video_time () const;

private:
	void process_video (boost::shared_ptr<Image> i, bool same, boost::shared_ptr<Subtitle> s);
	void process_audio (boost::shared_ptr<AudioBuffers>);
	void setup_decoders ();

	boost::shared_ptr<const Film> _film;
	boost::shared_ptr<const Playlist> _playlist;
	
	bool _video;
	bool _audio;
	bool _subtitles;
	
	bool _have_setup_decoders;
	boost::shared_ptr<FFmpegDecoder> _ffmpeg_decoder;
	bool _ffmpeg_decoder_done;
	std::list<boost::shared_ptr<ImageMagickDecoder> > _imagemagick_decoders;
	std::list<boost::shared_ptr<ImageMagickDecoder> >::iterator _imagemagick_decoder;
	std::list<boost::shared_ptr<SndfileDecoder> > _sndfile_decoders;

	bool _video_sync;
};
