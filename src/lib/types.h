/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_TYPES_H
#define DCPOMATIC_TYPES_H

#include "dcpomatic_time.h"
#include "position.h"
#include <dcp/util.h>
#include <boost/shared_ptr.hpp>
#include <vector>
#include <stdint.h>

class Content;
class VideoContent;
class AudioContent;
class SubtitleContent;
class FFmpegContent;
class AudioBuffers;

namespace cxml {
	class Node;
}

namespace xmlpp {
	class Node;
}

/** The version number of the protocol used to communicate
 *  with servers.  Intended to be bumped when incompatibilities
 *  are introduced.
 */
#define SERVER_LINK_VERSION 2

typedef std::vector<boost::shared_ptr<Content> > ContentList;
typedef std::vector<boost::shared_ptr<VideoContent> > VideoContentList;
typedef std::vector<boost::shared_ptr<AudioContent> > AudioContentList;
typedef std::vector<boost::shared_ptr<SubtitleContent> > SubtitleContentList;
typedef std::vector<boost::shared_ptr<FFmpegContent> > FFmpegContentList;

typedef int64_t VideoFrame;
typedef int64_t AudioFrame;

enum VideoFrameType
{
	VIDEO_FRAME_TYPE_2D,
	VIDEO_FRAME_TYPE_3D_LEFT_RIGHT,
	VIDEO_FRAME_TYPE_3D_TOP_BOTTOM,
	VIDEO_FRAME_TYPE_3D_ALTERNATE,
	/** This content is all the left frames of some 3D */
	VIDEO_FRAME_TYPE_3D_LEFT,
	/** This content is all the right frames of some 3D */
	VIDEO_FRAME_TYPE_3D_RIGHT
};

enum Eyes
{
	EYES_BOTH,
	EYES_LEFT,
	EYES_RIGHT,
	EYES_COUNT
};

enum Part
{
	PART_LEFT_HALF,
	PART_RIGHT_HALF,
	PART_TOP_HALF,
	PART_BOTTOM_HALF,
	PART_WHOLE
};

/** @struct Crop
 *  @brief A description of the crop of an image or video.
 */
struct Crop
{
	Crop () : left (0), right (0), top (0), bottom (0) {}
	Crop (int l, int r, int t, int b) : left (l), right (r), top (t), bottom (b) {}
	Crop (boost::shared_ptr<cxml::Node>);

	/** Number of pixels to remove from the left-hand side */
	int left;
	/** Number of pixels to remove from the right-hand side */
	int right;
	/** Number of pixels to remove from the top */
	int top;
	/** Number of pixels to remove from the bottom */
	int bottom;

	dcp::Size apply (dcp::Size s, int minimum = 4) const {
		s.width -= left + right;
		s.height -= top + bottom;

		if (s.width < minimum) {
			s.width = minimum;
		}

		if (s.height < minimum) {
			s.height = minimum;
		}
		
		return s;
	}

	void as_xml (xmlpp::Node *) const;
};

struct CPLSummary
{
	CPLSummary (std::string d, std::string i, std::string a, boost::filesystem::path f)
		: dcp_directory (d)
		, cpl_id (i)
		, cpl_annotation_text (a)
		, cpl_file (f)
	{}
	
	std::string dcp_directory;
	std::string cpl_id;
	std::string cpl_annotation_text;
	boost::filesystem::path cpl_file;
};

extern bool operator== (Crop const & a, Crop const & b);
extern bool operator!= (Crop const & a, Crop const & b);

enum Resolution {
	RESOLUTION_2K,
	RESOLUTION_4K
};

std::string resolution_to_string (Resolution);
Resolution string_to_resolution (std::string);

#endif
