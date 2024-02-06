/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "dcp_digest_file.h"
#include <dcp/cpl.h>
#include <dcp/mxf.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/reel_smpte_subtitle_asset.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS


using std::dynamic_pointer_cast;
using std::shared_ptr;
using std::string;


template <class R, class A>
void add_asset(string film_key, shared_ptr<R> reel_asset, shared_ptr<A> asset, xmlpp::Element* reel, string name)
{
	if (asset) {
		auto out = cxml::add_child(reel, name);
		cxml::add_text_child(out, "Id", "urn:uuid:" + asset->id());
		if (reel_asset->annotation_text()) {
			cxml::add_text_child(out, "AnnotationText", reel_asset->annotation_text().get());
		}
		if (asset->key_id()) {
			cxml::add_text_child(out, "KeyId", "urn:uuid:" + asset->key_id().get());
			cxml::add_text_child(out, "Key", asset->key() ? asset->key()->hex() : film_key);
		}
	}
};


void
write_dcp_digest_file (
	boost::filesystem::path path,
	shared_ptr<dcp::CPL> cpl,
	string film_key
	)
{
	xmlpp::Document doc;
	auto root = doc.create_root_node("FHG_DCP_DIGEST", "http://www.fhg.de/2009/04/02/dcpdig");
	cxml::add_text_child(root, "InteropMode", cpl->standard() == dcp::Standard::INTEROP ? "true" : "false");
	auto composition = cxml::add_child(cxml::add_child(root, "CompositionList"), "Composition");
	cxml::add_text_child(composition, "Id", "urn:uuid:" + cpl->id());
	cxml::add_text_child(composition, "AnnotationText", cpl->annotation_text().get_value_or(""));
	cxml::add_text_child(composition, "ContentTitleText", cpl->content_title_text());
	auto reel_list = cxml::add_child(composition, "ReelList");
	for (auto in_reel: cpl->reels()) {
		auto out_reel = cxml::add_child(reel_list, "Reel");
		cxml::add_text_child(out_reel, "Id", "urn:uuid:" + in_reel->id());
		cxml::add_child(out_reel, "AnnotationText");
		if (in_reel->main_picture()) {
			add_asset(film_key, in_reel->main_picture(), in_reel->main_picture()->asset(), out_reel, "MainPicture");
		}
		if (in_reel->main_sound()) {
			add_asset(film_key, in_reel->main_sound(), in_reel->main_sound()->asset(), out_reel, "MainSound");
		}
		if (auto smpte_sub = dynamic_pointer_cast<dcp::ReelSMPTESubtitleAsset>(in_reel->main_subtitle())) {
			add_asset(film_key, smpte_sub, smpte_sub->smpte_asset(), out_reel, "MainSubtitle");
		}
	}
	doc.write_to_file_formatted(path.string());
}

