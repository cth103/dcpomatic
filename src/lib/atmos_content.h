/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#include "content_part.h"
#include "immersive_audio_origin.h"
#include "types.h"
#include <dcp/types.h>
#include <boost/optional.hpp>


class Content;


class AtmosContentProperty
{
public:
	static int constexpr EDIT_RATE = 700;
	static int constexpr ORIGIN = 701;
};


class AtmosContent : public ContentPart
{
public:
	explicit AtmosContent (Content* parent);
	AtmosContent (Content* parent, cxml::ConstNodePtr node);

	static std::shared_ptr<AtmosContent> from_xml (Content* parent, cxml::ConstNodePtr node);

	void as_xml(xmlpp::Element* element) const;

	void set_length (Frame len);

	Frame length () const {
		return _length;
	}

	void set_edit_rate (dcp::Fraction rate);

	dcp::Fraction edit_rate () const {
		return _edit_rate;
	}

	void set_origin(boost::optional<ImmersiveAudioOrigin> origin);
	/** @return who made the track file, if it could be seen (see immersive_audio_origin()) */
	boost::optional<ImmersiveAudioOrigin> origin() const {
		return _origin;
	}
	/** @return true if the track file is known to be Dolby Atmos; otherwise it is called IAB */
	bool dolby() const {
		return _origin == ImmersiveAudioOrigin::DOLBY;
	}

private:
	Frame _length;
	dcp::Fraction _edit_rate;
	boost::optional<ImmersiveAudioOrigin> _origin;
};

