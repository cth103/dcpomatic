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
#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "i18n.h"


using std::string;


namespace {

/** How many frames are looked at, at most, for an object with the silence gain prefix and for how the audio is coded */
int constexpr max_frames_looked_at = 1000;


/** @return true if @p frame starts with Dolby's preamble: PreambleTag 0x01, a 32-bit PreambleLength, then 11 01 0E,
 *  or 11 01 02 as Dolby's older encoders (Dolby Atmos Encoder 1.0.0.2, Atmos Encoder 1.9.45) write it, with IAFrame
 *  version 0.
 */
bool
dolby_preamble(uint8_t const* frame, ui32_t size)
{
	return size >= 8 && frame[0] == 0x01 && frame[5] == 0x11 && frame[6] == 0x01 && (frame[7] == 0x0e || frame[7] == 0x02);
}


/** Reads the bit fields of ST 2098-2 (most significant bit first) and throws std::out_of_range past the end */
class BitReader
{
public:
	BitReader(uint8_t const* data, size_t size)
		: _data(data)
		, _size(size)
	{}

	uint32_t bits(int n)
	{
		uint32_t value = 0;
		for (int i = 0; i < n; ++i) {
			if (_position / 8 >= _size) {
				throw std::out_of_range("IAFrame");
			}
			value = (value << 1) | ((_data[_position / 8] >> (7 - _position % 8)) & 1);
			++_position;
		}
		return value;
	}

	/** Plex(n) of ST 2098-2 5.2: a field of all ones is followed by one twice as long */
	uint32_t plex(int n)
	{
		while (true) {
			auto const value = bits(n);
			uint64_t const escape = (uint64_t{1} << n) - 1;
			if (value != escape || n >= 32) {
				return value;
			}
			n *= 2;
		}
	}

	void align()
	{
		_position = (_position + 7) / 8 * 8;
	}

	void skip_bytes(size_t n)
	{
		_position += n * 8;
	}

	size_t byte_position() const
	{
		return _position / 8;
	}

private:
	uint8_t const* _data;
	size_t _size;
	size_t _position = 0;
};


uint32_t
big_endian_32(uint8_t const* p)
{
	return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3];
}


/** @return true if an ObjectDefinition of this IABitstream frame has the silence gain prefix (0x1) in its first
 *  pan sub-block (ST 2098-2 9.4).  An IAB Profile 1 file (RDD 57 O5) never does; Dolby's encoders do for an
 *  inactive object.  A frame that cannot be parsed has none.
 */
bool
silent_object(uint8_t const* frame, ui32_t size)
{
	if (size < 5 || frame[0] != 0x01) {
		return false;
	}
	size_t const ia_tag = 5 + size_t{big_endian_32(frame + 1)};
	if (ia_tag + 5 > size || frame[ia_tag] != 0x02) {
		return false;
	}
	size_t const ia_start = ia_tag + 5;
	size_t const ia_size = std::min(size_t{big_endian_32(frame + ia_tag + 1)}, size - ia_start);
	auto const ia = frame + ia_start;

	try {
		BitReader reader(ia, ia_size);
		/* The IAFrame element */
		if (reader.plex(8) != 0x08) {
			return false;
		}
		reader.plex(8);   // ElementSize
		reader.bits(8);   // Version
		reader.bits(2);   // SampleRate
		reader.bits(2);   // BitDepth
		reader.bits(4);   // FrameRate
		reader.plex(8);   // MaxRendered
		reader.align();
		auto const sub_elements = reader.plex(8);
		for (uint32_t i = 0; i < sub_elements; ++i) {
			auto const id = reader.plex(8);
			auto const element_size = reader.plex(8);
			auto const body = reader.byte_position();
			if (id == 0x40) {
				/* ObjectDefinition */
				BitReader object(ia + body, std::min(size_t{element_size}, ia_size - body));
				object.plex(8);       // MetaID
				object.plex(8);       // AudioDataID
				if (object.bits(1)) { // ConditionalObject
					object.bits(1);   // Reserved
					object.bits(8);   // ObjectUseCase
				}
				object.bits(1);       // Reserved
				if (object.bits(2) == 0x1) {
					/* ObjectGainPrefix of the first sub-block: silence */
					return true;
				}
			}
			reader.skip_bytes(element_size);
		}
	} catch (std::out_of_range&) {

	}

	return false;
}


