/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_TYPES_H
#define DCPOMATIC_TYPES_H

#include "position.h"
#include "rect.h"
#include <dcp/util.h>
#include <vector>
#include <stdint.h>

class Content;
class VideoContent;
class AudioContent;
class TextContent;
class FFmpegContent;

namespace cxml {
	class Node;
}

namespace xmlpp {
	class Node;
}

/** The version number of the protocol used to communicate
 *  with servers.  Intended to be bumped when incompatibilities
 *  are introduced.  v2 uses 64+n
 *
 *  64 - first version used
 *  65 - v2.16.0 - checksums added to communication
 */
#define SERVER_LINK_VERSION (64+1)

/** A film of F seconds at f FPS will be Ff frames;
    Consider some delta FPS d, so if we run the same
    film at (f + d) FPS it will last F(f + d) seconds.

    Hence the difference in length over the length of the film will
    be F(f + d) - Ff frames
    = Ff + Fd - Ff frames
    = Fd frames
    = Fd/f seconds

    So if we accept a difference of 1 frame, ie 1/f seconds, we can
    say that

    1/f = Fd/f
    ie 1 = Fd
    ie d = 1/F

    So for a 3hr film, ie F = 3 * 60 * 60 = 10800, the acceptable
    FPS error is 1/F ~= 0.0001 ~= 1e-4
*/
#define VIDEO_FRAME_RATE_EPSILON (1e-4)

/** Port on which EncodeServer listens for frame encoding requests */
#define ENCODE_FRAME_PORT (Config::instance()->server_port_base())
/** Port on which EncodeServer listens for DCPOMATIC_HELLO from masters */
#define HELLO_PORT (Config::instance()->server_port_base()+1)
/** Port on which EncodeServerFinder in the main DCP-o-matic listens for replies to DCPOMATIC_HELLO from servers */
#define MAIN_SERVER_PRESENCE_PORT (Config::instance()->server_port_base()+2)
/** Port on which EncodeServerFinder in the batch converter listens for replies to DCPOMATIC_HELLO from servers */
#define BATCH_SERVER_PRESENCE_PORT (Config::instance()->server_port_base()+3)
/** Port on which batch converter listens for job requests */
#define BATCH_JOB_PORT (Config::instance()->server_port_base()+4)
/** Port on which player listens for play requests */
#define PLAYER_PLAY_PORT (Config::instance()->server_port_base()+5)

typedef std::vector<std::shared_ptr<Content>> ContentList;
typedef std::vector<std::shared_ptr<FFmpegContent>> FFmpegContentList;

typedef int64_t Frame;

enum class VideoFrameType
{
	TWO_D,
	/** `True' 3D content, e.g. 3D DCPs */
	THREE_D,
	THREE_D_LEFT_RIGHT,
	THREE_D_TOP_BOTTOM,
	THREE_D_ALTERNATE,
	/** This content is all the left frames of some 3D */
	THREE_D_LEFT,
	/** This content is all the right frames of some 3D */
	THREE_D_RIGHT
};

std::string video_frame_type_to_string (VideoFrameType);
VideoFrameType string_to_video_frame_type (std::string);

enum class Eyes
{
	BOTH,
	LEFT,
	RIGHT,
	COUNT
};

enum class Part
{
	LEFT_HALF,
	RIGHT_HALF,
	TOP_HALF,
	BOTTOM_HALF,
	WHOLE
};

enum class ReelType
{
	SINGLE,
	BY_VIDEO_CONTENT,
	BY_LENGTH
};

enum class ChangeType
{
	PENDING,
	DONE,
	CANCELLED
};


enum class VideoRange
{
	FULL, ///< full,  or "JPEG" (0-255 for 8-bit)
	VIDEO ///< video, or "MPEG" (16-235 for 8-bit)
};

extern std::string video_range_to_string (VideoRange r);
extern VideoRange string_to_video_range (std::string s);


/** Type of captions.
 *
 *  The generally accepted definitions seem to be:
 *  - subtitles: text for an audience who doesn't speak the film's language
 *  - captions:  text for a hearing-impaired audience
 *  - open:      on-screen
 *  - closed:    only visible by some audience members
 *
 *  At the moment DoM supports open subtitles and closed captions.
 *
 *  There is some use of the word `subtitle' in the code which may mean
 *  caption in some contexts.
 */
enum class TextType
{
	UNKNOWN,
	OPEN_SUBTITLE,
	CLOSED_CAPTION,
	COUNT
};

extern std::string text_type_to_string (TextType t);
extern std::string text_type_to_name (TextType t);
extern TextType string_to_text_type (std::string s);

enum class ExportFormat
{
	PRORES,
	H264_AAC,
	H264_PCM,
	SUBTITLES_DCP
};

/** @struct Crop
 *  @brief A description of the crop of an image or video.
 */
struct Crop
{
	Crop () : left (0), right (0), top (0), bottom (0) {}
	Crop (int l, int r, int t, int b) : left (l), right (r), top (t), bottom (b) {}
	explicit Crop (std::shared_ptr<cxml::Node>);

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

extern bool operator== (Crop const & a, Crop const & b);
extern bool operator!= (Crop const & a, Crop const & b);

struct CPLSummary
{
	CPLSummary (boost::filesystem::path p);

	CPLSummary (std::string d, std::string i, std::string a, boost::filesystem::path f, bool e, time_t t)
		: dcp_directory (d)
		, cpl_id (i)
		, cpl_annotation_text (a)
		, cpl_file (f)
		, encrypted (e)
		, last_write_time (t)
	{}

	std::string dcp_directory;
	std::string cpl_id;
	boost::optional<std::string> cpl_annotation_text;
	boost::filesystem::path cpl_file;
	/** true if this CPL has any encrypted assets */
	bool encrypted;
	time_t last_write_time;
};

enum class Resolution {
	TWO_K,
	FOUR_K
};

std::string resolution_to_string (Resolution);
Resolution string_to_resolution (std::string);

enum class FileTransferProtocol {
	SCP,
	FTP
};

enum class EmailProtocol {
	AUTO,
	PLAIN,
	STARTTLS,
	SSL
};


class NamedChannel
{
public:
	NamedChannel (std::string name_, int index_)
		: name(name_)
		, index(index_)
	{}

	std::string name;
	int index;
};


bool operator== (NamedChannel const& a, NamedChannel const& b);

#endif
