/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file src/film_state.cc
 *  @brief The state of a Film.  This is separate from Film so that
 *  state can easily be copied and kept around for reference
 *  by long-running jobs.  This avoids the jobs getting confused
 *  by the user changing Film settings during their run.
 */

#include <fstream>
#include <string>
#include <iomanip>
#include <sstream>
#include <boost/filesystem.hpp>
#include <boost/date_time.hpp>
#include "film_state.h"
#include "scaler.h"
#include "filter.h"
#include "format.h"
#include "dcp_content_type.h"
#include "util.h"
#include "exceptions.h"

using namespace std;
using namespace boost;

/** Write state to a stream.
 *  @param f Stream to write to.
 */
void
FilmState::write_metadata (ofstream& f) const
{
	/* User stuff */
	f << "name " << name << "\n";
	f << "use_dci_name " << use_dci_name << "\n";
	f << "content " << content << "\n";
	if (dcp_content_type) {
		f << "dcp_content_type " << dcp_content_type->pretty_name () << "\n";
	}
	f << "frames_per_second " << frames_per_second << "\n";
	if (format) {
		f << "format " << format->as_metadata () << "\n";
	}
	f << "left_crop " << crop.left << "\n";
	f << "right_crop " << crop.right << "\n";
	f << "top_crop " << crop.top << "\n";
	f << "bottom_crop " << crop.bottom << "\n";
	for (vector<Filter const *>::const_iterator i = filters.begin(); i != filters.end(); ++i) {
		f << "filter " << (*i)->id () << "\n";
	}
	f << "scaler " << scaler->id () << "\n";
	f << "dcp_frames " << dcp_frames << "\n";

	f << "dcp_trim_action ";
	switch (dcp_trim_action) {
	case CUT:
		f << "cut\n";
		break;
	case BLACK_OUT:
		f << "black_out\n";
		break;
	}
	
	f << "dcp_ab " << (dcp_ab ? "1" : "0") << "\n";
	f << "audio_gain " << audio_gain << "\n";
	f << "audio_delay " << audio_delay << "\n";
	f << "still_duration " << still_duration << "\n";
	f << "with_subtitles " << with_subtitles << "\n";
	f << "subtitle_offset " << subtitle_offset << "\n";
	f << "subtitle_scale " << subtitle_scale << "\n";

	/* Cached stuff; this is information about our content; we could
	   look it up each time, but that's slow.
	*/
	for (vector<int>::const_iterator i = thumbs.begin(); i != thumbs.end(); ++i) {
		f << "thumb " << *i << "\n";
	}
	f << "width " << size.width << "\n";
	f << "height " << size.height << "\n";
	f << "length " << length << "\n";
	f << "audio_channels " << audio_channels << "\n";
	f << "audio_sample_rate " << audio_sample_rate << "\n";
	f << "audio_sample_format " << audio_sample_format_to_string (audio_sample_format) << "\n";
	f << "content_digest " << content_digest << "\n";
	f << "has_subtitles " << has_subtitles << "\n";
	f << "audio_language " << audio_language << "\n";
	f << "subtitle_language " << subtitle_language << "\n";
	f << "territory " << territory << "\n";
	f << "rating " << rating << "\n";
	f << "studio " << studio << "\n";
	f << "facility " << facility << "\n";
	f << "package_type " << package_type << "\n";
}

/** Read state from a key / value pair.
 *  @param k Key.
 *  @param v Value.
 */
void
FilmState::read_metadata (string k, string v)
{
	/* User-specified stuff */
	if (k == "name") {
		name = v;
	} else if (k == "use_dci_name") {
		use_dci_name = (v == "1");
	} else if (k == "content") {
		content = v;
	} else if (k == "dcp_content_type") {
		dcp_content_type = DCPContentType::from_pretty_name (v);
	} else if (k == "frames_per_second") {
		frames_per_second = atof (v.c_str ());
	} else if (k == "format") {
		format = Format::from_metadata (v);
	} else if (k == "left_crop") {
		crop.left = atoi (v.c_str ());
	} else if (k == "right_crop") {
		crop.right = atoi (v.c_str ());
	} else if (k == "top_crop") {
		crop.top = atoi (v.c_str ());
	} else if (k == "bottom_crop") {
		crop.bottom = atoi (v.c_str ());
	} else if (k == "filter") {
		filters.push_back (Filter::from_id (v));
	} else if (k == "scaler") {
		scaler = Scaler::from_id (v);
	} else if (k == "dcp_frames") {
		dcp_frames = atoi (v.c_str ());
	} else if (k == "dcp_trim_action") {
		if (v == "cut") {
			dcp_trim_action = CUT;
		} else if (v == "black_out") {
			dcp_trim_action = BLACK_OUT;
		}
	} else if (k == "dcp_ab") {
		dcp_ab = (v == "1");
	} else if (k == "audio_gain") {
		audio_gain = atof (v.c_str ());
	} else if (k == "audio_delay") {
		audio_delay = atoi (v.c_str ());
	} else if (k == "still_duration") {
		still_duration = atoi (v.c_str ());
	} else if (k == "with_subtitles") {
		with_subtitles = (v == "1");
	} else if (k == "subtitle_offset") {
		subtitle_offset = atoi (v.c_str ());
	} else if (k == "subtitle_scale") {
		subtitle_scale = atof (v.c_str ());
	}
	
	/* Cached stuff */
	if (k == "thumb") {
		int const n = atoi (v.c_str ());
		/* Only add it to the list if it still exists */
		if (filesystem::exists (thumb_file_for_frame (n))) {
			thumbs.push_back (n);
		}
	} else if (k == "width") {
		size.width = atoi (v.c_str ());
	} else if (k == "height") {
		size.height = atoi (v.c_str ());
	} else if (k == "length") {
		length = atof (v.c_str ());
	} else if (k == "audio_channels") {
		audio_channels = atoi (v.c_str ());
	} else if (k == "audio_sample_rate") {
		audio_sample_rate = atoi (v.c_str ());
	} else if (k == "audio_sample_format") {
		audio_sample_format = audio_sample_format_from_string (v);
	} else if (k == "content_digest") {
		content_digest = v;
	} else if (k == "has_subtitles") {
		has_subtitles = (v == "1");
	} else if (k == "audio_language") {
		audio_language = v;
	} else if (k == "subtitle_language") {
		subtitle_language = v;
	} else if (k == "territory") {
		territory = v;
	} else if (k == "rating") {
		rating = v;
	} else if (k == "studio") {
		studio = v;
	} else if (k == "facility") {
		facility = v;
	} else if (k == "package_type") {
		package_type = v;
	}
}