/** How the AudioDataDLC elements of a frame are coded, compared with what Dolby's encoders use */
enum class DLCCoding
{
	/** The frame has no AudioDataDLC element, or it cannot be read */
	NONE,
	/** Every element is coded within what Dolby's encoders use */
	LIKE_DOLBY,
	/** An element uses something Dolby's encoders do not */
	NOT_LIKE_DOLBY
};


/** Samples of a 48kHz frame for each IAFrame FrameRate code (ST 2098-2 table 18) */
uint32_t const frame_samples_48k[] = { 2000, 1920, 1600, 1000, 960, 800, 500, 480, 400, 2002 };


/** @return the number of sub-blocks of a 48kHz frame of @p samples samples and the samples in each (ST 2098-2 table 30),
 *  or { 0, 0 } if there is no such layout
 */
std::pair<uint32_t, uint32_t>
sub_block_layout(uint32_t samples)
{
	std::pair<uint32_t, uint32_t> const layouts[] = {
		{ 10, 200 }, { 10, 192 }, { 8, 200 }, { 5, 200 }, { 5, 192 }, { 4, 200 }, { 5, 100 }, { 4, 120 }, { 4, 100 }
	};
	for (auto const& layout: layouts) {
		if (layout.first * layout.second == samples) {
			return layout;
		}
	}
	return { 0, 0 };
}


/** @return true if the coded audio of one AudioDataDLC element (ST 2098-2 9.6, from DLCSampleRate on) is coded within
 *  what Dolby's encoders use, as measured in their track files from 2012 to 2026: DLCSampleRate 48kHz, ShiftBits 8
 *  to 12, at most one predictor region of order at most 16, and PCM sub-blocks only with BitDepth 0 (all zero).
 *  Throws std::out_of_range if the element is cut short.
 */
bool
dlc_element_like_dolby(BitReader& reader, std::pair<uint32_t, uint32_t> layout)
{
	if (reader.bits(2) != 0) {
		/* DLCSampleRate other than 48kHz */
		return false;
	}
	auto const shift_bits = reader.bits(5);
	if (shift_bits < 8 || shift_bits > 12) {
		return false;
	}
	auto const regions = reader.bits(2);
	if (regions > 1) {
		return false;
	}
	for (uint32_t i = 0; i < regions; ++i) {
		reader.bits(4);   // RegionLength
		auto const order = reader.bits(5);
		if (order > 16) {
			return false;
		}
		for (uint32_t j = 0; j < order; ++j) {
			reader.bits(10);   // KCoeff
		}
	}

	for (uint32_t block = 0; block < layout.first; ++block) {
		auto const rice = reader.bits(1);
		auto const bits = reader.bits(5);   // RiceRemBits, or the PCM BitDepth
		if (!rice) {
			if (bits != 0) {
				return false;
			}
			/* PCM of BitDepth 0: every residual is 0, with no bits and no sign */
			continue;
		}
		for (uint32_t sample = 0; sample < layout.second; ++sample) {
			uint32_t magnitude = 0;
			while (reader.bits(1)) {
				/* The unary quotient */
				magnitude = 1;
			}
			if (bits > 0 && reader.bits(bits)) {
				magnitude = 1;
			}
			if (magnitude) {
				reader.bits(1);   // Sign
			}
		}
	}

	return true;
}


