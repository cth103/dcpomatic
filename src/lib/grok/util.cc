/*
    Copyright (C) 2023 Grok Image Compression Inc.

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


#include "util.h"
#include "../config.h"
#include "../dcpomatic_log.h"
#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <future>


using std::string;
using std::vector;


vector<string>
get_gpu_names()
{
	auto const binary = Config::instance()->grok().binary_location / "gpu_lister";

#ifdef DCPOMATIC_BOOST_PROCESS_V1

	namespace bp = boost::process;

	bp::ipstream stream;

	try {
		bp::child child(binary, bp::std_out > stream);

		string line;
		vector<string> gpu_names;
		while (child.running() && std::getline(stream, line) && !line.empty()) {
			gpu_names.push_back(line);
		}

		child.wait();

		return gpu_names;
	} catch (std::exception& e) {
		LOG_ERROR("Could not fetch GPU names: {}", e.what());
		return {};
	}

#else

	namespace bp = boost::process::v2;

	try {
    boost::asio::io_context context;
    boost::asio::readable_pipe out{context};
    bp::process child(context, binary, {}, bp::process_stdio{{}, out, {}});

    string output;
    boost::system::error_code ec;
    while (child.running()) {
      string block;
      boost::asio::read(out, boost::asio::dynamic_buffer(block), ec);
      output += block;
      if (ec && ec == boost::asio::error::eof) {
        break;
      }
    }

    vector<string> lines;
    boost::algorithm::split(lines, output, boost::is_any_of("\n"));
    return lines;
  } catch (std::exception& e) {
		LOG_ERROR("Could not fetch GPU names: {}", e.what());
		return {};
  }
#endif
}


