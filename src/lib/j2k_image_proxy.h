/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "image_proxy.h"
#include "data.h"
#include <dcp/util.h>

namespace dcp {
	class MonoPictureFrame;
	class StereoPictureFrame;
}

class Data;

class J2KImageProxy : public ImageProxy
{
public:
	J2KImageProxy (boost::filesystem::path path, dcp::Size);
	J2KImageProxy (boost::shared_ptr<const dcp::MonoPictureFrame> frame, dcp::Size);
	J2KImageProxy (boost::shared_ptr<const dcp::StereoPictureFrame> frame, dcp::Size, dcp::Eye);
	J2KImageProxy (boost::shared_ptr<cxml::Node> xml, boost::shared_ptr<Socket> socket);

	boost::shared_ptr<Image> image (boost::optional<dcp::NoteHandler> note = boost::optional<dcp::NoteHandler> ()) const;
	void add_metadata (xmlpp::Node *) const;
	void send_binary (boost::shared_ptr<Socket>) const;
	/** @return true if our image is definitely the same as another, false if it is probably not */
	virtual bool same (boost::shared_ptr<const ImageProxy>) const;

	Data j2k () const {
		return _data;
	}

	dcp::Size size () const {
		return _size;
	}

private:
	Data _data;
	dcp::Size _size;
	boost::optional<dcp::Eye> _eye;
};
