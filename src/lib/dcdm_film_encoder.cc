/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


#include "audio_buffers.h"
#include "cross.h"
#include "dcdm_film_encoder.h"
#include "dcdm_j2k_writer.h"
#include "dcdm_quality.h"
#include "dcp_video.h"
#include "dpx_writer.h"
#include "exceptions.h"
#include "film.h"
#include "font.h"
#include "font_id_map.h"
#include "image.h"
#include "image_png.h"
#include "job.h"
#include "player.h"
#include "player_text.h"
#include "player_video.h"
#include "render_text.h"
#include "string_text.h"
#include "dcp_text_track.h"
#include "text_content.h"
#include "text_type.h"
#include "util.h"
#include "version.h"
#include "wav_writer.h"
#include <dcp/atmos_asset.h>
#include <dcp/atmos_asset_writer.h>
#include <dcp/filesystem.h>
#include <dcp/interop_text_asset.h>
#include <dcp/smpte_text_asset.h>
#include <dcp/text_image.h>
#include <dcp/text_string.h>
#include <dcp/openjpeg_image.h>
#include <fmt/format.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <deque>
#include <fstream>
#include <future>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>

#include <boost/thread.hpp>

#include "i18n.h"


using std::make_shared;
using std::pair;
using std::set;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using std::weak_ptr;
using namespace dcpomatic;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


static
shared_ptr<const Film>
check_film(shared_ptr<const Film> film)
{
	auto const quality = dcdm_source_quality(film);
	if (!quality.ok) {
		throw std::runtime_error(quality.reason);
	}
	return film;
}


/** Write a text file through dcp::File (Unicode-safe paths, checked writes) */
static
void
write_file(boost::filesystem::path const& path, string const& text)
{
	dcp::File file(path, "wb");
	if (!file) {
		throw FileError("Could not open file for writing", path);
	}
	file.checked_write(text.data(), text.size());
	file.close();
}


static
boost::filesystem::path
check_output(boost::filesystem::path output)
{
	if (dcp::filesystem::exists(output / "DCDM_INCOMPLETE.txt")) {
		throw std::runtime_error(
			fmt::format(_("The DCDM output folder {} holds an incomplete DCDM export (see DCDM_INCOMPLETE.txt).  Empty it before exporting again."), output.string())
			);
	}
	if (dcp::filesystem::exists(output) && !dcp::filesystem::is_empty(output)) {
		throw std::runtime_error(fmt::format(_("The DCDM output folder {} is not empty."), output.string()));
	}
	return output;
}


static
set<dcp::Channel>
used_channels(shared_ptr<const Film> film)
{
	set<dcp::Channel> used;
	for (auto channel: film->mapped_audio_channels()) {
		used.insert(static_cast<dcp::Channel>(channel));
	}

	return used;
}


/** @return the channels to write and their labels: the 5.1 or 7.1 layout, then any
 *  HI, VI and sign language channels that the film uses.
 */
static
vector<pair<dcp::Channel, string>>
channels_of(shared_ptr<const Film> film)
{
	auto const used = used_channels(film);
	auto const layout = dcdm_layout(used);
	auto const layout_channels = dcdm_channels(layout);
	auto const layout_labels = dcdm_channel_labels(layout);

	vector<pair<dcp::Channel, string>> out;
	for (size_t i = 0; i < layout_channels.size(); ++i) {
		out.push_back({ layout_channels[i], layout_labels[i] });
	}
	for (auto const& extra: dcdm_extra_channels(used)) {
		out.push_back(extra);
	}
	return out;
}


static
AudioMapping
identity_map(shared_ptr<const Film> film)
{
	AudioMapping map(film->audio_channels(), film->audio_channels());
	for (int i = 0; i < film->audio_channels(); ++i) {
		map.set(i, i, 1);
	}
	return map;
}


/** @return an XYZ12LE image holding the 12-bit values of an OpenJPEG XYZ image */
static
shared_ptr<Image>
to_xyz12(shared_ptr<const dcp::OpenJPEGImage> xyz)
{
	auto const size = xyz->size();
	auto image = make_shared<Image>(AV_PIX_FMT_XYZ12LE, size, Image::Alignment::COMPACT);
	for (int y = 0; y < size.height; ++y) {
		auto row = reinterpret_cast<uint16_t*>(image->data()[0] + y * image->stride()[0]);
		for (int x = 0; x < size.width; ++x) {
			for (int c = 0; c < 3; ++c) {
				row[x * 3 + c] = static_cast<uint16_t>(xyz->data(c)[y * size.width + x] << 4);
			}
		}
	}
	return image;
}


