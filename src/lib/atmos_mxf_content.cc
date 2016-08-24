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

#include "atmos_mxf_content.h"
#include "job.h"
#include "film.h"
#include "compose.hpp"
#include <dcp/atmos_asset.h>
#include <dcp/exceptions.h>
#include <libxml++/libxml++.h>

#include "i18n.h"

using std::list;
using std::string;
using boost::shared_ptr;

AtmosMXFContent::AtmosMXFContent (shared_ptr<const Film> film, boost::filesystem::path path)
	: Content (film, path)
{

}

AtmosMXFContent::AtmosMXFContent (shared_ptr<const Film> film, cxml::ConstNodePtr node, int)
	: Content (film, node)
{

}

bool
AtmosMXFContent::valid_mxf (boost::filesystem::path path)
{
	try {
		shared_ptr<dcp::AtmosAsset> a (new dcp::AtmosAsset (path));
		return true;
	} catch (dcp::MXFFileError& e) {

	} catch (dcp::DCPReadError& e) {

	}

	return false;
}

void
AtmosMXFContent::examine (shared_ptr<Job> job)
{
	job->set_progress_unknown ();
	Content::examine (job);
	shared_ptr<dcp::AtmosAsset> a (new dcp::AtmosAsset (path(0)));

	{
		boost::mutex::scoped_lock lm (_mutex);
		_length = a->intrinsic_duration ();
	}
}

string
AtmosMXFContent::summary () const
{
	return String::compose (_("%1 [Atmos]"), path_summary());
}

void
AtmosMXFContent::as_xml (xmlpp::Node* node, bool with_paths) const
{
	node->add_child("Type")->add_child_text ("AtmosMXF");
	Content::as_xml (node, with_paths);
}

DCPTime
AtmosMXFContent::full_length () const
{
	FrameRateChange const frc (active_video_frame_rate(), film()->video_frame_rate());
	return DCPTime::from_frames (llrint (_length * frc.factor()), film()->video_frame_rate());
}
