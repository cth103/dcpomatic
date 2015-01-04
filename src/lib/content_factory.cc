/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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
#include "image_content.h"
#include "sndfile_content.h"
#include "subrip_content.h"
#include "dcp_content.h"
#include "dcp_subtitle_content.h"
#include "util.h"
#include <libcxml/cxml.h>

using std::string;
using std::list;
using boost::shared_ptr;

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
		content.reset (new FFmpegContent (film, node, version, notes));
	} else if (type == "Image") {
		content.reset (new ImageContent (film, node, version));
	} else if (type == "Sndfile") {
		content.reset (new SndfileContent (film, node, version));
	} else if (type == "SubRip") {
		content.reset (new SubRipContent (film, node, version));
	} else if (type == "DCP") {
		content.reset (new DCPContent (film, node, version));
	} else if (type == "DCPSubtitle") {
		content.reset (new DCPSubtitleContent (film, node, version));
	}

	return content;
}

/** Create a Content object from a file, depending on its extension.
 *  @param film Film that the content will be in.
 *  @param path File's path.
 *  @return Content object.
 */
shared_ptr<Content>
content_factory (shared_ptr<const Film> film, boost::filesystem::path path)
{
	shared_ptr<Content> content;

	string ext = path.extension().string ();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);

	if (valid_image_file (path)) {
		content.reset (new ImageContent (film, path));
	} else if (SndfileContent::valid_file (path)) {
		content.reset (new SndfileContent (film, path));
	} else if (ext == ".srt") {
		content.reset (new SubRipContent (film, path));
	} else if (ext == ".xml") {
		content.reset (new DCPSubtitleContent (film, path));
	} else if (ext == ".mxf") {
		/* Try to read this .mxf as a subtitle file; if we fail, we fall back
		   to using FFmpeg below.
		*/
		try {
			content.reset (new DCPSubtitleContent (film, path));
		} catch (...) {

		}
	}

	if (!content) {
		content.reset (new FFmpegContent (film, path));
	}

	return content;
}
