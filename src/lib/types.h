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


/** The version number of the protocol used to communicate
 *  with servers.  Intended to be bumped when incompatibilities
 *  are introduced.  v2 uses 64+n
 *
 *  64 - first version used
 *  65 - v2.16.0 - checksums added to communication
 *  66 - v2.17.x - J2KBandwidth -> VideoBitRate in metadata
 */
#define SERVER_LINK_VERSION (64+2)

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
	BY_LENGTH,
	CUSTOM
};


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


#endif
