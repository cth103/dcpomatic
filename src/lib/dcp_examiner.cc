/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#include "config.h"
#include "constants.h"
#include "dcp_content.h"
#include "dcp_examiner.h"
#include "dcpomatic_log.h"
#include "exceptions.h"
#include "font_id_allocator.h"
#include "image.h"
#include "text_content.h"
#include "util.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/mono_j2k_picture_asset.h>
#include <dcp/mono_j2k_picture_asset_reader.h>
#include <dcp/mono_j2k_picture_frame.h>
#include <dcp/mono_mpeg2_picture_asset.h>
#include <dcp/mpeg2_transcode.h>
#include <dcp/reel.h>
#include <dcp/reel_atmos_asset.h>
#include <dcp/reel_markers_asset.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_text_asset.h>
#include <dcp/search.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_reader.h>
#include <dcp/stereo_j2k_picture_asset.h>
#include <dcp/stereo_j2k_picture_asset_reader.h>
#include <dcp/stereo_j2k_picture_frame.h>
#include <dcp/text_asset.h>
#include <iostream>

#include "i18n.h"


using std::cout;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;


DCPExaminer::DCPExaminer (shared_ptr<const DCPContent> content, bool tolerant)
{
	shared_ptr<dcp::CPL> selected_cpl;

	auto cpls = dcp::find_and_resolve_cpls (content->directories(), tolerant);

	if (content->cpl ()) {
		/* Use the CPL that was specified, or that the content was using before */
		auto iter = std::find_if(cpls.begin(), cpls.end(), [content](shared_ptr<dcp::CPL> cpl) { return cpl->id() == content->cpl().get(); });
		if (iter == cpls.end()) {
			throw CPLNotFoundError(content->cpl().get());
		}
		selected_cpl = *iter;
	} else {
		/* Choose the CPL with the fewest unsatisfied references */

		int least_unsatisfied = INT_MAX;

		for (auto cpl: cpls) {
			int unsatisfied = 0;
			for (auto const& reel: cpl->reels()) {
				if (reel->main_picture() && !reel->main_picture()->asset_ref().resolved()) {
					++unsatisfied;
				}
				if (reel->main_sound() && !reel->main_sound()->asset_ref().resolved()) {
					++unsatisfied;
				}
				if (reel->main_subtitle() && !reel->main_subtitle()->asset_ref().resolved()) {
					++unsatisfied;
				}
				if (reel->main_caption() && !reel->main_caption()->asset_ref().resolved()) {
					++unsatisfied;
				}
				if (reel->atmos() && !reel->atmos()->asset_ref().resolved()) {
					++unsatisfied;
				}
			}

			if (unsatisfied < least_unsatisfied) {
				least_unsatisfied = unsatisfied;
				selected_cpl = cpl;
			}
		}
	}

	if (!selected_cpl) {
		throw DCPError ("No CPLs found in DCP");
	}

	if (content->kdm()) {
		selected_cpl->add(decrypt_kdm_with_helpful_error(content->kdm().get()));
	}

	_cpl = selected_cpl->id();
	_name = selected_cpl->content_title_text();
	_content_kind = selected_cpl->content_kind();

	LOG_GENERAL ("Selected CPL %1", _cpl);

	auto try_to_parse_language = [](optional<string> lang) -> boost::optional<dcp::LanguageTag> {
		try {
			if (lang) {
				return dcp::LanguageTag (*lang);
			}
		} catch (...) {}
		return boost::none;
	};

	LOG_GENERAL("Looking at %1 reels", selected_cpl->reels().size());

	int reel_index = 0;
	for (auto reel: selected_cpl->reels()) {
		LOG_GENERAL("Reel %1", reel->id());

		if (reel->main_picture()) {
			/* This will mean a VF can be displayed in the timeline even if its picture asset
			 * is yet be resolved.
			 */
			_has_video = true;
			_video_length += reel->main_picture()->actual_duration();

			if (!reel->main_picture()->asset_ref().resolved()) {
				LOG_GENERAL("Main picture %1 of reel %2 is missing", reel->main_picture()->id(), reel->id());
				_needs_assets = true;
			} else {
				LOG_GENERAL("Main picture %1 of reel %2 found", reel->main_picture()->id(), reel->id());

				auto const frac = reel->main_picture()->edit_rate();
				float const fr = float(frac.numerator) / frac.denominator;
				if (!_video_frame_rate) {
					_video_frame_rate = fr;
				} else if (_video_frame_rate.get() != fr) {
					throw DCPError (_("Mismatched frame rates in DCP"));
				}

				auto asset = reel->main_picture()->asset();
				if (!_video_size) {
					_video_size = asset->size ();
				} else if (_video_size.get() != asset->size ()) {
					throw DCPError (_("Mismatched video sizes in DCP"));
				}

				if (dynamic_pointer_cast<dcp::MPEG2PictureAsset>(asset)) {
					_video_range = VideoRange::VIDEO;
				}
			}
		}

		if (reel->main_sound()) {
			_has_audio = true;
			auto const edit_rate = reel->main_sound()->edit_rate();

			if (!reel->main_sound()->asset_ref().resolved()) {
				LOG_GENERAL("Main sound %1 of reel %2 is missing", reel->main_sound()->id(), reel->id());
				_needs_assets = true;
			} else {
				LOG_GENERAL("Main sound %1 of reel %2 found", reel->main_sound()->id(), reel->id());

				auto asset = reel->main_sound()->asset();

				if (!_audio_channels) {
					_audio_channels = asset->channels();
				} else if (_audio_channels.get() != asset->channels()) {
					throw DCPError (_("Mismatched audio channel counts in DCP"));
				}

				_active_audio_channels = std::max(_active_audio_channels.get_value_or(0), asset->active_channels());

				if (!_audio_frame_rate) {
					_audio_frame_rate = asset->sampling_rate ();
				} else if (_audio_frame_rate.get() != asset->sampling_rate ()) {
					throw DCPError (_("Mismatched audio sample rates in DCP"));
				}

				_audio_language = try_to_parse_language (asset->language());
				_audio_length += reel->main_sound()->actual_duration() * (asset->sampling_rate() * edit_rate.denominator / edit_rate.numerator);
			}
		}

		auto read_main_text = [this, reel, reel_index, try_to_parse_language](
			shared_ptr<dcp::ReelTextAsset> reel_asset, TextType type, string name, boost::optional<dcp::LanguageTag>& language
			) {

			if (reel_asset) {
				if (reel_asset->entry_point().get_value_or(0) != 0) {
					_has_non_zero_entry_point[type] = true;
				}
				if (!reel_asset->asset_ref().resolved()) {
					LOG_GENERAL("Main %1 %2 of reel %3 is missing", name, reel_asset->id(), reel->id());
					_needs_assets = true;
				} else {
					LOG_GENERAL("Main %1 %2 of reel %3 found", name, reel_asset->id(), reel->id());

					_text_count[type] = 1;
					language = try_to_parse_language(reel_asset->language());

					auto asset = reel_asset->asset();
					for (auto const& font: asset->font_data()) {
						_fonts.push_back({reel_index, asset->id(), make_shared<dcpomatic::Font>(font.first, font.second)});
					}
				}
			}

		};

		read_main_text(reel->main_subtitle(), TextType::OPEN_SUBTITLE, "subtitle", _open_subtitle_language);
		read_main_text(reel->main_caption(), TextType::OPEN_CAPTION, "caption", _open_caption_language);

		auto read_closed_text = [this, reel, reel_index, try_to_parse_language](
			vector<shared_ptr<dcp::ReelTextAsset>> reel_assets, TextType type, string name, vector<DCPTextTrack>& tracks
			) {

			_text_count[type] = std::max(_text_count[type], static_cast<int>(reel_assets.size()));

			if (tracks.size() < reel_assets.size()) {
				/* We only want to add 1 DCPTextTrack to tracks per closed subtitle/caption.  I guess it's possible that different
				 * reels have different numbers of tracks (though I don't think they should) so make sure that tracks ends
				 * up with the maximum.
				 */
				tracks.clear();
				for (auto subtitle: reel_assets) {
					tracks.push_back(DCPTextTrack(subtitle->annotation_text().get_value_or(""), try_to_parse_language(subtitle->language())));
				}
			}

			for (auto text: reel_assets) {
				if (text->entry_point().get_value_or(0) != 0) {
					_has_non_zero_entry_point[type] = true;
				}
				if (!text->asset_ref().resolved()) {
					LOG_GENERAL("Closed %1 %2 of reel %3 is missing", name, text->id(), reel->id());
					_needs_assets = true;
				} else {
					LOG_GENERAL("Closed %1 %2 of reel %3 found", name, text->id(), reel->id());

					auto asset = text->asset();
					for (auto const& font: asset->font_data()) {
						_fonts.push_back({reel_index, asset->id(), make_shared<dcpomatic::Font>(font.first, font.second)});
					}
				}
			}
		};

		read_closed_text(reel->closed_subtitles(), TextType::CLOSED_SUBTITLE, "subtitle", _dcp_subtitle_tracks);
		read_closed_text(reel->closed_captions(), TextType::CLOSED_CAPTION, "caption", _dcp_caption_tracks);

		if (reel->main_markers ()) {
			auto rm = reel->main_markers()->get();
			_markers.insert (rm.begin(), rm.end());
		}

		if (reel->atmos()) {
			_has_atmos = true;
			_atmos_length += reel->atmos()->actual_duration();
			if (_atmos_edit_rate != dcp::Fraction()) {
				DCPOMATIC_ASSERT(reel->atmos()->edit_rate() == _atmos_edit_rate);
			}
			_atmos_edit_rate = reel->atmos()->edit_rate();
		}

		if (reel->main_picture()) {
			_reel_lengths.push_back(reel->main_picture()->actual_duration());
		} else if (reel->main_sound()) {
			_reel_lengths.push_back(reel->main_sound()->actual_duration());
		} else if (reel->main_subtitle()) {
			_reel_lengths.push_back(reel->main_subtitle()->actual_duration());
		} else if (reel->main_caption()) {
			_reel_lengths.push_back(reel->main_caption()->actual_duration());
		} else if (!reel->closed_subtitles().empty()) {
			_reel_lengths.push_back(reel->closed_subtitles().front()->actual_duration());
		} else if (!reel->closed_captions().empty()) {
			_reel_lengths.push_back(reel->closed_captions().front()->actual_duration());
		} else if (!reel->atmos()) {
			_reel_lengths.push_back(reel->atmos()->actual_duration());
		}

		++reel_index;
	}

	_encrypted = selected_cpl->any_encrypted();
	_kdm_valid = true;

	LOG_GENERAL_NC ("Check that everything encrypted has a key");

	/* Check first that anything encrypted has a key.  We must do this, as if we try to
	 * read encrypted data with asdcplib without even offering a key it will just return
	 * the encrypted data.  Secondly, check that we can read the first thing from each
	 * asset in each reel.  This checks that when we do have a key it's the right one.
	 */
	try {
		for (auto i: selected_cpl->reels()) {
			LOG_GENERAL ("Reel %1", i->id());
			if (i->main_picture() && i->main_picture()->asset_ref().resolved()) {
				auto pic = i->main_picture()->asset();
				if (pic->encrypted() && !pic->key()) {
					_kdm_valid = false;
					LOG_GENERAL_NC ("Picture has no key");
					break;
				}
				auto j2k_mono = dynamic_pointer_cast<dcp::MonoJ2KPictureAsset>(pic);
				auto j2k_stereo = dynamic_pointer_cast<dcp::StereoJ2KPictureAsset>(pic);
				auto mpeg2_mono = dynamic_pointer_cast<dcp::MonoMPEG2PictureAsset>(pic);

				if (j2k_mono) {
					auto reader = j2k_mono->start_read();
					reader->set_check_hmac (false);
					reader->get_frame(0)->xyz_image();
					_video_encoding = VideoEncoding::JPEG2000;
				} else if (j2k_stereo) {
					auto reader = j2k_stereo->start_read();
					reader->set_check_hmac (false);
					reader->get_frame(0)->xyz_image(dcp::Eye::LEFT);
					_video_encoding = VideoEncoding::JPEG2000;
				} else if (mpeg2_mono) {
					auto reader = mpeg2_mono->start_read();
					reader->set_check_hmac(false);
					dcp::MPEG2Decompressor decompressor;
					decompressor.decompress_frame(reader->get_frame(0));
					_video_encoding = VideoEncoding::MPEG2;
				}
			}

			if (i->main_sound() && i->main_sound()->asset_ref().resolved()) {
				auto sound = i->main_sound()->asset();
				if (sound->encrypted() && !sound->key()) {
					_kdm_valid = false;
					LOG_GENERAL_NC ("Sound has no key");
					break;
				}
				auto reader = sound->start_read();
				reader->set_check_hmac (false);
				reader->get_frame(0);
			}

			if (i->main_subtitle() && i->main_subtitle()->asset_ref().resolved()) {
				auto sub = i->main_subtitle()->asset();
				auto mxf_sub = dynamic_pointer_cast<dcp::MXF>(sub);
				if (mxf_sub && mxf_sub->encrypted() && !mxf_sub->key()) {
					_kdm_valid = false;
					LOG_GENERAL_NC ("Subtitle has no key");
					break;
				}
				sub->texts();
			}

			if (i->main_caption() && i->main_caption()->asset_ref().resolved()) {
				auto cap = i->main_caption()->asset();
				auto mxf_cap = dynamic_pointer_cast<dcp::MXF>(cap);
				if (mxf_cap && mxf_cap->encrypted() && !mxf_cap->key()) {
					_kdm_valid = false;
					LOG_GENERAL_NC("Caption has no key");
					break;
				}
				cap->texts();
			}

			if (i->atmos() && i->atmos()->asset_ref().resolved()) {
				if (auto atmos = i->atmos()->asset()) {
					if (atmos->encrypted() && !atmos->key()) {
						_kdm_valid = false;
						LOG_GENERAL_NC ("ATMOS sound has no key");
						break;
					}
					auto reader = atmos->start_read();
					reader->set_check_hmac (false);
					reader->get_frame(0);
				}
			}
		}
	} catch (dcp::ReadError& e) {
		_kdm_valid = false;
		LOG_GENERAL ("KDM is invalid: %1", e.what());
	} catch (dcp::MiscError& e) {
		_kdm_valid = false;
		LOG_GENERAL ("KDM is invalid: %1", e.what());
	}

	_standard = selected_cpl->standard();
	if (!selected_cpl->reels().empty()) {
		auto first_reel = selected_cpl->reels()[0];
		_three_d = first_reel->main_picture() && first_reel->main_picture()->asset_ref().resolved() && dynamic_pointer_cast<dcp::StereoJ2KPictureAsset>(first_reel->main_picture()->asset());
	}
	_ratings = selected_cpl->ratings();
	for (auto version: selected_cpl->content_versions()) {
		_content_versions.push_back(version.label_text);
	}

	_cpl = selected_cpl->id();
}


void
DCPExaminer::add_fonts(shared_ptr<TextContent> content)
{
	FontIDAllocator font_id_allocator;

	for (auto const& font: _fonts) {
		font_id_allocator.add_font(font.reel_index, font.asset_id, font.font->id());
	}

	font_id_allocator.allocate();

	for (auto const& font: _fonts) {
		auto font_copy = make_shared<dcpomatic::Font>(*font.font);
		font_copy->set_id(font_id_allocator.font_id(font.reel_index, font.asset_id, font.font->id()));
		content->add_font(font_copy);
	}

	if (!font_id_allocator.has_default_font()) {
		content->add_font(make_shared<dcpomatic::Font>(font_id_allocator.default_font_id(), default_font_file()));
	}
}

