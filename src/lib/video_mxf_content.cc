/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "video_mxf_examiner.h"
#include "video_mxf_content.h"
#include "video_content.h"
#include "job.h"
#include "film.h"
#include "compose.hpp"
#include <dcp/mono_picture_asset.h>
#include <dcp/stereo_picture_asset.h>
#include <dcp/exceptions.h>
#include <libxml++/libxml++.h>

#include "i18n.h"

using std::list;
using std::string;
using boost::shared_ptr;

VideoMXFContent::VideoMXFContent (shared_ptr<const Film> film, boost::filesystem::path path)
	: Content (film, path)
{

}

VideoMXFContent::VideoMXFContent (shared_ptr<const Film> film, cxml::ConstNodePtr node, int version)
	: Content (film, node)
{
	video = VideoContent::from_xml (this, node, version);
}

bool
VideoMXFContent::valid_mxf (boost::filesystem::path path)
{
	try {
		shared_ptr<dcp::MonoPictureAsset> mp (new dcp::MonoPictureAsset (path));
		return true;
	} catch (dcp::MXFFileError& e) {

	} catch (dcp::DCPReadError& e) {

	}

	try {
		shared_ptr<dcp::StereoPictureAsset> sp (new dcp::StereoPictureAsset (path));
		return true;
	} catch (dcp::MXFFileError& e) {

	} catch (dcp::DCPReadError& e) {

	}

	return false;
}

void
VideoMXFContent::examine (shared_ptr<Job> job)
{
	job->set_progress_unknown ();

	Content::examine (job);

	video.reset (new VideoContent (this));
	shared_ptr<VideoMXFExaminer> examiner (new VideoMXFExaminer (shared_from_this ()));
	video->take_from_examiner (examiner);
}

string
VideoMXFContent::summary () const
{
	return String::compose (_("%1 [video]"), path_summary());
}

string
VideoMXFContent::technical_summary () const
{
	return Content::technical_summary() + " - " + video->technical_summary ();
}

string
VideoMXFContent::identifier () const
{
	return Content::identifier() + "_" + video->identifier();
}

void
VideoMXFContent::as_xml (xmlpp::Node* node, bool with_paths) const
{
	node->add_child("Type")->add_child_text ("VideoMXF");
	Content::as_xml (node, with_paths);
	video->as_xml (node);
}

DCPTime
VideoMXFContent::full_length () const
{
	FrameRateChange const frc (active_video_frame_rate(), film()->video_frame_rate());
	return DCPTime::from_frames (llrint (video->length_after_3d_combine() * frc.factor()), film()->video_frame_rate());
}

void
VideoMXFContent::add_properties (list<UserProperty>& p) const
{
	video->add_properties (p);
}

void
VideoMXFContent::set_default_colour_conversion ()
{
	video->unset_colour_conversion ();
}
