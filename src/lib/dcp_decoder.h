/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

/** @file  src/dcp_decoder.h
 *  @brief A decoder of existing DCPs.
 */

#include "decoder.h"
#include "dcp.h"
#include <dcp/mono_picture_asset_reader.h>
#include <dcp/stereo_picture_asset_reader.h>
#include <dcp/sound_asset_reader.h>
#include <dcp/subtitle_asset.h>

namespace dcp {
	class Reel;
}

class DCPContent;
class Log;
struct dcp_subtitle_within_dcp_test;

class DCPDecoder : public DCP, public Decoder
{
public:
	DCPDecoder (boost::shared_ptr<const DCPContent>, boost::shared_ptr<Log> log, bool fast);

	std::list<boost::shared_ptr<dcp::Reel> > reels () const {
		return _reels;
	}

	void set_decode_referenced (bool r);
	void set_forced_reduction (boost::optional<int> reduction);

	bool pass ();
	void seek (ContentTime t, bool accurate);

private:
	friend struct dcp_subtitle_within_dcp_test;

	void next_reel ();
	void get_readers ();
	void pass_captions (ContentTime next);
	void pass_captions (ContentTime next, boost::shared_ptr<dcp::SubtitleAsset> asset, bool reference, int64_t entry_point, boost::shared_ptr<CaptionDecoder> decoder);

	/** Time of next thing to return from pass relative to the start of _reel */
	ContentTime _next;
	std::list<boost::shared_ptr<dcp::Reel> > _reels;

	std::list<boost::shared_ptr<dcp::Reel> >::iterator _reel;
	/** Offset of _reel from the start of the content in frames */
	int64_t _offset;
	/** Reader for current mono picture asset, if applicable */
	boost::shared_ptr<dcp::MonoPictureAssetReader> _mono_reader;
	/** Reader for current stereo picture asset, if applicable */
	boost::shared_ptr<dcp::StereoPictureAssetReader> _stereo_reader;
	/** Reader for current sound asset, if applicable */
	boost::shared_ptr<dcp::SoundAssetReader> _sound_reader;

	bool _decode_referenced;
	boost::optional<int> _forced_reduction;
};
