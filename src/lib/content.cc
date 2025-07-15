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


/** @file  src/lib/content.cc
 *  @brief Content class.
 */


#include "audio_content.h"
#include "change_signaller.h"
#include "content.h"
#include "content_factory.h"
#include "exceptions.h"
#include "film.h"
#include "font.h"
#include "job.h"
#include "text_content.h"
#include "util.h"
#include "video_content.h"
#include <dcp/locale_convert.h>
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <fmt/format.h>
#include <boost/thread/mutex.hpp>
#include <iostream>

#include "i18n.h"


using std::cout;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using dcp::locale_convert;
using namespace dcpomatic;


Content::Content(DCPTime p)
	: _position(p)
{

}


Content::Content(boost::filesystem::path p)
{
	add_path(p);
}


Content::Content(cxml::ConstNodePtr node, boost::optional<boost::filesystem::path> film_directory)
{
	for (auto i: node->node_children("Path")) {
		if (film_directory) {
			_paths.push_back(boost::filesystem::weakly_canonical(boost::filesystem::absolute(i->content(), *film_directory)));
		} else {
			_paths.push_back(i->content());
		}
		auto const mod = i->optional_number_attribute<time_t>("mtime");
		if (mod) {
			_last_write_times.push_back(*mod);
		} else {
			boost::system::error_code ec;
			auto last_write = dcp::filesystem::last_write_time(i->content(), ec);
			_last_write_times.push_back(ec ? 0 : last_write);
		}
	}
	_digest = node->optional_string_child("Digest").get_value_or("X");
	_position = DCPTime(node->number_child<DCPTime::Type>("Position"));
	_trim_start = ContentTime(node->number_child<ContentTime::Type>("TrimStart"));
	_trim_end = ContentTime(node->number_child<ContentTime::Type>("TrimEnd"));
	_video_frame_rate = node->optional_number_child<double>("VideoFrameRate");
}


Content::Content(vector<shared_ptr<Content>> c)
	: _position(c.front()->position())
	, _trim_start(c.front()->trim_start())
	, _trim_end(c.back()->trim_end())
	, _video_frame_rate(c.front()->video_frame_rate())
{
	for (size_t i = 0; i < c.size(); ++i) {
		if (i > 0 && c[i]->trim_start() > ContentTime()) {
			throw JoinError(_("Only the first piece of content to be joined can have a start trim."));
		}

		if (i < (c.size() - 1) && c[i]->trim_end() > ContentTime()) {
			throw JoinError(_("Only the last piece of content to be joined can have an end trim."));
		}

		if (
			(_video_frame_rate && !c[i]->_video_frame_rate) ||
			(!_video_frame_rate && c[i]->_video_frame_rate)
			) {
			throw JoinError(_("Content to be joined must have the same video frame rate"));
		}

		if (_video_frame_rate && fabs(_video_frame_rate.get() - c[i]->_video_frame_rate.get()) > VIDEO_FRAME_RATE_EPSILON) {
			throw JoinError(_("Content to be joined must have the same video frame rate"));
		}

		for (size_t j = 0; j < c[i]->number_of_paths(); ++j) {
			_paths.push_back(c[i]->path(j));
			_last_write_times.push_back(c[i]->_last_write_times[j]);
		}
	}
}


void
Content::as_xml(xmlpp::Element* element, bool with_paths, PathBehaviour path_behaviour, optional<boost::filesystem::path> film_directory) const
{
	boost::mutex::scoped_lock lm(_mutex);

	if (with_paths) {
		for (size_t i = 0; i < _paths.size(); ++i) {
			auto path = _paths[i];
			if (path_behaviour == PathBehaviour::MAKE_RELATIVE) {
				DCPOMATIC_ASSERT(film_directory);
				path = boost::filesystem::relative(path, *film_directory);
			}
			auto p = cxml::add_child(element, "Path");
			p->add_child_text(path.string());
			p->set_attribute("mtime", fmt::to_string(_last_write_times[i]));
		}
	}
	cxml::add_text_child(element, "Digest", _digest);
	cxml::add_text_child(element, "Position", fmt::to_string(_position.get()));
	cxml::add_text_child(element, "TrimStart", fmt::to_string(_trim_start.get()));
	cxml::add_text_child(element, "TrimEnd", fmt::to_string(_trim_end.get()));
	if (_video_frame_rate) {
		cxml::add_text_child(element, "VideoFrameRate", fmt::to_string(_video_frame_rate.get()));
	}
}


string
Content::calculate_digest() const
{
	/* Some content files are very big, so we use a poor man's
	   digest here: a digest of the first and last 1e6 bytes with the
	   size of the first file tacked on the end as a string.
	*/
	return simple_digest(paths());
}


