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
#include "subtitle_decoder.h"
#include "ffmpeg.h"

class Film;
class FilterGraph;
class ffmpeg_pts_offset_test;

/** @class FFmpegDecoder
 *  @brief A decoder using FFmpeg to decode content.
 */
class FFmpegDecoder : public VideoDecoder, public AudioDecoder, public SubtitleDecoder, public FFmpeg
{
public:
	FFmpegDecoder (boost::shared_ptr<const Film>, boost::shared_ptr<const FFmpegContent>, bool video, bool audio);
	~FFmpegDecoder ();

	void pass ();
	void seek (VideoContent::Frame, bool);
	bool done () const;

private:
	friend class ::ffmpeg_pts_offset_test;

	static double compute_pts_offset (double, double, float);

	void setup_subtitle ();

	AVSampleFormat audio_sample_format () const;
	int bytes_per_audio_sample () const;

	bool decode_video_packet ();
	void decode_audio_packet ();
	void decode_subtitle_packet ();

	void maybe_add_subtitle ();
	boost::shared_ptr<AudioBuffers> deinterleave_audio (uint8_t** data, int size);

	AVCodecContext* _subtitle_codec_context; ///< may be 0 if there is no subtitle
	AVCodec* _subtitle_codec;		 ///< may be 0 if there is no subtitle
	
	std::list<boost::shared_ptr<FilterGraph> > _filter_graphs;
	boost::mutex _filter_graphs_mutex;

	bool _decode_video;
	bool _decode_audio;

	double _video_pts_offset;
	double _audio_pts_offset;
	bool _just_sought;
};