DCDMFilmEncoder::DCDMFilmEncoder(shared_ptr<const Film> film, weak_ptr<Job> job, boost::filesystem::path output, DCDMPictureFormat picture_format)
	: FilmEncoder(check_film(film), job)
	, _output(check_output(output))
	, _picture_format(picture_format)
	, _channels(channels_of(film))
	, _reels(static_cast<int>(film->reels().size()))
	, _history(200)
	, _reel_periods(film->reels())
	/* Connect before the butler starts the player, so that no Atmos is missed */
	, _atmos_connection(_player.Atmos.connect(boost::bind(&DCDMFilmEncoder::atmos, this, _1, _2, _3)))
	, _butler(
		_film,
		_player,
		identity_map(film),
		film->audio_channels(),
		AV_PIX_FMT_RGB48LE,
		VideoRange::FULL,
		Image::Alignment::PADDED,
		false,
		false,
		Butler::Video::ENABLED,
		Butler::Audio::ENABLED
		)
{

}


void
DCDMFilmEncoder::atmos(shared_ptr<const dcp::AtmosFrame> frame, DCPTime time, AtmosMetadata metadata)
{
	boost::mutex::scoped_lock lm(_atmos_mutex);

	/* Once finished, a late frame (signals2 does not wait for a running slot) is dropped */
	if (_atmos_finished) {
		return;
	}

	int reel = 0;
	while (reel + 1 < static_cast<int>(_reel_periods.size()) && _reel_periods[reel].to <= time) {
		++reel;
	}

	if (reel != _atmos_reel) {
		if (_atmos_writer) {
			_atmos_writer->finalize();
			_atmos_writer.reset();
		}
		auto const dir = reel_directory(reel);
		dcp::filesystem::create_directories(dir);
		_atmos_asset = metadata.create(dcp::Fraction(_film->video_frame_rate(), 1));
		_atmos_writer = _atmos_asset->start_write(dir / "atmos.mxf");
		_atmos_reel = reel;
	}

	/* We get one video frame's worth of Atmos at a time, as the DCP writer does */
	_atmos_writer->write(frame);
}


void
DCDMFilmEncoder::finish_atmos()
{
	boost::mutex::scoped_lock lm(_atmos_mutex);
	_atmos_finished = true;
	if (_atmos_writer) {
		_atmos_writer->finalize();
		_atmos_writer.reset();
	}
}


boost::filesystem::path
DCDMFilmEncoder::picture_directory(boost::filesystem::path reel_directory) const
{
	/* ST 428-24 wants each packed image sequence in a directory of its own */
	return _picture_format == DCDMPictureFormat::J2K_LOSSLESS ? reel_directory / "picture" : reel_directory;
}


boost::filesystem::path
DCDMFilmEncoder::reel_directory(int reel) const
{
	if (_reels == 1) {
		return _output;
	}
	return _output / fmt::format("reel_{:02d}", reel + 1);
}


void
DCDMFilmEncoder::go()
{
	try {
		do_go();
	} catch (std::exception& e) {
		mark_incomplete(e.what());
		throw;
	} catch (...) {
		/* e.g. boost::thread_interrupted when the job is cancelled */
		mark_incomplete(_("the export was stopped"));
		throw;
	}
}


/** Leave the output folder clearly marked as incomplete, with what could be closed properly closed */
void
DCDMFilmEncoder::mark_incomplete(string why)
{
	_atmos_connection.disconnect();
	try {
		finish_atmos();
	} catch (...) {}

	try {
		write_file(
			_output / "DCDM_INCOMPLETE.txt",
			fmt::format(
				"This DCDM export did not finish: {}\n"
				"The files in this folder are incomplete and must not be used.\n",
				why
				)
			);
	} catch (...) {}
}


