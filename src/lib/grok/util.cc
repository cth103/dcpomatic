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


using std::string;
using std::vector;


vector<string>
get_gpu_names(boost::filesystem::path binary, boost::filesystem::path filename)
{
    // Execute the GPU listing program and redirect its output to a file
    if (std::system((binary.string() + " > " + filename.string()).c_str()) < 0) {
	    return {};
    }

    std::vector<std::string> gpu_names;
    std::ifstream file(filename.c_str());
    if (file.is_open())
    {
        std::string line;
        while (std::getline(file, line))
            gpu_names.push_back(line);
        file.close();
    }

    return gpu_names;
}


