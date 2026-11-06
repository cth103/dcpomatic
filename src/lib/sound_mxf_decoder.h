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


#include "decoder.h"
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_reader.h>


class SoundMXFContent;


class SoundMXFDecoder : public Decoder
{
public:
	SoundMXFDecoder(std::shared_ptr<const Film> film, std::shared_ptr<const SoundMXFContent> content, bool fast);

	bool pass() override;
	void seek(dcpomatic::ContentTime t, bool accurate) override;

private:
	std::shared_ptr<const SoundMXFContent> _content;
	std::shared_ptr<dcp::SoundAsset> _asset;
	std::shared_ptr<dcp::SoundAssetReader> _reader;
	/** The next frame (of the asset's edit rate) to return from pass() */
	int64_t _frame = 0;
	/** The asset's edit rate in frames per second */
	double _edit_rate;
};