void
DCDMFilmEncoder::do_go()
{
	if (auto job = _job.lock()) {
		job->sub(_("Encoding"));
	}

	Waker waker(Waker::Reason::ENCODING);

	vector<string> labels;
	vector<dcp::Channel> channels_to_write;
	for (auto const& channel: _channels) {
		channels_to_write.push_back(channel.first);
		labels.push_back(channel.second);
	}
	auto const three_d = _film->three_d();
	auto const video_frame_rate = _film->video_frame_rate();
	auto const video_frame = DCPTime::from_frames(1, video_frame_rate);
	int const audio_frames = video_frame.frames_round(_film->audio_frame_rate());
	int const channels = _film->audio_channels();

	vector<float> interleaved(channels * audio_frames);
	auto deinterleaved = make_shared<AudioBuffers>(channels, audio_frames);

	auto const reels = _film->reels();
	int reel_index = 0;
	vector<unique_ptr<WAVWriter>> wavs;

	auto open_reel = [&](int index) {
		auto const dir = reel_directory(index);
		dcp::filesystem::create_directories(dir / "audio");
		auto const picture = picture_directory(dir);
		if (three_d) {
			dcp::filesystem::create_directories(picture / "left");
			dcp::filesystem::create_directories(picture / "right");
		} else {
			dcp::filesystem::create_directories(picture);
		}
		wavs.clear();
		for (auto const& label: labels) {
			wavs.push_back(unique_ptr<WAVWriter>(new WAVWriter(dir / "audio" / (label + ".wav"), _film->audio_frame_rate())));
		}
	};

	open_reel(0);

	/* Pictures are converted and encoded on several threads, as for a DCP; each frame's
	 * file name is fixed, so the order they finish in does not matter.  At most one
	 * frame per core is in flight; a failure comes back through get().
	 */
	std::deque<std::future<void>> pending;
	auto const max_pending = std::max(1U, boost::thread::hardware_concurrency());
	auto wait_for_one = [&pending]() {
		auto next = std::move(pending.front());
		pending.pop_front();
		next.get();
	};

	for (DCPTime time; time < _film->length(); time += video_frame) {

		while (reel_index + 1 < static_cast<int>(reels.size()) && !reels[reel_index].contains(time)) {
			for (auto& wav: wavs) {
				wav->close();
			}
			++reel_index;
			open_reel(reel_index);
		}

		auto const reel = reels[reel_index];
		auto const dir = reel_directory(reel_index);

		for (int i = 0; i < (three_d ? 2 : 1); ++i) {
			Butler::Error error;
			auto video = _butler.get_video(Butler::Behaviour::BLOCKING, &error);
			_butler.rethrow();
			if (!video.first) {
				if (error.code != Butler::Error::Code::FINISHED) {
					throw DecodeError(fmt::format("Error during decoding: {}", error.summary()));
				}
				continue;
			}

			/* The butler's frame must be the one for this loop time, or frames would be renamed over each other */
			DCPOMATIC_ASSERT(video.second.frames_round(video_frame_rate) == time.frames_round(video_frame_rate));
			auto const index = (video.second - reel.from).frames_round(video_frame_rate);
			auto path = picture_directory(dir);
			if (three_d) {
				path /= video.first->eyes() == Eyes::RIGHT ? "right" : "left";
			}

			/* SMPTE ST 428-24 file names: fixed-width, contiguous frame numbers and .pdc.j2c */
			path /= _picture_format == DCDMPictureFormat::J2K_LOSSLESS ? fmt::format("{:08d}.pdc.j2c", index) : fmt::format("frame_{:08d}.dpx", index);

			if (pending.size() >= max_pending) {
				wait_for_one();
			}
			pending.push_back(
				std::async(std::launch::async, [frame = video.first, path, format = _picture_format]() {
					auto const xyz = DCPVideo::convert_to_xyz(frame);
					switch (format) {
					case DCDMPictureFormat::J2K_LOSSLESS:
						write_lossless_j2k(path, xyz);
						break;
					case DCDMPictureFormat::DPX:
						write_dpx(path, to_xyz12(xyz));
						break;
					}
				})
			);
		}

		_butler.get_audio(Butler::Behaviour::BLOCKING, interleaved.data(), audio_frames);
		float const* p = interleaved.data();
		for (int j = 0; j < audio_frames; ++j) {
			for (int k = 0; k < channels; ++k) {
				deinterleaved->data(k)[j] = *p++;
			}
		}
		auto const layout = dcdm_pick_channels(deinterleaved, channels_to_write);
		for (size_t c = 0; c < wavs.size(); ++c) {
			wavs[c]->write(layout->data(static_cast<int>(c)), audio_frames);
		}

		_history.event();
		{
			boost::mutex::scoped_lock lm(_mutex);
			_last_time = time;
		}

		if (auto job = _job.lock()) {
			job->set_progress(float(time.get()) / _film->length().get());
		}

		waker.nudge();
	}

	while (!pending.empty()) {
		wait_for_one();
	}

	for (auto& wav: wavs) {
		wav->close();
	}

	/* Stop the player sending more Atmos and close the last asset */
	_atmos_connection.disconnect();
	finish_atmos();

	write_text();
	write_info();
}


