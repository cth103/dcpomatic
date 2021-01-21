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


#include "lib/warnings.h"
#include <dcp/types.h>
#include <boost/filesystem.hpp>


class Film;
class Image;
class Log;


class TestPaths
{
public:
	static boost::filesystem::path private_data();
	static boost::filesystem::path xsd();
};

extern bool wait_for_jobs ();
extern void setup_test_config ();
extern std::shared_ptr<Film> new_test_film (std::string);
extern std::shared_ptr<Film> new_test_film2 (std::string);
extern void check_dcp (boost::filesystem::path, boost::filesystem::path);
extern void check_dcp (boost::filesystem::path, std::shared_ptr<const Film>);
extern void check_file (boost::filesystem::path ref, boost::filesystem::path check);
extern void check_wav_file (boost::filesystem::path ref, boost::filesystem::path check);
extern void check_mxf_audio_file (boost::filesystem::path ref, boost::filesystem::path check);
extern bool mxf_atmos_files_same (boost::filesystem::path ref, boost::filesystem::path check, bool verbose = false);
extern void check_xml (boost::filesystem::path, boost::filesystem::path, std::list<std::string>);
extern void check_file (boost::filesystem::path, boost::filesystem::path);
extern void check_ffmpeg (boost::filesystem::path, boost::filesystem::path, int audio_tolerance);
extern void check_image (boost::filesystem::path, boost::filesystem::path, double threshold = 4);
extern boost::filesystem::path test_film_dir (std::string);
extern void write_image (std::shared_ptr<const Image> image, boost::filesystem::path file);
boost::filesystem::path dcp_file (std::shared_ptr<const Film> film, std::string prefix);
void check_one_frame (boost::filesystem::path dcp, int64_t index, boost::filesystem::path ref);
extern boost::filesystem::path subtitle_file (std::shared_ptr<Film> film);
extern void make_random_file (boost::filesystem::path path, size_t size);

class LogSwitcher
{
public:
	LogSwitcher (std::shared_ptr<Log> log);
	~LogSwitcher ();

private:
	std::shared_ptr<Log> _old;
};

namespace dcp {

std::ostream& operator<< (std::ostream& s, dcp::Size i);
std::ostream& operator<< (std::ostream& s, Standard t);

}
