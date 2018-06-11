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

#include "ffmpeg_content.h"
#include "audio_content.h"
#include "image_content.h"
#include "atmos_mxf_content.h"
#include "text_text_content.h"
#include "dcp_content.h"
#include "dcp_text_content.h"
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

#include "i18n.h"

using std::string;
using std::list;
using boost::shared_ptr;
using boost::optional;

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
		content.reset (new TextTextContent (film, node, version));
	} else if (type == "DCP") {
		content.reset (new DCPContent (film, node, version));
	} else if (type == "DCPSubtitle") {
		content.reset (new DCPTextContent (film, node, version));
	} else if (type == "VideoMXF") {
		content.reset (new VideoMXFContent (film, node, version));
	} else if (type == "AtmosMXF") {
		content.reset (new AtmosMXFContent (film, node, version));
	}

	/* See if this content should be nudged to start on a video frame */
	DCPTime const old_pos = content->position();
	content->set_position(old_pos);
	if (old_pos != content->position()) {
		string note = _("Your project contains video content that was not aligned to a frame boundary.");
		note += "  ";
		if (old_pos < content->position()) {
			note += String::compose(
				_("The file %1 has been moved %2 milliseconds later."),
				content->path_summary(), DCPTime(content->position() - old_pos).seconds() * 1000
				);
		} else {
			note += String::compose(
				_("The file %1 has been moved %2 milliseconds earlier."),
				content->path_summary(), DCPTime(content->position() - old_pos).seconds() * 1000
				);
		}
		notes.push_back (note);
	}

	/* ...or have a start trim which is an integer number of frames */
	ContentTime const old_trim = content->trim_start();
	content->set_trim_start(old_trim);
	if (old_trim != content->trim_start()) {
		string note = _("Your project contains video content whose trim was not aligned to a frame boundary.");
		note += "  ";
		if (old_trim < content->trim_start()) {
			note += String::compose(
				_("The file %1 has been trimmed by %2 milliseconds more."),
				content->path_summary(), ContentTime(content->trim_start() - old_trim).seconds() * 1000
				);
		} else {
			note += String::compose(
				_("The file %1 has been trimmed by %2 milliseconds less."),
				content->path_summary(), ContentTime(old_trim - content->trim_start()).seconds() * 1000
				);
		}
		notes.push_back (note);
	}

	return content;
}

/** Create some Content objects from a file or directory.
 *  @param film Film that the content will be in.
 *  @param path File or directory.
 *  @return Content objects.
 */
list<shared_ptr<Content> >
content_factory (shared_ptr<const Film> film, boost::filesystem::path path)
{
	list<shared_ptr<Content> > content;

	if (boost::filesystem::is_directory (path)) {

		LOG_GENERAL ("Look in directory %1", path);

		if (boost::filesystem::is_empty (path)) {
			return content;
		}

		/* See if this is a set of images or a set of sound files */

		int image_files = 0;
		int sound_files = 0;
		int read = 0;
		for (boost::filesystem::directory_iterator i(path); i != boost::filesystem::directory_iterator() && read < 10; ++i) {

			LOG_GENERAL ("Checking file %1", i->path());

			if (boost::starts_with (i->path().leaf().string(), ".")) {
				/* We ignore hidden files */
				LOG_GENERAL ("Ignored %1 (starts with .)", i->path());
				continue;
			}

			if (!boost::filesystem::is_regular_file(i->path())) {
				/* Ignore things which aren't files (probably directories) */
				LOG_GENERAL ("Ignored %1 (not a regular file)", i->path());
				continue;
			}

			if (valid_image_file (i->path ())) {
				++image_files;
			}

			if (valid_sound_file (i->path ())) {
				++sound_files;
			}

			++read;
		}

		if (image_files > 0 && sound_files == 0)  {
			content.push_back (shared_ptr<Content> (new ImageContent (film, path)));
		} else if (image_files == 0 && sound_files > 0) {
			for (boost::filesystem::directory_iterator i(path); i != boost::filesystem::directory_iterator(); ++i) {
				content.push_back (shared_ptr<FFmpegContent> (new FFmpegContent (film, i->path())));
			}
		}

	} else {

		shared_ptr<Content> single;

		string ext = path.extension().string ();
		transform (ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (valid_image_file (path)) {
			single.reset (new ImageContent (film, path));
		} else if (ext == ".srt" || ext == ".ssa" || ext == ".ass") {
			single.reset (new TextTextContent (film, path));
		} else if (ext == ".xml") {
			cxml::Document doc;
			doc.read_file (path);
			if (doc.root_name() == "DCinemaSecurityMessage") {
				throw KDMAsContentError ();
			}
			single.reset (new DCPTextContent (film, path));
		} else if (ext == ".mxf" && dcp::SMPTESubtitleAsset::valid_mxf (path)) {
			single.reset (new DCPTextContent (film, path));
		} else if (ext == ".mxf" && VideoMXFContent::valid_mxf (path)) {
			single.reset (new VideoMXFContent (film, path));
		} else if (ext == ".mxf" && AtmosMXFContent::valid_mxf (path)) {
			single.reset (new AtmosMXFContent (film, path));
		}

		if (!single) {
			single.reset (new FFmpegContent (film, path));
		}

		content.push_back (single);
	}

	return content;
}