/** Correct a subtitle's vertical position for the standard we are writing, as ReelWriter does */
static
float
vertical_position(StringText const& subtitle, dcp::SubtitleStandard to, FontMetrics& metrics)
{
	if (dcp::uses_baseline(subtitle.valign_standard) == dcp::uses_baseline(to)) {
		return subtitle.v_position();
	}

	auto const baseline_to_bottom = metrics.baseline_to_bottom(subtitle);
	auto const height = metrics.height(subtitle);

	float correction = 0;
	switch (subtitle.v_align()) {
	case dcp::VAlign::TOP:
		correction = height - baseline_to_bottom;
		break;
	case dcp::VAlign::CENTER:
		correction = (height / 2) - baseline_to_bottom;
		break;
	case dcp::VAlign::BOTTOM:
		correction = baseline_to_bottom;
		break;
	}

	return subtitle.v_position() + (dcp::uses_bounding_box(subtitle.valign_standard) ? correction : -correction);
}


static
string
safe_name(string s)
{
	for (auto& c: s) {
		if (!isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') {
			c = '_';
		}
	}
	return s;
}


void
DCDMFilmEncoder::write_text()
{
	/* Key is (reel, track); no track means open subtitles */
	std::map<pair<int, boost::optional<DCPTextTrack>>, shared_ptr<dcp::TextAsset>> assets;
	vector<DCPTextTrack> tracks;

	auto const interop = _film->interop();
	auto const standard = interop ? dcp::SubtitleStandard::INTEROP : dcp::SubtitleStandard::SMPTE_2014;
	FontMetrics metrics(_film->frame_size().height);
	/* Only needed when a subtitle has no font of its own; a film with no text must not need it */
	boost::optional<dcp::ArrayData> default_font_data;
	auto default_font = [&default_font_data]() {
		if (!default_font_data) {
			default_font_data = dcp::ArrayData(default_font_file());
		}
		return *default_font_data;
	};
	auto const languages = _film->open_text_languages();

	auto make_asset = [&](int reel, boost::optional<DCPTextTrack> track) -> shared_ptr<dcp::TextAsset> {
		if (interop) {
			auto asset = make_shared<dcp::InteropTextAsset>();
			asset->set_movie_title(_film->name());
			if (!track) {
				asset->set_language(languages.first ? languages.first->as_string() : "Unknown");
			} else if (track->language) {
				asset->set_language(track->language->as_string());
			}
			asset->set_reel_number(fmt::to_string(reel + 1));
			return asset;
		}

		auto asset = make_shared<dcp::SMPTETextAsset>();
		asset->set_content_title_text(_film->name());
		if (!track && languages.first) {
			asset->set_language(*languages.first);
		} else if (track && track->language) {
			asset->set_language(dcp::LanguageTag(track->language->as_string()));
		}
		asset->set_edit_rate(dcp::Fraction(_film->video_frame_rate(), 1));
		asset->set_reel_number(reel + 1);
		asset->set_time_code_rate(_film->video_frame_rate());
		asset->set_start_time(dcp::Time());
		return asset;
	};

	Player player(_film, Image::Alignment::PADDED, false);
	player.set_ignore_video();
	player.set_ignore_audio();

	/* Collect the texts first: the font IDs come from every font in the film, as Writer does */
	struct PendingText {
		PlayerText subs;
		TextType type;
		boost::optional<DCPTextTrack> track;
		DCPTimePeriod period;
	};
	vector<PendingText> pending_texts;

	boost::signals2::scoped_connection connection = player.Text.connect(
		[&](PlayerText subs, TextType type, boost::optional<DCPTextTrack> track, DCPTimePeriod period) {
			if (subs.string.empty() && subs.bitmap.empty()) {
				return;
			}
			pending_texts.push_back({ subs, type, track, period });
		});

	if (auto job = _job.lock()) {
		job->sub(_("Subtitles"));
		job->set_progress_unknown();
	}
	/* set_progress() is an interruption point, so a cancelled job stops here */
	while (!player.pass()) {
		if (auto job = _job.lock()) {
			job->set_progress_unknown();
		}
	}

	/* Font IDs as Writer makes them: Interop has one font for all subtitles, SMPTE one ID per font */
	FontIdMap fonts;
	auto fix_id = [](string id) {
		return id.empty() ? string("font") : id;
	};
	auto const all_fonts = player.get_subtitle_fonts();
	shared_ptr<dcpomatic::Font> chosen_interop_font;
	if (!all_fonts.empty()) {
		if (interop) {
			for (auto font: all_fonts) {
				fonts.put(font, fix_id(all_fonts[0]->id()));
			}
			chosen_interop_font = all_fonts[0];
		} else {
			for (auto font: all_fonts) {
				fonts.put(font, fix_id(font->id()));
			}
		}
	}

	for (auto const& pending: pending_texts) {
		auto const& subs = pending.subs;
		auto const& type = pending.type;
		auto const& track = pending.track;
		auto const& period = pending.period;

		boost::optional<DCPTextTrack> key_track;
		if (type == TextType::CLOSED_CAPTION || type == TextType::CLOSED_SUBTITLE) {
			DCPOMATIC_ASSERT(track);
			key_track = track;
			if (std::find(tracks.begin(), tracks.end(), *track) == tracks.end()) {
				tracks.push_back(*track);
			}
		} else if (type != TextType::OPEN_SUBTITLE && type != TextType::OPEN_CAPTION) {
			continue;
		}

		/* As ReelWriter: times relative to the reel, to the millisecond */
		int const tcr = 1000;

		auto add_to_reel = [&](int reel, DCPTimePeriod part) {
			auto const reel_period = _reel_periods[reel];
			auto& asset = assets[std::make_pair(reel, key_track)];
			if (!asset) {
				asset = make_asset(reel, key_track);
			}

			for (auto i: subs.string) {
				i.set_in(dcp::Time(part.from.seconds() - reel_period.from.seconds(), tcr));
				i.set_out(dcp::Time(part.to.seconds() - reel_period.from.seconds(), tcr));
				i.set_v_position(vertical_position(i, standard, metrics));
				auto text = make_shared<dcp::TextString>(i);
				/* The font the DCP would use: Interop always uses the chosen one */
				auto const font = interop ? chosen_interop_font : i.font;
				auto const id = (font && fonts.map().count(font)) ? fonts.get(font) : string("default");
				text->set_font(id);
				asset->add(text);
				asset->ensure_font(id, font && font->data() ? *font->data() : default_font());
			}

			for (auto const& i: subs.bitmap) {
				asset->add(
					make_shared<dcp::TextImage>(
						image_as_png(i.image),
						dcp::Time(part.from.seconds() - reel_period.from.seconds(), tcr),
						dcp::Time(part.to.seconds() - reel_period.from.seconds(), tcr),
						i.rectangle.x, dcp::HAlign::LEFT, i.rectangle.y, dcp::VAlign::TOP, 0, vector<dcp::Text::VariableZPosition>(),
						dcp::Time(), dcp::Time()
						)
					);
			}
		};

		/* As Writer::write(PlayerText...): back off a couple of frames from reel boundaries */
		auto const vfr = _film->video_frame_rate();
		auto back_off = [vfr](DCPTimePeriod p) {
			p.to -= DCPTime::from_frames(2, vfr);
			if (p.duration().frames_floor(vfr) <= 0) {
				p.to = p.from + DCPTime::from_frames(1, vfr);
			}
			return p;
		};

		int reel = 0;
		while (reel + 1 < static_cast<int>(_reel_periods.size()) && _reel_periods[reel].to <= period.from) {
			++reel;
		}

		auto part = period;
		if (period.to > _reel_periods[reel].to) {
			/* This text goes off the end of the reel: the parts in later reels go there */
			for (int later = reel + 1; later < static_cast<int>(_reel_periods.size()); ++later) {
				if (auto overlap = _reel_periods[later].overlap(period)) {
					add_to_reel(later, back_off(*overlap));
				}
			}
			part.to = _reel_periods[reel].to;
			part = back_off(part);
		}

		add_to_reel(reel, part);
	}


	for (auto const& i: assets) {
		auto const reel = i.first.first;
		auto const& track = i.first.second;
		boost::filesystem::path dir = reel_directory(reel);
		boost::filesystem::path name;
		if (track) {
			auto const index = std::find(tracks.begin(), tracks.end(), *track) - tracks.begin();
			dir /= "captions";
			name = fmt::format("{:02d}_{}.xml", index + 1, safe_name(track->name));
		} else {
			dir /= "subtitles";
			name = "subtitles.xml";
		}

		dcp::filesystem::create_directories(dir);
		if (interop) {
			/* This writes the fonts and subtitle images alongside the XML */
			i.second->write(dir / name);
		} else {
			write_file(dir / name, i.second->xml_as_string());
			for (auto const& font: i.second->font_data()) {
				font.second.write(dir / (safe_name(font.first) + ".ttf"));
			}
			/* SMPTE XML refers to subtitle images by ID; put them beside it */
			for (auto text: i.second->texts()) {
				if (auto image = std::dynamic_pointer_cast<const dcp::TextImage>(text)) {
					image->png_image().write(dir / (image->id() + ".png"));
				}
			}
		}

		if (track) {
			pair<string, string> const written(name.string(), track->name);
			if (std::find(_caption_tracks_written.begin(), _caption_tracks_written.end(), written) == _caption_tracks_written.end()) {
				_caption_tracks_written.push_back(written);
			}
		} else {
			_open_subtitles_written = true;
		}
	}
}


