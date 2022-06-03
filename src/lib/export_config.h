/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_EXPORT_CONFIG_H
#define DCPOMATIC_EXPORT_CONFIG_H


#include "types.h"
#include <libcxml/cxml.h>


class Config;


class ExportConfig
{
public:
	ExportConfig(Config* parent);

	void set_defaults();
	void read(cxml::ConstNodePtr node);
	void write(xmlpp::Element* node) const;

	ExportFormat format() const {
		return _format;
	}

	bool mixdown_to_stereo() const {
		return _mixdown_to_stereo;
	}

	bool split_reels() const {
		return _split_reels;
	}

	bool split_streams() const {
		return _split_streams;
	}

	int x264_crf() const {
		return _x264_crf;
	}

	void set_format(ExportFormat format);
	void set_mixdown_to_stereo(bool mixdown);
	void set_split_reels(bool split);
	void set_split_streams(bool split);
	void set_x264_crf(int crf);

private:
	Config* _config;
	ExportFormat _format;
	bool _mixdown_to_stereo;
	bool _split_reels;
	bool _split_streams;
	int _x264_crf;
};


#endif