void
Content::examine(shared_ptr<const Film>, shared_ptr<Job> job, bool)
{
	if (job) {
		job->sub(_("Computing digest"));
	}

	auto const d = calculate_digest();

	boost::mutex::scoped_lock lm(_mutex);
	_digest = d;

	_last_write_times.clear();
	for (auto i: _paths) {
		boost::system::error_code ec;
		auto last_write = dcp::filesystem::last_write_time(i, ec);
		_last_write_times.push_back(ec ? 0 : last_write);
	}
}


void
Content::signal_change(ChangeType c, int p)
{
	try {
		if (c == ChangeType::PENDING || c == ChangeType::CANCELLED) {
			Change(c, p, _change_signals_frequent);
		} else {
			emit(boost::bind(boost::ref(Change), c, p, _change_signals_frequent));
		}
	} catch (std::bad_weak_ptr &) {
		/* This must be during construction; never mind */
	}
}


void
Content::set_position(shared_ptr<const Film> film, DCPTime p, bool force_emit)
{
	/* video and audio content can modify its position */

	if (video) {
		video->modify_position(film, p);
	}

	/* Only allow the audio to modify if we have no video;
	   sometimes p can't be on an integer video AND audio frame,
	   and in these cases we want the video constraint to be
	   satisfied since (I think) the audio code is better able to
	   cope.
	*/
	if (!video && audio) {
		audio->modify_position(film, p);
	}

	ContentChangeSignaller cc(this, ContentProperty::POSITION);

	{
		boost::mutex::scoped_lock lm(_mutex);
		if (p == _position && !force_emit) {
			cc.abort();
			return;
		}

		_position = p;
	}
}


void
Content::set_trim_start(shared_ptr<const Film> film, ContentTime t)
{
	DCPOMATIC_ASSERT(t.get() >= 0);

	/* video and audio content can modify its start trim */

	if (video) {
		video->modify_trim_start(t);
	}

	/* See note in ::set_position */
	if (!video && audio) {
		audio->modify_trim_start(film, t);
	}

	ContentChangeSignaller cc(this, ContentProperty::TRIM_START);

	{
		boost::mutex::scoped_lock lm(_mutex);
		if (_trim_start == t) {
			cc.abort();
		} else {
			_trim_start = t;
		}
	}
}


void
Content::set_trim_end(ContentTime t)
{
	DCPOMATIC_ASSERT(t.get() >= 0);

	ContentChangeSignaller cc(this, ContentProperty::TRIM_END);

	{
		boost::mutex::scoped_lock lm(_mutex);
		_trim_end = t;
	}
}


shared_ptr<Content>
Content::clone() const
{
	/* This is a bit naughty, but I can't think of a compelling reason not to do it ... */
	xmlpp::Document doc;
	auto node = doc.create_root_node("Content");
	as_xml(node, true, PathBehaviour::KEEP_ABSOLUTE, {});

	/* notes is unused here (we assume) */
	list<string> notes;
	return content_factory(make_shared<cxml::Node>(node), {}, Film::current_state_version, notes);
}


string
Content::technical_summary() const
{
	auto s = fmt::format("{} {} {}", path_summary(), digest(), position().seconds());
	if (_video_frame_rate) {
		s += fmt::format(" {}", *_video_frame_rate);
	}
	return s;
}


DCPTime
Content::length_after_trim(shared_ptr<const Film> film) const
{
	auto length = max(DCPTime(), full_length(film) - DCPTime(trim_start() + trim_end(), film->active_frame_rate_change(position())));
	if (video) {
		length = length.round(film->video_frame_rate());
	}
	return length;
}


/** @return string which changes when something about this content changes which affects
 *  the appearance of its video.
 */
string
Content::identifier() const
{
	char buffer[256];
	snprintf(
		buffer, sizeof(buffer), "%s_%" PRId64 "_%" PRId64 "_%" PRId64,
		Content::digest().c_str(), position().get(), trim_start().get(), trim_end().get()
		);
	return buffer;
}


void
Content::set_paths(vector<boost::filesystem::path> paths)
{
	ContentChangeSignaller cc(this, ContentProperty::PATH);

	{
		boost::mutex::scoped_lock lm(_mutex);
		_paths.clear();
		for (auto path: paths) {
			_paths.push_back(boost::filesystem::canonical(path));
		}
		_last_write_times.clear();
		for (auto i: _paths) {
			boost::system::error_code ec;
			auto last_write = dcp::filesystem::last_write_time(i, ec);
			_last_write_times.push_back(ec ? 0 : last_write);
		}
	}
}


string
Content::path_summary() const
{
	/* XXX: should handle multiple paths more gracefully */

	DCPOMATIC_ASSERT(number_of_paths());

	auto s = path(0).filename().string();
	if (number_of_paths() > 1) {
		s += " ...";
	}

	return s;
}


/** @return a list of properties that might be interesting to the user */
list<UserProperty>
Content::user_properties(shared_ptr<const Film> film) const
{
	list<UserProperty> p;
	add_properties(film, p);
	return p;
}


