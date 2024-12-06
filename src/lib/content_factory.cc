/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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


/** @file  src/lib/content_factory.cc
 *  @brief Methods to create content objects.
 */


#include "atmos_mxf_content.h"
#include "audio_content.h"
#include "dcp_content.h"
#include "dcp_subtitle_content.h"
#include "dcpomatic_log.h"
#include "ffmpeg_audio_stream.h"
#include "ffmpeg_content.h"
#include "film.h"
#include "image_content.h"
#include "log.h"
#include "log_entry.h"
#include "string_text_file_content.h"
#include "util.h"
#include "video_mxf_content.h"
#include "compose.hpp"
#include <libcxml/cxml.h>
#include <dcp/filesystem.h>
#include <dcp/smpte_text_asset.h>
#include <boost/algorithm/string.hpp>

#include "i18n.h"


using std::list;
using std::shared_ptr;
using std::string;
using std::vector;


/** Create a Content object from an XML node.
 *  @param node XML description.
 *  @param directory "Current" directory for any relative file paths mentioned in the XML.
 *  @param version XML state version.
 *  @param notes A list to which is added descriptions of any non-critial warnings / messages.
 *  @return Content object, or 0 if no content was recognised in the XML.
 */
shared_ptr<Content>
content_factory(cxml::ConstNodePtr node, boost::optional<boost::filesystem::path> film_directory, int version, list<string>& notes)
{
	auto const type = node->string_child ("Type");

	std::shared_ptr<Content> content;

	if (type == "FFmpeg") {
		content = std::make_shared<FFmpegContent>(node, film_directory, version, notes);
	} else if (type == "Image") {
		content = std::make_shared<ImageContent>(node, film_directory, version);
	} else if (type == "Sndfile") {
		/* SndfileContent is now handled by the FFmpeg code rather than by
		   separate libsndfile-based code.
		*/
		content = std::make_shared<FFmpegContent>(node, film_directory, version, notes);

		content->audio->set_stream (
			std::make_shared<FFmpegAudioStream>(
				"Stream", 0,
				node->number_child<int> ("AudioFrameRate"),
				node->number_child<Frame> ("AudioLength"),
				AudioMapping(node->node_child("AudioMapping"), version),
				16
				)
			);

	} else if (type == "SubRip" || type == "TextSubtitle") {
		content = std::make_shared<StringTextFileContent>(node, film_directory, version, notes);
	} else if (type == "DCP") {
		content = std::make_shared<DCPContent>(node, film_directory, version);
	} else if (type == "DCPSubtitle") {
		content = std::make_shared<DCPSubtitleContent>(node, film_directory, version);
	} else if (type == "VideoMXF") {
		content = std::make_shared<VideoMXFContent>(node, film_directory, version);
	} else if (type == "AtmosMXF") {
		content = std::make_shared<AtmosMXFContent>(node, film_directory, version);
	}

	return content;
}


/** Create some Content objects from a file or directory.
 *  @param path File or directory.
 *  @return Content objects.
 */
vector<shared_ptr<Content>>
content_factory (boost::filesystem::path path)
{
	vector<shared_ptr<Content>> content;

	if (dcp::filesystem::is_directory(path)) {

		LOG_GENERAL ("Look in directory %1", path);

		if (dcp::filesystem::is_empty(path)) {
			return content;
		}

		/* See if this is a set of images or a set of sound files */

		int image_files = 0;
		int sound_files = 0;
		int read = 0;
		for (dcp::filesystem::directory_iterator i(path); i != dcp::filesystem::directory_iterator() && read < 10; ++i) {

			LOG_GENERAL ("Checking file %1", i->path());

			if (boost::starts_with(i->path().filename().string(), ".")) {
				/* We ignore hidden files */
				LOG_GENERAL ("Ignored %1 (starts with .)", i->path());
				continue;
			}

			if (!dcp::filesystem::is_regular_file(i->path())) {
				/* Ignore things which aren't files (probably directories) */
				LOG_GENERAL ("Ignored %1 (not a regular file)", i->path());
				continue;
			}

			if (valid_image_file(i->path())) {
				++image_files;
			}

			if (valid_sound_file(i->path())) {
				++sound_files;
			}

			++read;
		}

		if (image_files > 0 && sound_files == 0)  {
			content.push_back(std::make_shared<ImageContent>(path));
		} else if (image_files == 0 && sound_files > 0) {
			for (auto i: dcp::filesystem::directory_iterator(path)) {
				content.push_back(std::make_shared<FFmpegContent>(i.path()));
			}
		}

	} else {

		shared_ptr<Content> single;

		auto ext = path.extension().string();
		transform (ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (valid_image_file (path)) {
			single = std::make_shared<ImageContent>(path);
		} else if (ext == ".srt" || ext == ".ssa" || ext == ".ass" || ext == ".stl" || ext == ".vtt") {
			single = std::make_shared<StringTextFileContent>(path);
		} else if (ext == ".xml") {
			cxml::Document doc;
			doc.read_file(dcp::filesystem::fix_long_path(path));
			if (doc.root_name() == "DCinemaSecurityMessage") {
				throw KDMAsContentError ();
			}
			single = std::make_shared<DCPSubtitleContent>(path);
		} else if (ext == ".mxf" && dcp::SMPTETextAsset::valid_mxf(path)) {
			single = std::make_shared<DCPSubtitleContent>(path);
		} else if (ext == ".mxf" && VideoMXFContent::valid_mxf(path)) {
			single = std::make_shared<VideoMXFContent>(path);
		} else if (ext == ".mxf" && AtmosMXFContent::valid_mxf(path)) {
			single = std::make_shared<AtmosMXFContent>(path);
		}

		if (!single) {
			single = std::make_shared<FFmpegContent>(path);
		}

		content.push_back (single);
	}

	return content;
}
