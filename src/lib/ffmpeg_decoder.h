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
extern "C" {
#include <libavcodec/avcodec.h>
#include <libpostproc/postprocess.h>
}
#include "util.h"
#include "decoder.h"

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

/** @class FFmpegDecoder
 *  @brief A decoder using FFmpeg to decode content.
 */
class FFmpegDecoder : public Decoder
{
public:
	FFmpegDecoder (boost::shared_ptr<Film>, boost::shared_ptr<const Options>, Job *, bool, bool);
	~FFmpegDecoder ();

	/* Methods to query our input video */
	float frames_per_second () const;
	Size native_size () const;
	int audio_channels () const;
	int audio_sample_rate () const;
	AVSampleFormat audio_sample_format () const;
	int64_t audio_channel_layout () const;
	bool has_subtitles () const;

	std::vector<AudioStream> audio_streams () const;
	std::vector<SubtitleStream> subtitle_streams () const;

private:

	bool do_pass ();
	PixelFormat pixel_format () const;
	int time_base_numerator () const;
	int time_base_denominator () const;
	int sample_aspect_ratio_numerator () const;
	int sample_aspect_ratio_denominator () const;

	void setup_general ();
	void setup_video ();
	void setup_audio ();
	void setup_subtitle ();

	void maybe_add_subtitle ();

	std::string stream_name (AVStream* s) const;

	AVFormatContext* _format_context;
	int _video_stream;
	int _audio_stream; ///< may be < 0 if there is no audio
	int _subtitle_stream; ///< may be < 0 if there is no subtitle
	AVFrame* _frame;

	std::vector<AudioStream> _audio_streams;
	std::vector<SubtitleStream> _subtitle_streams;
	
	AVCodecContext* _video_codec_context;
	AVCodec* _video_codec;
	AVCodecContext* _audio_codec_context;    ///< may be 0 if there is no audio
	AVCodec* _audio_codec;                   ///< may be 0 if there is no audio
	AVCodecContext* _subtitle_codec_context; ///< may be 0 if there is no subtitle
	AVCodec* _subtitle_codec;                ///< may be 0 if there is no subtitle

	AVPacket _packet;

	boost::optional<double> _first_video;
	boost::optional<double> _first_audio;
};