/** @return how the AudioDataDLC elements of this IABitstream frame are coded */
DLCCoding
dlc_coding(uint8_t const* frame, ui32_t size)
{
	if (size < 5 || frame[0] != 0x01) {
		return DLCCoding::NONE;
	}
	size_t const ia_tag = 5 + size_t{big_endian_32(frame + 1)};
	if (ia_tag + 5 > size || frame[ia_tag] != 0x02) {
		return DLCCoding::NONE;
	}
	size_t const ia_start = ia_tag + 5;
	size_t const ia_size = std::min(size_t{big_endian_32(frame + ia_tag + 1)}, size - ia_start);
	auto const ia = frame + ia_start;

	bool any = false;
	try {
		BitReader reader(ia, ia_size);
		if (reader.plex(8) != 0x08) {
			return DLCCoding::NONE;
		}
		reader.plex(8);   // ElementSize
		reader.bits(8);   // Version
		reader.bits(2);   // SampleRate
		reader.bits(2);   // BitDepth
		auto const frame_rate = reader.bits(4);
		reader.plex(8);   // MaxRendered
		reader.align();
		if (frame_rate >= sizeof(frame_samples_48k) / sizeof(frame_samples_48k[0])) {
			return DLCCoding::NONE;
		}
		auto const layout = sub_block_layout(frame_samples_48k[frame_rate]);
		if (layout.first == 0) {
			return DLCCoding::NONE;
		}
		auto const sub_elements = reader.plex(8);
		for (uint32_t i = 0; i < sub_elements; ++i) {
			auto const id = reader.plex(8);
			auto const element_size = reader.plex(8);
			auto const body = reader.byte_position();
			if (id == 0x200) {
				/* AudioDataDLC */
				BitReader element(ia + body, std::min(size_t{element_size}, ia_size - body));
				element.plex(8);     // AudioDataID
				element.bits(16);    // DLCSize
				if (!dlc_element_like_dolby(element, layout)) {
					return DLCCoding::NOT_LIKE_DOLBY;
				}
				any = true;
			}
			reader.skip_bytes(element_size);
		}
	} catch (std::out_of_range&) {
		return DLCCoding::NONE;
	}

	return any ? DLCCoding::LIKE_DOLBY : DLCCoding::NONE;
}


/** What we need to see of a track file */
struct Start
{
	ASDCP::WriterInfo writer;
	bool dolby_preamble = false;
	bool silent_object = false;
	/** At least one frame has AudioDataDLC elements, and every one read is coded as Dolby's encoders do */
	bool dlc_like_dolby = false;
};


/** Read the Identification set and the frames we need with one of asdcplib's readers.
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

	start.dolby_preamble = dolby_preamble(buffer.RoData(), buffer.Size());
	if (!start.dolby_preamble || start.writer.CompanyName.compare(0, 5, "Dolby") == 0) {
		/* The other frames would change nothing */
		return true;
	}

	/* Dolby's preamble but another CompanyName: Dolby's frames wrapped again, if an object is coded as Dolby's
	 * encoders do and IAB Profile 1 does not allow, or if the audio is coded within what Dolby's encoders use.
	 */
	bool dlc_seen = false;
	bool dlc_other = false;
	for (int i = 0; i < max_frames_looked_at; ++i) {
		if (i > 0 && ASDCP_FAILURE(reader.ReadFrame(i, buffer))) {
			break;
		}
		if (silent_object(buffer.RoData(), buffer.Size())) {
			start.silent_object = true;
			break;
		}
		if (!dlc_other) {
			switch (dlc_coding(buffer.RoData(), buffer.Size())) {
			case DLCCoding::NONE:
				break;
			case DLCCoding::LIKE_DOLBY:
				dlc_seen = true;
				break;
			case DLCCoding::NOT_LIKE_DOLBY:
				dlc_other = true;
				break;
			}
		}
	}
	start.dlc_like_dolby = dlc_seen && !dlc_other;

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

	bool const company = start.writer.CompanyName.compare(0, 5, "Dolby") == 0;
	return start.dolby_preamble && (company || start.silent_object || start.dlc_like_dolby) ? ImmersiveAudioOrigin::DOLBY : ImmersiveAudioOrigin::IAB;
}
