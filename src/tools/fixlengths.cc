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

#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <getopt.h>
#include <sndfile.h>
#include <boost/filesystem.hpp>
#include "lib/film.h"

using namespace std;
using namespace boost;

void
help (string n)
{
	cerr << "Syntax: " << n << " [--help] [--chop-audio-start] [--chop-audio-end] --film <film>\n";
}

void
sox (vector<string> const & audio_files, string const & process)
{
	for (vector<string>::const_iterator i = audio_files.begin(); i != audio_files.end(); ++i) {
		stringstream cmd;
		cmd << "sox \"" << *i << "\" -t wav \"" << *i << ".tmp\" " << process;
		cout << "> " << cmd.str() << "\n";
		int r = ::system (cmd.str().c_str());
		if (r == -1 || WEXITSTATUS (r) != 0) {
			cerr << "fixlengths: call to sox failed.\n";
			exit (EXIT_FAILURE);
		}
		filesystem::rename (*i + ".tmp", *i);
	}
}

int main (int argc, char* argv[])
{
	string film_dir;
	bool chop_audio_start = false;
	bool chop_audio_end = false;
	bool pad_audio_end = false;
	
	while (1) {
		static struct option long_options[] = {
			{ "help", no_argument, 0, 'h' },
			{ "chop-audio-start", no_argument, 0, 'c' },
			{ "chop-audio-end", no_argument, 0, 'd' },
			{ "pad-audio-end", no_argument, 0, 'p' },
			{ "film", required_argument, 0, 'f' },
			{ 0, 0, 0, 0 }
		};

		int option_index = 0;
		int c = getopt_long (argc, argv, "hcf:", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch (c) {
		case 'h':
			help (argv[0]);
			exit (EXIT_SUCCESS);
		case 'c':
			chop_audio_start = true;
			break;
		case 'd':
			chop_audio_end = true;
			break;
		case 'p':
			pad_audio_end = true;
			break;
		case 'f':
			film_dir = optarg;
			break;
		}
	}

	if (film_dir.empty ()) {
		help (argv[0]);
		exit (EXIT_FAILURE);
	}

	dvdomatic_setup ();

	Film* film = 0;
	try {
		film = new Film (film_dir, true);
	} catch (std::exception& e) {
		cerr << argv[0] << ": error reading film `" << film_dir << "' (" << e.what() << ")\n";
		exit (EXIT_FAILURE);
	}

	/* XXX: hack */
	int video_frames = 0;
	for (filesystem::directory_iterator i = filesystem::directory_iterator (film->j2k_dir()); i != filesystem::directory_iterator(); ++i) {
		++video_frames;
	}

	float const video_length = video_frames / film->frames_per_second();
	cout << "Video length: " << video_length << " (" << video_frames << " frames at " << film->frames_per_second() << " frames per second).\n";

	vector<string> audio_files = film->audio_files ();
	if (audio_files.empty ()) {
		cerr << argv[0] << ": film has no audio files.\n";
		exit (EXIT_FAILURE);
	}

	sf_count_t audio_frames = 0;
	int audio_sample_rate = 0;

	for (vector<string>::iterator i = audio_files.begin(); i != audio_files.end(); ++i) {
		SF_INFO info;
		info.format = 0;
		SNDFILE* sf = sf_open (i->c_str(), SFM_READ, &info);
		if (sf == 0) {
			cerr << argv[0] << ": could not open WAV file for reading.\n";
			exit (EXIT_FAILURE);
		}

		if (audio_frames == 0) {
			audio_frames = info.frames;
		}

		if (audio_sample_rate == 0) {
			audio_sample_rate = info.samplerate;
		}

		if (audio_frames != info.frames) {
			cerr << argv[0] << ": audio files have differing lengths.\n";
			exit (EXIT_FAILURE);
		}

		if (audio_sample_rate != info.samplerate) {
			cerr << argv[0] << ": audio files have differing sample rates.\n";
			exit (EXIT_FAILURE);
		}

		sf_close (sf);
	}

	float const audio_length = audio_frames / float (audio_sample_rate);

	cout << "Audio length: " << audio_length << " (" << audio_frames << " frames at " << audio_sample_rate << " frames per second).\n";

	cout << "\n";

	if (audio_length > video_length) {
		cout << setprecision (3);
		cout << "Audio " << (audio_length - video_length) << "s longer than video.\n";
		
		float const delta = audio_length - video_length;
		int const delta_samples = delta * audio_sample_rate;
		
		if (chop_audio_start) {
			cout << "Chopping difference off the start of the audio.\n";

			stringstream s;
			s << "trim " << delta_samples << "s";
			sox (audio_files, s.str ());
			
		} else if (chop_audio_end) {
			cout << "Chopping difference off the end of the audio.\n";

			stringstream s;
			s << "reverse trim " << delta_samples << "s reverse";
			sox (audio_files, s.str ());

		} else {
			cout << "Re-run with --chop-audio-start or --chop-audio-end, perhaps.\n";
		}

	} else if (audio_length < video_length) {
		cout << setprecision (3);
		cout << "Audio " << (video_length - audio_length) << "s shorter than video.\n";

		if (pad_audio_end) {

			float const delta = video_length - audio_length;
			int const delta_samples = delta * audio_sample_rate;
			stringstream s;
			s << "pad 0 " << delta_samples << "s";
			sox (audio_files, s.str ());
		
		} else {
			cout << "Re-run with --pad-audio-end, perhaps.\n";
		}
	}
	

	return 0;
}
