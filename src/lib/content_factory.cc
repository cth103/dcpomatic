/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/content_factory.cc
 *  @brief Methods to create content objects.
 */

#include "ffmpeg_content.h"
#include "audio_content.h"
#include "image_content.h"
#include "atmos_mxf_content.h"
#include "text_subtitle_content.h"
#include "dcp_content.h"
#include "dcp_subtitle_content.h"
#include "util.h"
#include "ffmpeg_audio_stream.h"
#include "video_mxf_content.h"
#include "film.h"
#include "log_entry.h"
#include "log.h"
#include "compose.hpp"
#include <libcxml/cxml.h>
#include <dcp/smpte_subtitle_asset.h>
#include <boost/algorithm/string.hpp>

using std::string;
using std::list;
using boost::shared_ptr;

#define LOG_GENERAL(...) film->log()->log (String::compose (__VA_ARGS__), LogEntry::TYPE_GENERAL);

/** Create a Content object from an XML node.
 *  @param film Film that the content will be in.
 *  @param node XML description.
 *  @param version XML state version.
 *  @param notes A list to which is added descriptions of any non-critial warnings / messages.
 *  @return Content object, or 0 if no content was recognised in the XML.
 */
shared_ptr<Content>
content_factory (shared_ptr<const Film> film, cxml::NodePtr node, int version, list<string>& notes)
{
	string const type = node->string_child ("Type");

	boost::shared_ptr<Content> content;

	if (type == "FFmpeg") {
		/* SndfileContent is now handled by the FFmpeg code rather than by
		   separate libsndfile-based code.
		*/
		content.reset (new FFmpegContent (film, node, version, notes));
	} else if (type == "Image") {
		content.reset (new ImageContent (film, node, version));
	} else if (type == "Sndfile") {
		/* SndfileContent is now handled by the FFmpeg code rather than by
		   separate libsndfile-based code.
		*/
		content.reset (new FFmpegContent (film, node, version, notes));

		content->audio->set_stream (
			AudioStreamPtr (
				new FFmpegAudioStream (
					"Stream", 0,
					node->number_child<int> ("AudioFrameRate"),
					node->number_child<Frame> ("AudioLength"),
					AudioMapping (node->node_child ("AudioMapping"), version)
					)
				)
			);

	} else if (type == "SubRip" || type == "TextSubtitle") {
		content.reset (new TextSubtitleContent (film, node, version));
	} else if (type == "DCP") {
		content.reset (new DCPContent (film, node, version));
	} else if (type == "DCPSubtitle") {
		content.reset (new DCPSubtitleContent (film, node, version));
	} else if (type == "VideoMXF") {
		content.reset (new VideoMXFContent (film, node, version));
	} else if (type == "AtmosMXF") {
		content.reset (new AtmosMXFContent (film, node, version));
	}

	return content;
}

/** Create a Content object from a file or directory.
 *  @param film Film that the content will be in.
 *  @param path File or directory.
 *  @return Content object.
 */
shared_ptr<Content>
content_factory (shared_ptr<const Film> film, boost::filesystem::path path)
{
	shared_ptr<Content> content;

	if (boost::filesystem::is_directory (path)) {

		LOG_GENERAL ("Look in directory %1", path);

		if (boost::filesystem::is_empty (path)) {
			return shared_ptr<Content> ();
		}

		/* Guess if this is a DCP or a set of images: read the first ten filenames and if they
		   are all valid image files we assume it is a set of images.
		*/

		bool is_dcp = false;
		int read = 0;
		for (boost::filesystem::directory_iterator i(path); i != boost::filesystem::directory_iterator() && read < 10; ++i) {

			LOG_GENERAL ("Checking file %1", i->path());

			if (boost::starts_with (i->path().leaf().string(), "._") || i->path().leaf().string() == ".DS_Store") {
				/* We ignore these files */
				LOG_GENERAL ("Ignored %1 (starts with {._}, or .DS_Store)", i->path());
				continue;
			}

			if (!boost::filesystem::is_regular_file(i->path())) {
				/* Ignore things which aren't files (probably directories) */
				LOG_GENERAL ("Ignored %1 (not a regular file)", i->path());
				continue;
			}

			if (!valid_image_file (i->path())) {
				/* We have a normal file which isn't an image; assume we are looking
				   at a DCP.
				*/
				LOG_GENERAL ("It's a DCP because of %1", i->path());
				is_dcp = true;
			}

			++read;
		}

		if (is_dcp) {
			content.reset (new DCPContent (film, path));
		} else {
			content.reset (new ImageContent (film, path));
		}

	} else {

		string ext = path.extension().string ();
		transform (ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (valid_image_file (path)) {
			content.reset (new ImageContent (film, path));
		} else if (ext == ".srt" || ext == ".ssa") {
			content.reset (new TextSubtitleContent (film, path));
		} else if (ext == ".xml") {
			content.reset (new DCPSubtitleContent (film, path));
		} else if (ext == ".mxf" && dcp::SMPTESubtitleAsset::valid_mxf (path)) {
			content.reset (new DCPSubtitleContent (film, path));
		} else if (ext == ".mxf" && VideoMXFContent::valid_mxf (path)) {
			content.reset (new VideoMXFContent (film, path));
		} else if (ext == ".mxf" && AtmosMXFContent::valid_mxf (path)) {
			content.reset (new AtmosMXFContent (film, path));
		}

		if (!content) {
			content.reset (new FFmpegContent (film, path));
		}
	}

	return content;
}
