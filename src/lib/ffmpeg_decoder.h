/* -*- c-basic-offset: 8; default-tab-width: 8; -*- */

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

/** @file  src/ffmpeg_decoder.h
 *  @brief A decoder using FFmpeg to decode content.
 */

#include <vector>
#include <string>
#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/thread/mutex.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libpostproc/postprocess.h>
}
#include "util.h"
#include "decoder.h"
#include "video_decoder.h"
#include "audio_decoder.h"

struct AVFilterGraph;
struct AVCodecContext;
struct AVFilterContext;
struct AVFormatContext;
struct AVFrame;
struct AVBufferContext;
struct AVCodec;
struct AVStream;
class Job;
class Options;
class Image;
class Log;
class FFmpegContent;
class Film;

/** @class FFmpegDecoder
 *  @brief A decoder using FFmpeg to decode content.
 */
class FFmpegDecoder : public VideoDecoder, public AudioDecoder
{
public:
	FFmpegDecoder (boost::shared_ptr<const Film>, boost::shared_ptr<const FFmpegContent>, bool video, bool audio);
	~FFmpegDecoder ();

	/* Decoder */

	void pass ();
	void seek (Time);
	void seek_back ();
	void seek_forward ();
	Time position () const;
	bool done () const;

	/* VideoDecoder */

	float video_frame_rate () const;
	libdcp::Size video_size () const;
	ContentVideoFrame video_length () const;

	/* FFmpegDecoder */

	std::vector<boost::shared_ptr<FFmpegSubtitleStream> > subtitle_streams () const {
		return _subtitle_streams;
	}
	
	std::vector<boost::shared_ptr<FFmpegAudioStream> > audio_streams () const {
		return _audio_streams;
	}

	boost::shared_ptr<const FFmpegContent> ffmpeg_content () const {
		return _ffmpeg_content;
	}

private:

	/* No copy construction */
	FFmpegDecoder (FFmpegDecoder const &);
	FFmpegDecoder& operator= (FFmpegDecoder const &);

	PixelFormat pixel_format () const;
	AVSampleFormat audio_sample_format () const;
	int bytes_per_audio_sample () const;
	void do_seek (Time, bool, bool);

	void setup_general ();
	void setup_video ();
	void setup_audio ();
	void setup_subtitle ();

	bool decode_video_packet ();
	void decode_audio_packet ();

	void maybe_add_subtitle ();
	boost::shared_ptr<AudioBuffers> deinterleave_audio (uint8_t** data, int size);

	std::string stream_name (AVStream* s) const;

	boost::shared_ptr<const FFmpegContent> _ffmpeg_content;

	AVFormatContext* _format_context;
	int _video_stream;
	
	AVFrame* _frame;

	AVCodecContext* _video_codec_context;
	AVCodec* _video_codec;
	AVCodecContext* _audio_codec_context;    ///< may be 0 if there is no audio
	AVCodec* _audio_codec;                   ///< may be 0 if there is no audio
	AVCodecContext* _subtitle_codec_context; ///< may be 0 if there is no subtitle
	AVCodec* _subtitle_codec;                ///< may be 0 if there is no subtitle

	AVPacket _packet;

	std::list<boost::shared_ptr<FilterGraph> > _filter_graphs;
	boost::mutex _filter_graphs_mutex;

        std::vector<boost::shared_ptr<FFmpegSubtitleStream> > _subtitle_streams;
        std::vector<boost::shared_ptr<FFmpegAudioStream> > _audio_streams;

	bool _decode_video;
	bool _decode_audio;

	/* It would appear (though not completely verified) that one must have
	   a mutex around calls to avcodec_open* and avcodec_close... and here
	   it is.
	*/
	static boost::mutex _mutex;
};
