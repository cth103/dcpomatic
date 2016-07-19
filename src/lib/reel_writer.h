/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

#include "types.h"
#include "dcpomatic_time.h"
#include "referenced_reel_asset.h"
#include "player_subtitles.h"
#include <dcp/picture_asset_writer.h>
#include <boost/shared_ptr.hpp>

class Film;
class Job;
class Font;
class AudioBuffers;

namespace dcp {
	class MonoPictureAsset;
	class MonoPictureAssetWriter;
	class StereoPictureAsset;
	class StereoPictureAssetWriter;
	class PictureAsset;
	class PictureAssetWriter;
	class SoundAsset;
	class SoundAssetWriter;
	class SubtitleAsset;
	class ReelAsset;
	class Reel;
}

class ReelWriter
{
public:
	ReelWriter (boost::shared_ptr<const Film> film, DCPTimePeriod period, boost::shared_ptr<Job> job);

	void write (boost::optional<dcp::Data> encoded, Frame frame, Eyes eyes);
	void fake_write (Frame frame, Eyes eyes, int size);
	void repeat_write (Frame frame, Eyes eyes);
	void write (boost::shared_ptr<const AudioBuffers> audio);
	void write (PlayerSubtitles subs);

	void finish ();
	boost::shared_ptr<dcp::Reel> create_reel (std::list<ReferencedReelAsset> const & refs, std::list<boost::shared_ptr<Font> > const & fonts);
	void calculate_digests (boost::function<void (float)> set_progress);

	Frame start () const;

	DCPTimePeriod period () const {
		return _period;
	}

	int total_written_audio_frames () const {
		return _total_written_audio_frames;
	}

	int last_written_video_frame () const {
		return _last_written_video_frame;
	}

	Eyes last_written_eyes () const {
		return _last_written_eyes;
	}

	int first_nonexistant_frame () const {
		return _first_nonexistant_frame;
	}

	dcp::FrameInfo read_frame_info (FILE* file, Frame frame, Eyes eyes) const;

private:

	void write_frame_info (Frame frame, Eyes eyes, dcp::FrameInfo info) const;
	long frame_info_position (Frame frame, Eyes eyes) const;
	void check_existing_picture_asset ();
	bool existing_picture_frame_ok (FILE* asset_file, FILE* info_file) const;

	boost::shared_ptr<const Film> _film;

	DCPTimePeriod _period;
	/** the first frame index that does not already exist in our MXF */
	int _first_nonexistant_frame;
	/** the data of the last written frame, if there is one */
	boost::optional<dcp::Data> _last_written[EYES_COUNT];
	/** the index of the last written video frame within the reel */
	int _last_written_video_frame;
	Eyes _last_written_eyes;
	/** the number of audio frames that have been written to the reel */
	int _total_written_audio_frames;

	boost::shared_ptr<dcp::PictureAsset> _picture_asset;
	boost::shared_ptr<dcp::PictureAssetWriter> _picture_asset_writer;
	boost::shared_ptr<dcp::SoundAsset> _sound_asset;
	boost::shared_ptr<dcp::SoundAssetWriter> _sound_asset_writer;
	boost::shared_ptr<dcp::SubtitleAsset> _subtitle_asset;

	static int const _info_size;
};