boost::optional<float>
DCDMFilmEncoder::current_rate() const
{
	return _history.rate();
}


Frame
DCDMFilmEncoder::frames_done() const
{
	boost::mutex::scoped_lock lm(_mutex);
	return _last_time.frames_round(_film->video_frame_rate());
}


/** Write DCDM_INFO.txt in the output folder, describing what was written and the standards it follows */
void
DCDMFilmEncoder::write_info() const
{
	auto const reels = _film->reels();
	auto const rate = _film->video_frame_rate();
	auto const size = _film->frame_size();
	auto const j2k = _picture_format == DCDMPictureFormat::J2K_LOSSLESS;

	/* Report what was actually written */
	auto const open_subtitles = _open_subtitles_written;
	auto const& caption_tracks = _caption_tracks_written;

	string labels;
	bool seven_one = false;
	for (auto const& channel: _channels) {
		labels += (labels.empty() ? "" : ", ") + channel.second;
		seven_one = seven_one || channel.first == dcp::Channel::BSL || channel.first == dcp::Channel::BSR;
	}

	string f;
	auto line = [&f](string const& s) { f += s + "\n"; };

	line("D-CINEMA DISTRIBUTION MASTER (DCDM)");
	line("===================================");
	line("");
	line(fmt::format("Title: {}", _film->name()));
	line(fmt::format("Written by: DCP-o-matic {} ({})", dcpomatic_version, dcpomatic_git_commit));
	line(fmt::format("Written on: {}", boost::posix_time::to_simple_string(boost::posix_time::second_clock::local_time())));
	line("");
	line("PICTURE");
	if (j2k) {
		line("Format: Lossless JPEG 2000 codestreams (DCDM packed images), one file per frame: picture/NNNNNNNN.pdc.j2c");
		line("Coding: reversible 5/3 wavelet (mathematically lossless), 5 decomposition levels, RPCL progression,");
		line("        code-blocks 128x32, precincts 256x256 (128x128 at the lowest resolution), tile-parts by resolution, TLM");
		line("        COM marker: SMPTE DCDM X'Y'Z' image as defined in SMPTE ST 428-1");
		line("        The High-Throughput (HT) block coder recommended by ST 428-24 is not used (encoder limitation).");
		line("Import:  add this folder with \"Add folder...\" in DCP-o-matic to make a DCP from it; this turns on");
		line("         \"Re-encode JPEG2000 data\", without which the lossless codestreams would be copied into the DCP unchanged.");
	} else {
		line("Format: Uncompressed DPX, one file per frame: frame_NNNNNNNN.dpx");
		line("Coding: 3 components, 12 bits each, in big-endian 16-bit words, left-justified (packing method A).");
		line("        DPX has no X'Y'Z' colorimetric code: components are X', Y', Z' by convention (descriptor 50).");
		line("Import:  add this folder with \"Add folder...\" in DCP-o-matic to make a DCP from it; this sets the");
		line("         colour conversion of the pictures to None, as the default conversion would convert X'Y'Z' to XYZ");
		line("         a second time.");
	}
	line(fmt::format("Size: {}x{}", size.width, size.height));
	line("Colour: X'Y'Z', 12 bits per component, as SMPTE ST 428-1 (converted exactly as for the DCP)");
	line(fmt::format("Frame rate: {} fps", rate));
	line(fmt::format("Stereoscopy: {}", _film->three_d() ? "3D (left/ and right/ hold each eye)" : "2D"));
	line(fmt::format("Duration: {} frames", _film->length().frames_round(rate)));
	line("");
	line(fmt::format("Reels: {}", reels.size()));
	for (size_t i = 0; i < reels.size(); ++i) {
		line(fmt::format("  {}: {} frames, starts at frame {}{}", i + 1, reels[i].duration().frames_round(rate), reels[i].from.frames_round(rate), reels.size() > 1 ? fmt::format(" (folder reel_{:02d})", i + 1) : string()));
	}
	line("");
	line("SOUND");
	line(fmt::format("Layout: {}", seven_one ? "7.1" : "5.1"));
	line(fmt::format("Channels: {} (one mono file per channel in audio/)", labels));
	line(fmt::format("Format: WAV, linear PCM, 24-bit, {} Hz, the same samples as the DCP (gain, delay, trim and processing applied)", _film->audio_frame_rate()));
	line(fmt::format("Atmos: {}", _film->contains_atmos_content() ? "yes (atmos.mxf in each reel that has Atmos)" : "no"));
	line("");
	line("TEXT");
	line(fmt::format("Open subtitles: {}", open_subtitles ? "yes (subtitles/subtitles.xml)" : "no"));
	line(fmt::format("Closed captions: {}", caption_tracks.empty() ? string("no") : fmt::format("{} track(s) in captions/", caption_tracks.size())));
	/* The file names cannot hold every character of a track name, so give the names here */
	for (auto const& track: caption_tracks) {
		line(fmt::format("  Track: {} = {}", track.first, track.second));
	}
	line(fmt::format("Text format: {} XML, timed relative to each reel, fonts alongside", _film->interop() ? "Interop" : "SMPTE"));
	line("");
	line("STANDARDS");
	line("SMPTE ST 428-1   D-Cinema Distribution Master - Image Characteristics");
	if (j2k) {
		line("SMPTE ST 428-24  D-Cinema Distribution Master - Packed Image");
		line("SMPTE ST 2067-40 Interoperable Master Format - Application #4 Cinema Mezzanine (encoding constraints)");
		line("ISO/IEC 15444-1  JPEG 2000 image coding system - Core coding system");
	} else {
		line("SMPTE ST 268     File Format for Digital Moving-Picture Exchange (DPX)");
	}
	line("SMPTE ST 428-2   D-Cinema Distribution Master - Audio Characteristics");
	line("SMPTE ST 428-12  D-Cinema Distribution Master - Audio Channel Mapping and Channel Labeling");
	line("RP 428-4         Audio file format for a DCDM (BWF, ITU-R BR.1352): NOT followed; the WAV files are plain");
	line("                 RIFF PCM without the bext chunk");
	if (open_subtitles || !caption_tracks.empty()) {
		line("SMPTE ST 428-7   D-Cinema Distribution Master - Subtitle");
	}
	if (_film->contains_atmos_content()) {
		line("SMPTE ST 429-18  D-Cinema Packaging - Immersive Audio Track File");
	}

	write_file(_output / "DCDM_INFO.txt", f);
}
