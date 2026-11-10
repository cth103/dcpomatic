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


/** @file  src/lib/dcdm_import.h
 *  @brief DCDMImport class.
 */


#ifndef DCPOMATIC_DCDM_IMPORT_H
#define DCPOMATIC_DCDM_IMPORT_H


#include <dcp/language_tag.h>
#include <dcp/types.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <memory>
#include <string>
#include <vector>


class Content;
class Film;


/** A DCDM as DCP-o-matic's DCDM export writes it (see DCDMFilmEncoder), read back so that a DCP can be made from it.
 *
 *  The folder has DCDM_INFO.txt and, for more than one reel, a reel_NN folder per reel; each reel has its picture
 *  (picture/ for lossless JPEG 2000, the reel folder itself for DPX, with left/ and right/ for 3D), one mono WAV
 *  per channel in audio/, perhaps atmos.mxf, and its text in subtitles/ and captions/.
 */
class DCDMImport
{
public:
	/** @return true if @param directory looks like a DCDM that DCP-o-matic wrote */
	static bool valid(boost::filesystem::path directory);

	explicit DCDMImport(boost::filesystem::path directory);

	/** @return content for every part of every reel, not yet examined */
	std::vector<std::shared_ptr<Content>> content() const;

	/** Set up @param film to make a DCP from the DCDM, once our content has been examined and added to it:
	 *  each reel's parts start where the reel starts, each sound file is mapped to its channel, captions are
	 *  closed captions in their track, the pictures are taken as X'Y'Z', lossless JPEG 2000 is re-encoded and
	 *  there is a DCP reel for each DCDM reel.
	 */
	void set_up(std::shared_ptr<Film> film) const;

	/** Examine our content, add it to @param film and then set_up() the film, in a job */
	void examine_and_add(std::shared_ptr<Film> film) const;

	int reels() const {
		return static_cast<int>(_reels.size());
	}

	boost::optional<int> frame_rate() const {
		return _frame_rate;
	}

private:
	struct Sound
	{
		std::shared_ptr<Content> content;
		dcp::Channel channel;
	};

	struct Caption
	{
		std::shared_ptr<Content> content;
		std::string track;
		boost::optional<dcp::LanguageTag> language;
	};

	struct Reel
	{
		std::vector<std::shared_ptr<Content>> pictures;
		std::vector<Sound> sound;
		std::shared_ptr<Content> atmos;
		std::shared_ptr<Content> subtitles;
		std::vector<Caption> captions;
	};

	std::vector<Reel> _reels;
	boost::optional<int> _frame_rate;
	bool _three_d = false;
	bool _j2k = false;
	bool _interop = false;
};


#endif
