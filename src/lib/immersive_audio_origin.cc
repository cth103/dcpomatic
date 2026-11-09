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


#include "exceptions.h"
#include "immersive_audio_origin.h"
#include <dcp/scope_guard.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <asdcp/AS_DCP.h>
#include <asdcp/KM_fileio.h>
#include <asdcp/KM_log.h>
LIBDCP_ENABLE_WARNINGS
#include <cstring>

#include "i18n.h"


using std::string;


namespace {

/** What we need to see of a track file */
struct Start
{
	ASDCP::WriterInfo writer;
	/** The first bytes of frame 0 */
	std::vector<uint8_t> frame;
};


/** Read the Identification set and the start of frame 0 with one of asdcplib's readers.
 *  @return false if this reader cannot open the file.
 */
template <class Reader>
bool
read_start(boost::filesystem::path mxf, Start& start)
{
	Kumu::FileReaderFactory factory;
	Reader reader(factory);
	if (ASDCP_FAILURE(reader.OpenRead(mxf.string().c_str())) || ASDCP_FAILURE(reader.FillWriterInfo(start.writer))) {
		return false;
	}

	if (start.writer.EncryptedEssence) {
		throw FileError(_("This immersive audio file is encrypted, so who made it cannot be seen."), mxf);
	}

	ASDCP::DCData::FrameBuffer buffer(4 * Kumu::Megabyte);
	if (ASDCP_FAILURE(reader.ReadFrame(0, buffer))) {
		throw FileError(_("Could not read the first frame of this immersive audio file."), mxf);
	}

	auto const size = std::min(buffer.Size(), static_cast<ui32_t>(8));
	start.frame.assign(buffer.RoData(), buffer.RoData() + size);
	return true;
}

}


ImmersiveAudioOrigin
immersive_audio_origin(boost::filesystem::path mxf)
{
	Kumu::DefaultLogSink().UnsetFilterFlag(Kumu::LOG_ALLOW_ALL);
	dcp::ScopeGuard sg = []() {
		Kumu::DefaultLogSink().SetFilterFlag(Kumu::LOG_ALLOW_ALL);
	};

	/* ATMOS::MXFReader opens a file with the Dolby Atmos sub-descriptor; DCData::MXFReader also opens one without it */
	Start start;
	if (!read_start<ASDCP::ATMOS::MXFReader>(mxf, start) && !read_start<ASDCP::DCData::MXFReader>(mxf, start)) {
		throw FileError(_("This file is not an immersive audio track file."), mxf);
	}

	/* The preamble is PreambleTag 0x01, a 32-bit PreambleLength, then Dolby's own 11 01 0E */
	uint8_t const dolby_preamble[] = { 0x11, 0x01, 0x0e };
	bool const preamble = start.frame.size() >= 8 && start.frame[0] == 0x01 && memcmp(start.frame.data() + 5, dolby_preamble, sizeof(dolby_preamble)) == 0;
	bool const company = start.writer.CompanyName.compare(0, 5, "Dolby") == 0;

	return preamble && company ? ImmersiveAudioOrigin::DOLBY : ImmersiveAudioOrigin::IAB;
}
