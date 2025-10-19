/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#include "dcp_content.h"
#include "dcpomatic_assert.h"
#include "show_playlist_entry.h"
#include <fmt/format.h>


using std::dynamic_pointer_cast;
using std::shared_ptr;
using std::string;
using boost::optional;


ShowPlaylistEntry::ShowPlaylistEntry(shared_ptr<Content> content, optional<float> crop_to_ratio)
	: _kind(dcp::ContentKind::FEATURE)
	, _crop_to_ratio(crop_to_ratio)
{
	if (auto dcp = dynamic_pointer_cast<DCPContent>(content)) {
		DCPOMATIC_ASSERT(dcp->cpl());
		_uuid = *dcp->cpl();
		_name = dcp->name();
		_kind = dcp->content_kind().get_value_or(dcp::ContentKind::FEATURE);
		_encrypted = dcp->encrypted();
	} else {
		_uuid = content->digest();
		_name = content->path(0).filename().string();
		_encrypted = false;
	}

	auto const hmsf = content->approximate_length().split(24);
	_approximate_length = fmt::format("{:02d}:{:02d}:{:02d}", hmsf.h, hmsf.m, hmsf.s);
}


ShowPlaylistEntry::ShowPlaylistEntry(
	string uuid,
	string name,
	dcp::ContentKind kind,
	string approximate_length,
	bool encrypted,
	boost::optional<float> crop_to_ratio
	)
	: _uuid(std::move(uuid))
	, _name(std::move(name))
	, _kind(std::move(kind))
	, _approximate_length(std::move(approximate_length))
	, _encrypted(encrypted)
	, _crop_to_ratio(crop_to_ratio)
{

}


string
ShowPlaylistEntry::uuid() const
{
	return _uuid;
}


string
ShowPlaylistEntry::name() const
{
	return _name;
}


dcp::ContentKind
ShowPlaylistEntry::kind() const
{
	return _kind;
}


bool
ShowPlaylistEntry::encrypted() const
{
	return _encrypted;
}


string
ShowPlaylistEntry::approximate_length() const
{
	return _approximate_length;
}


optional<float>
ShowPlaylistEntry::crop_to_ratio() const
{
	return _crop_to_ratio;
}


void
ShowPlaylistEntry::set_crop_to_ratio(optional<float> ratio)
{
	_crop_to_ratio = ratio;
}


nlohmann::json
ShowPlaylistEntry::as_json() const
{
	nlohmann::json json;
	json["uuid"] = _uuid;
	json["name"] = _name;
	json["kind"] = _kind.name();
	json["encrypted"] = _encrypted;
	json["approximate_length"] = _approximate_length;
	if (_crop_to_ratio) {
		json["crop_to_ratio"] = static_cast<int>(std::round(*_crop_to_ratio * 100));
	}
	return json;
}



bool
operator==(ShowPlaylistEntry const& a, ShowPlaylistEntry const& b)
{
	return a.uuid() == b.uuid() &&
		a.name() == b.name() &&
		a.kind() == b.kind() &&
		a.approximate_length() == b.approximate_length() &&
		a.encrypted() == b.encrypted() &&
		a.crop_to_ratio() == b.crop_to_ratio();

}


bool
operator!=(ShowPlaylistEntry const& a, ShowPlaylistEntry const& b)
{
	return !(a == b);
}

