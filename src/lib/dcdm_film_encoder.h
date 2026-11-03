/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


/** @file  src/lib/dcdm_film_encoder.h
 *  @brief DCDMFilmEncoder class.
 */


#ifndef DCPOMATIC_DCDM_FILM_ENCODER_H
#define DCPOMATIC_DCDM_FILM_ENCODER_H


#include "atmos_metadata.h"
#include "audio_mapping.h"
#include "butler.h"
#include "dcdm_audio_layout.h"
#include "event_history.h"
#include "film_encoder.h"
#include <boost/filesystem.hpp>
#include <boost/signals2.hpp>
#include <boost/thread/mutex.hpp>

#include <dcp/atmos_frame.h>


namespace dcp {
	class AtmosAsset;
	class AtmosAssetWriter;
}


/** How DCDM pictures are stored */
enum class DCDMPictureFormat
{
	/** Mathematically lossless JPEG 2000 codestreams (SMPTE ST 428-24 packed images), in picture/ */
	J2K_LOSSLESS,
	/** Uncompressed 12-bit DPX */
	DPX
};


/** @class DCDMFilmEncoder
 *  @brief Export a film as a DCDM: 12-bit X'Y'Z' pictures (lossless JPEG 2000 by default, or DPX) and 24-bit mono WAVs.
 *
 *  Picture is converted to XYZ exactly as for a DCP (DCPVideo::convert_to_xyz)
 *  and audio is what the player gives a DCP (gain, delay, trim, mapping and the
 *  audio processor applied), so that importing the result gives the same DCP.
 *
 *  Layout, per reel (in reel_NN/ when the film has more than one reel):
 *  picture/NNNNNNNN.pdc.j2c (lossless JPEG 2000, the default) or frame_NNNNNNNN.dpx
 *  (with left/ and right/ for 3D), audio/<label>.wav (the 5.1 or
 *  7.1 layout plus HI, VI and SL when they are used) and,
 *  if the film has Atmos, atmos.mxf with the Atmos frames that fall in the reel.
 *  Open subtitles and closed captions that are not burnt in go to
 *  subtitles/subtitles.xml and captions/NN_<track>.xml (Interop or SMPTE XML,
 *  as the film is), with their fonts, timed relative to the reel as in a DCP.
 */
class DCDMFilmEncoder : public FilmEncoder
{
public:
	/** @throw std::runtime_error if the film's sources are not good enough for a DCDM,
	 *  or the output folder exists and is not empty.
	 */
	DCDMFilmEncoder(
		std::shared_ptr<const Film> film,
		std::weak_ptr<Job> job,
		boost::filesystem::path output,
		DCDMPictureFormat picture_format = DCDMPictureFormat::J2K_LOSSLESS
		);

	void go() override;

	boost::optional<float> current_rate() const override;
	Frame frames_done() const override;
	bool finishing() const override {
		return false;
	}

private:
	void do_go();
	void mark_incomplete(std::string why);
	boost::filesystem::path reel_directory(int reel) const;
	boost::filesystem::path picture_directory(boost::filesystem::path reel_directory) const;
	void atmos(std::shared_ptr<const dcp::AtmosFrame> frame, dcpomatic::DCPTime time, AtmosMetadata metadata);
	void finish_atmos();

	friend struct dcdm_atmos_after_finish_is_ignored_test;
	void write_text();
	void write_info() const;

	boost::filesystem::path _output;
	DCDMPictureFormat _picture_format;
	/** DCP channels to write, with their file labels */
	std::vector<std::pair<dcp::Channel, std::string>> _channels;
	int _reels;

	mutable boost::mutex _mutex;
	dcpomatic::DCPTime _last_time;
	EventHistory _history;

	/** Protects the Atmos state, which is written from the player's thread */
	boost::mutex _atmos_mutex;
	std::vector<dcpomatic::DCPTimePeriod> _reel_periods;
	int _atmos_reel = -1;
	std::shared_ptr<dcp::AtmosAsset> _atmos_asset;
	std::shared_ptr<dcp::AtmosAssetWriter> _atmos_writer;
	boost::signals2::scoped_connection _atmos_connection;

	/** What write_text() actually wrote, for DCDM_INFO.txt */
	bool _open_subtitles_written = false;
	bool _atmos_finished = false;
	std::vector<std::string> _caption_tracks_written;

	Butler _butler;
};


#endif
