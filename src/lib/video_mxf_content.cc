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


#include "film.h"
#include "film_util.h"
#include "frame_rate_change.h"
#include "job.h"
#include "overlaps.h"
#include "text_content.h"
#include "video_content.h"
#include "video_mxf_content.h"
#include "video_mxf_examiner.h"
#include <asdcp/KM_log.h>
#include <dcp/exceptions.h>
#include <dcp/mono_j2k_picture_asset.h>
#include <dcp/scope_guard.h>
#include <dcp/stereo_j2k_picture_asset.h>
#include <libxml++/libxml++.h>
#include <algorithm>

#include "i18n.h"


using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using boost::optional;
using namespace dcpomatic;


VideoMXFContent::VideoMXFContent(boost::filesystem::path path)
	: Content(path)
{

}


VideoMXFContent::VideoMXFContent(cxml::ConstNodePtr node, boost::optional<boost::filesystem::path> film_directory, int version)
	: Content(node, film_directory)
{
	video = VideoContent::from_xml(this, node, version, VideoRange::FULL);
	if (auto standard = node->optional_string_child("MonoStandard")) {
		_mono_standard = *standard == "Interop" ? dcp::Standard::INTEROP : dcp::Standard::SMPTE;
	}
}


bool
VideoMXFContent::valid_mxf(boost::filesystem::path path)
{
	Kumu::DefaultLogSink().UnsetFilterFlag(Kumu::LOG_ALLOW_ALL);
	dcp::ScopeGuard sg = []() {
		Kumu::DefaultLogSink().SetFilterFlag(Kumu::LOG_ALLOW_ALL);
	};

	try {
		dcp::MonoJ2KPictureAsset mp(path);
		return true;
	} catch (dcp::MXFFileError& e) {

	} catch (dcp::ReadError& e) {

	}

	try {
		Kumu::DefaultLogSink().SetFilterFlag(0);
		dcp::StereoJ2KPictureAsset sp(path);
		return true;
	} catch (dcp::MXFFileError& e) {

	} catch (dcp::ReadError& e) {

	}

	return false;
}


void
VideoMXFContent::examine(shared_ptr<const Film> film, shared_ptr<Job> job, bool tolerant)
{
	job->set_progress_unknown();

	Content::examine(film, job, tolerant);

	video.reset(new VideoContent(this));
	auto examiner = make_shared<VideoMXFExaminer>(shared_from_this());
	video->take_from_examiner(film, examiner);
	video->unset_colour_conversion();

	optional<dcp::Standard> mono_standard;
	try {
		mono_standard = dcp::MonoJ2KPictureAsset(path(0)).standard();
	} catch (dcp::MXFFileError&) {
		/* It is stereo */
	} catch (dcp::ReadError&) {
		/* It is stereo */
	}

	boost::mutex::scoped_lock lm(_mutex);
	_mono_standard = mono_standard;
}


string
VideoMXFContent::summary() const
{
	return fmt::format(_("{} [video]"), path_summary());
}


string
VideoMXFContent::technical_summary() const
{
	return Content::technical_summary() + " - " + video->technical_summary();
}


string
VideoMXFContent::identifier() const
{
	return Content::identifier() + "_" + video->identifier();
}


void
VideoMXFContent::as_xml(xmlpp::Element* element, bool with_paths, PathBehaviour path_behaviour, optional<boost::filesystem::path> film_directory) const
{
	cxml::add_text_child(element, "Type", "VideoMXF");
	Content::as_xml(element, with_paths, path_behaviour, film_directory);
	video->as_xml(element);
	boost::mutex::scoped_lock lm(_mutex);
	if (_mono_standard) {
		cxml::add_text_child(element, "MonoStandard", *_mono_standard == dcp::Standard::INTEROP ? "Interop" : "SMPTE");
	}
}


DCPTime
VideoMXFContent::full_length(shared_ptr<const Film> film) const
{
	FrameRateChange const frc(film, shared_from_this());
	return DCPTime::from_frames(llrint(video->length_after_3d_combine() * frc.factor()), film->video_frame_rate());
}


DCPTime
VideoMXFContent::approximate_length() const
{
	return DCPTime::from_frames(video->length_after_3d_combine(), 24);
}


void
VideoMXFContent::add_properties(shared_ptr<const Film> film, list<UserProperty>& p) const
{
	Content::add_properties(film, p);
	video->add_properties(p);
}


bool
VideoMXFContent::can_reference_video(shared_ptr<const Film> film, string& why_not) const
{
	if (!video || !video->use()) {
		why_not = "it has no video in use";
		return false;
	}

	optional<dcp::Standard> standard;
	{
		boost::mutex::scoped_lock lm(_mutex);
		standard = _mono_standard;
	}

	if (!standard) {
		why_not = "it is 3D, or was examined by an earlier version";
		return false;
	}

	if (film->three_d()) {
		why_not = "the film is 3D";
		return false;
	}

	if ((*standard == dcp::Standard::INTEROP) != film->interop()) {
		why_not = *standard == dcp::Standard::INTEROP ? "it is Interop and the film is SMPTE" : "it is SMPTE and the film is Interop";
		return false;
	}

	FrameRateChange const frc(film, shared_from_this());
	if (frc.change_speed() || frc.skip() || frc.repeat() != 1) {
		why_not = "its frame rate is not the film's";
		return false;
	}

	auto const rate = static_cast<double>(film->video_frame_rate());
	if (trim_start() != trim_start().round(rate) || trim_end() != trim_end().round(rate)) {
		why_not = "its trims are not whole frames";
		return false;
	}

	if (film->video_encoding() != VideoEncoding::JPEG2000 || film->reencode_j2k()) {
		why_not = "the film encodes its picture again";
		return false;
	}

	if (film->encrypt_picture()) {
		why_not = "the film encrypts its picture";
		return false;
	}

	if (film->frame_size() != video->size()) {
		why_not = "its size is not the film's";
		return false;
	}

	if (!(video->actual_crop() == Crop()) || video->custom_ratio() || video->custom_size()) {
		why_not = "it is cropped or scaled";
		return false;
	}

	if (video->fade_in() != 0 || video->fade_out() != 0 || video->colour_conversion()) {
		why_not = "its fades or colour conversion are set";
		return false;
	}

	auto const period = DCPTimePeriod(position(), end(film));

	auto part = [](shared_ptr<const Content> c) {
		return static_cast<bool>(c->video) && c->video->use();
	};

	auto const others = dcpomatic::overlaps(film, film->content(), part, period.from, period.to);
	if (others.size() != 1 || others.front().get() != this) {
		why_not = "other video overlaps it";
		return false;
	}

	auto burnt = [](shared_ptr<const Content> c) {
		return std::any_of(c->text.begin(), c->text.end(), [](shared_ptr<TextContent> t) { return t->use() && t->burn(); });
	};

	if (!dcpomatic::overlaps(film, film->content(), burnt, period.from, period.to).empty()) {
		why_not = "text is burnt into it";
		return false;
	}

	/* The film's reels are never changed for a file: it can be in one or several of them, each with its own entry point
	 * and duration in the CPL, but it must fill every reel that it is in.
	 */
	if (!dcpomatic::film::reels_fit_inside(film, period)) {
		why_not = "it does not fill the film's reels that it is in";
		return false;
	}

	return true;
}