/** @return DCP times of points within this content where a reel split could occur */
list<DCPTime>
Content::reel_split_points(shared_ptr<const Film>) const
{
	list<DCPTime> t;
	/* This is only called for video content and such content has its position forced
	   to start on a frame boundary.
	*/
	t.push_back(position());
	return t;
}


void
Content::set_video_frame_rate(shared_ptr<const Film> film, double r)
{
	ContentChangeSignaller cc(this, ContentProperty::VIDEO_FRAME_RATE);

	{
		boost::mutex::scoped_lock lm(_mutex);
		if (_video_frame_rate && fabs(r - *_video_frame_rate) < VIDEO_FRAME_RATE_EPSILON) {
			cc.abort();
		}
		_video_frame_rate = r;
	}

	/* Make sure trim is still on a frame boundary */
	if (video) {
		set_trim_start(film, trim_start());
	}
}


void
Content::unset_video_frame_rate()
{
	ContentChangeSignaller cc(this, ContentProperty::VIDEO_FRAME_RATE);

	{
		boost::mutex::scoped_lock lm(_mutex);
		_video_frame_rate = optional<double>();
	}
}


double
Content::active_video_frame_rate(shared_ptr<const Film> film) const
{
	{
		boost::mutex::scoped_lock lm(_mutex);
		if (_video_frame_rate) {
			return _video_frame_rate.get();
		}
	}

	/* No frame rate specified, so assume this content has been
	   prepared for any concurrent video content or perhaps
	   just the DCP rate.
	*/
	return film->active_frame_rate_change(position()).source;
}


void
Content::add_properties(shared_ptr<const Film>, list<UserProperty>& p) const
{
	auto paths_to_show = std::min(number_of_paths(), size_t{8});
	string paths = "";
	for (auto i = size_t{0}; i < paths_to_show; ++i) {
		paths += path(i).string();
		if (i < (paths_to_show - 1)) {
			paths += "\n";
		}
	}
	if (paths_to_show < number_of_paths()) {
		paths += fmt::format("... and {} more", number_of_paths() - paths_to_show);
	}
	p.push_back(
		UserProperty(
			UserProperty::GENERAL,
			paths_to_show > 1 ? _("Filenames") : _("Filename"),
			paths
			)
		);

	if (_video_frame_rate) {
		if (video) {
			p.push_back(
				UserProperty(
					UserProperty::VIDEO,
					_("Frame rate"),
					locale_convert<string>(_video_frame_rate.get(), 5),
					_("frames per second")
					)
				);
		} else {
			p.push_back(
				UserProperty(
					UserProperty::GENERAL,
					_("Prepared for video frame rate"),
					locale_convert<string>(_video_frame_rate.get(), 5),
					_("frames per second")
					)
				);
		}
	}
}


/** Take settings from the given content if it is of the correct type */
void
Content::take_settings_from(shared_ptr<const Content> c)
{
	if (video && c->video) {
		video->take_settings_from(c->video);
	}
	if (audio && c->audio) {
		audio->take_settings_from(c->audio);
	}

	auto i = text.begin();
	auto j = c->text.begin();
	while (i != text.end() && j != c->text.end()) {
		(*i)->take_settings_from(*j);
		++i;
		++j;
 	}
}


shared_ptr<TextContent>
Content::only_text() const
{
	DCPOMATIC_ASSERT(text.size() < 2);
	if (text.empty()) {
		return {};
	}
	return text.front();
}


shared_ptr<TextContent>
Content::text_of_original_type(TextType type) const
{
	for (auto i: text) {
		if (i->original_type() == type) {
			return i;
		}
	}

	return {};
}


void
Content::add_path(boost::filesystem::path p)
{
	boost::mutex::scoped_lock lm(_mutex);
	_paths.push_back(boost::filesystem::canonical(p));
	boost::system::error_code ec;
	auto last_write = dcp::filesystem::last_write_time(p, ec);
	_last_write_times.push_back(ec ? 0 : last_write);
}


bool
Content::changed() const
{
	bool write_time_changed = false;
	for (auto i = 0U; i < _paths.size(); ++i) {
		if (dcp::filesystem::last_write_time(_paths[i]) != last_write_time(i)) {
			write_time_changed = true;
			break;
		}
	}

	return (write_time_changed || calculate_digest() != digest());
}


bool
Content::has_mapped_audio() const
{
	return audio && !audio->mapping().mapped_output_channels().empty();
}


vector<boost::filesystem::path>
Content::font_paths() const
{
	vector<boost::filesystem::path> paths;

	for (auto i: text) {
		for (auto j: i->fonts()) {
			if (j->file()) {
				paths.push_back(*j->file());
			}
		}
	}

	return paths;
}


void
Content::replace_font_path(boost::filesystem::path old_path, boost::filesystem::path new_path)
{
	for (auto i: text) {
		for (auto j: i->fonts()) {
			if (j->file() && *j->file() == old_path) {
				j->set_file(new_path);
			}
		}
	}
}

