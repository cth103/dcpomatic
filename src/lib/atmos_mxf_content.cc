/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


#include "atmos_content.h"
#include "atmos_mxf_content.h"
#include "film.h"
#include "job.h"
#include <asdcp/KM_log.h>
#include <dcp/atmos_asset.h>
#include <dcp/exceptions.h>
#include <libxml++/libxml++.h>

#include "i18n.h"


using std::list;
using std::make_shared;
using std::string;
using std::shared_ptr;
using boost::optional;
using namespace dcpomatic;


AtmosMXFContent::AtmosMXFContent (boost::filesystem::path path)
	: Content (path)
{

}


AtmosMXFContent::AtmosMXFContent(cxml::ConstNodePtr node, boost::optional<boost::filesystem::path> film_directory, int)
	: Content(node, film_directory)
{
	atmos = AtmosContent::from_xml (this, node);
}


bool
AtmosMXFContent::valid_mxf (boost::filesystem::path path)
{
	Kumu::DefaultLogSink().UnsetFilterFlag(Kumu::LOG_ALLOW_ALL);

	try {
		dcp::AtmosAsset a (path);
		return true;
	} catch (dcp::MXFFileError& e) {

	} catch (dcp::ReadError& e) {

	}

	Kumu::DefaultLogSink().SetFilterFlag(Kumu::LOG_ALLOW_ALL);

	return false;
}


void
AtmosMXFContent::examine(shared_ptr<const Film> film, shared_ptr<Job> job, bool tolerant)
{
	job->set_progress_unknown ();
	Content::examine(film, job, tolerant);
	auto a = make_shared<dcp::AtmosAsset>(path(0));

	{
		boost::mutex::scoped_lock lm (_mutex);
		atmos = make_shared<AtmosContent>(this);
		atmos->set_length (a->intrinsic_duration());
		atmos->set_edit_rate (a->edit_rate());
	}
}


string
AtmosMXFContent::summary () const
{
	return fmt::format(_("{} [Atmos]"), path_summary());
}


void
AtmosMXFContent::as_xml(xmlpp::Element* element, bool with_paths, PathBehaviour path_behaviour, optional<boost::filesystem::path> film_directory) const
{
	cxml::add_text_child(element, "Type", "AtmosMXF");
	Content::as_xml(element, with_paths, path_behaviour, film_directory);
	atmos->as_xml(element);
}


DCPTime
AtmosMXFContent::full_length (shared_ptr<const Film> film) const
{
	FrameRateChange const frc (film, shared_from_this());
	return DCPTime::from_frames (llrint(atmos->length() * frc.factor()), film->video_frame_rate());
}


DCPTime
AtmosMXFContent::approximate_length () const
{
	return DCPTime::from_frames (atmos->length(), 24);
}
