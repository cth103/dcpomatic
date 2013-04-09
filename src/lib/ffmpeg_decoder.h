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
#include "film.h"
#include "ffmpeg_content.h"

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
class FFmpegDecoder : public VideoDecoder, public AudioDecoder
{
public:
	FFmpegDecoder (boost::shared_ptr<const Film>, boost::shared_ptr<const FFmpegContent>, bool video, bool audio, bool subtitles, bool video_sync);
	~FFmpegDecoder ();

	float video_frame_rate () const;
	libdcp::Size native_size () const;
	ContentVideoFrame video_length () const;
	int time_base_numerator () const;
	int time_base_denominator () const;
	int sample_aspect_ratio_numerator () const;
	int sample_aspect_ratio_denominator () const;

	std::vector<FFmpegSubtitleStream> subtitle_streams () const {
		return _subtitle_streams;
	}
	
	std::vector<FFmpegAudioStream> audio_streams () const {
		return _audio_streams;
	}

	bool seek (double);
	bool pass ();

private:

	/* No copy construction */
	FFmpegDecoder (FFmpegDecoder const &);
	FFmpegDecoder& operator= (FFmpegDecoder const &);

	PixelFormat pixel_format () const;
	AVSampleFormat audio_sample_format () const;
	int bytes_per_audio_sample () const;

	void out_with_sync ();
	void filter_and_emit_video (AVFrame *);
	double frame_time () const;

	void setup_general ();
	void setup_video ();
	void setup_audio ();
	void setup_subtitle ();

	void decode_audio_packet ();

	void maybe_add_subtitle ();
	boost::shared_ptr<AudioBuffers> deinterleave_audio (uint8_t** data, int size);

	void film_changed (Film::Property);

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

	boost::optional<double> _first_video;
	boost::optional<double> _first_audio;

	std::list<boost::shared_ptr<FilterGraph> > _filter_graphs;
	boost::mutex _filter_graphs_mutex;

        std::vector<FFmpegSubtitleStream> _subtitle_streams;
        std::vector<FFmpegAudioStream> _audio_streams;

	bool _decode_video;
	bool _decode_audio;
	bool _decode_subtitles;
	bool _video_sync;

	/* It would appear (though not completely verified) that one must have
	   a mutex around calls to avcodec_open* and avcodec_close... and here
	   it is.
	*/
	static boost::mutex _mutex;
};