/** @param n A thumb index.
 *  @return The path to the thumb's image file.
 */
string
FilmState::thumb_file (int n) const
{
	return thumb_file_for_frame (thumb_frame (n));
}

/** @param n A frame index within the Film.
 *  @return The path to the thumb's image file for this frame;
 *  we assume that it exists.
 */
string
FilmState::thumb_file_for_frame (int n) const
{
	return thumb_base_for_frame(n) + ".png";
}

string
FilmState::thumb_base (int n) const
{
	return thumb_base_for_frame (thumb_frame (n));
}

string
FilmState::thumb_base_for_frame (int n) const
{
	stringstream s;
	s.width (8);
	s << setfill('0') << n;
	
	filesystem::path p;
	p /= dir ("thumbs");
	p /= s.str ();
		
	return p.string ();
}


/** @param n A thumb index.
 *  @return The frame within the Film that it is for.
 */
int
FilmState::thumb_frame (int n) const
{
	assert (n < int (thumbs.size ()));
	return thumbs[n];
}

Size
FilmState::cropped_size (Size s) const
{
	s.width -= crop.left + crop.right;
	s.height -= crop.top + crop.bottom;
	return s;
}

/** Given a directory name, return its full path within the Film's directory.
 *  The directory (and its parents) will be created if they do not exist.
 */
string
FilmState::dir (string d) const
{
	filesystem::path p;
	p /= directory;
	p /= d;
	filesystem::create_directories (p);
	return p.string ();
}

/** Given a file or directory name, return its full path within the Film's directory */
string
FilmState::file (string f) const
{
	filesystem::path p;
	p /= directory;
	p /= f;
	return p.string ();
}

string
FilmState::content_path () const
{
	if (filesystem::path(content).has_root_directory ()) {
		return content;
	}

	return file (content);
}

ContentType
FilmState::content_type () const
{
#if BOOST_FILESYSTEM_VERSION == 3
	string ext = filesystem::path(content).extension().string();
#else
	string ext = filesystem::path(content).extension();
#endif

	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	
	if (ext == ".tif" || ext == ".tiff" || ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
		return STILL;
	}

	return VIDEO;
}

/** @return Number of bytes per sample of a single channel */
int
FilmState::bytes_per_sample () const
{
	switch (audio_sample_format) {
	case AV_SAMPLE_FMT_S16:
		return 2;
	default:
		return 0;
	}

	return 0;
}

int
FilmState::target_sample_rate () const
{
	/* Resample to a DCI-approved sample rate */
	double t = dcp_audio_sample_rate (audio_sample_rate);

	/* Compensate for the fact that video will be rounded to the
	   nearest integer number of frames per second.
	*/
	if (rint (frames_per_second) != frames_per_second) {
		t *= frames_per_second / rint (frames_per_second);
	}

	return rint (t);
}

int
FilmState::dcp_length () const
{
	if (dcp_frames) {
		return dcp_frames;
	}

	return length;
}

string
FilmState::dci_name () const
{
	stringstream d;

	string fixed_name = to_upper_copy (name);
	for (size_t i = 0; i < fixed_name.length(); ++i) {
		if (fixed_name[i] == ' ') {
			fixed_name[i] = '-';
		}
	}

	d << fixed_name << "_";

	if (dcp_content_type) {
		d << dcp_content_type->dci_name() << "_";
	}

	if (format) {
		d << format->dci_name() << "_";
	}

	if (!audio_language.empty ()) {
		d << audio_language;
		if (with_subtitles) {
			if (!subtitle_language.empty ()) {
				d << "-" << subtitle_language;
			} else {
				d << "-XX";
			}
		}
			
		d << "_";
	}

	if (!territory.empty ()) {
		d << territory;
		if (!rating.empty ()) {
			d << "-" << rating;
		}
		d << "_";
	}

	switch (audio_channels) {
	case 1:
		d << "10_";
		break;
	case 2:
		d << "20_";
		break;
	case 6:
		d << "51_";
		break;
	}

	d << "2K_";

	if (!studio.empty ()) {
		d << studio << "_";
	}

	gregorian::date today = gregorian::day_clock::local_day ();
	d << gregorian::to_iso_extended_string (today) << "_";

	if (!facility.empty ()) {
		d << facility << "_";
	}

	if (!package_type.empty ()) {
		d << package_type;
	}

	return d.str ();
}

/** @return name to give the DCP */
string
FilmState::dcp_name () const
{
	if (use_dci_name) {
		return dci_name ();
	}

	return name;
}

	       
