/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/exceptions.h
 *  @brief Our exceptions.
 */

#ifndef DCPOMATIC_EXCEPTIONS_H
#define DCPOMATIC_EXCEPTIONS_H

extern "C" {
#include <libavutil/pixfmt.h>
}
#include <boost/filesystem.hpp>
#include <stdexcept>
#include <cstring>

/** @class DecodeError
 *  @brief A low-level problem with the decoder (possibly due to the nature
 *  of a source file).
 */
class DecodeError : public std::runtime_error
{
public:
	explicit DecodeError (std::string s)
		: std::runtime_error (s)
	{}
};

/** @class EncodeError
 *  @brief A low-level problem with an encoder.
 */
class EncodeError : public std::runtime_error
{
public:
	explicit EncodeError (std::string s)
		: std::runtime_error (s)
	{}
};

/** @class FileError.
 *  @brief Parent class for file-related errors.
 */
class FileError : public std::runtime_error
{
public:
	/** @param m Error message.
	 *  @param f Name of the file that this exception concerns.
	 */
	FileError (std::string m, boost::filesystem::path f)
		: std::runtime_error (m)
		, _file (f)
	{}

	virtual ~FileError () throw () {}

	/** @return name of the file that this exception concerns */
	boost::filesystem::path file () const {
		return _file;
	}

private:
	/** name of the file that this exception concerns */
	boost::filesystem::path _file;
};

class JoinError : public std::runtime_error
{
public:
	explicit JoinError (std::string s)
		: std::runtime_error (s)
	{}
};

/** @class OpenFileError.
 *  @brief Indicates that some error occurred when trying to open a file.
 */
class OpenFileError : public FileError
{
public:
	/** @param f File that we were trying to open.
	 *  @param error Code of error that occurred.
	 *  @param reading true if we were opening to read, false if opening to write.
	 */
	OpenFileError (boost::filesystem::path f, int error, bool reading);
};

/** @class ReadFileError.
 *  @brief Indicates that some error occurred when trying to read from a file
 */
class ReadFileError : public FileError
{
public:
	/** @param f File that we were trying to read from.
	 *  @param e errno value, or 0.
	 */
	ReadFileError (boost::filesystem::path f, int e = 0);
};

/** @class WriteFileError.
 *  @brief Indicates that some error occurred when trying to write to a file
 */
class WriteFileError : public FileError
{
public:
	/** @param f File that we were trying to write to.
	 *  @param e errno value, or 0.
	 */
	WriteFileError (boost::filesystem::path f, int e);
};

/** @class SettingError.
 *  @brief Indicates that something is wrong with a setting.
 */
class SettingError : public std::runtime_error
{
public:
	/** @param s Name of setting that was required.
	 *  @param m Message.
	 */
	SettingError (std::string s, std::string m)
		: std::runtime_error (m)
		, _setting (s)
	{}

	virtual ~SettingError () throw () {}

	/** @return name of setting in question */
	std::string setting () const {
		return _setting;
	}

private:
	std::string _setting;
};

/** @class MissingSettingError.
 *  @brief Indicates that a Film is missing a setting that is required for some operation.
 */
class MissingSettingError : public SettingError
{
public:
	/** @param s Name of setting that was required */
	explicit MissingSettingError (std::string s);
};

/** @class BadSettingError
 *  @brief Indicates that a setting is bad in some way.
 */
class BadSettingError : public SettingError
{
public:
	/** @param s Name of setting that is bad.
	 *  @param m Error message.
	 */
	BadSettingError (std::string s, std::string m)
		: SettingError (s, m)
	{}
};

/** @class NetworkError
 *  @brief Indicates some problem with communication on the network.
 */
class NetworkError : public std::runtime_error
{
public:
	explicit NetworkError (std::string s)
		: std::runtime_error (s)
	{}
};

/** @class KDMError
 *  @brief A problem with a KDM.
 */
class KDMError : public std::runtime_error
{
public:
	KDMError (std::string s, std::string d);

	std::string summary () const {
		return _summary;
	}

	std::string detail () const {
		return _detail;
	}

private:
	std::string _summary;
	std::string _detail;
};

/** @class PixelFormatError
 *  @brief A problem with an unsupported pixel format.
 */
class PixelFormatError : public std::runtime_error
{
public:
	PixelFormatError (std::string o, AVPixelFormat f);
};

/** @class TextSubtitleError
 *  @brief An error that occurs while parsing a TextSubtitleError file.
 */
class TextSubtitleError : public FileError
{
public:
	TextSubtitleError (std::string, std::string, boost::filesystem::path);
};

class DCPError : public std::runtime_error
{
public:
	explicit DCPError (std::string s)
		: std::runtime_error (s)
	{}
};

class InvalidSignerError : public std::runtime_error
{
public:
	InvalidSignerError ();
	explicit InvalidSignerError (std::string reason);
};

class ProgrammingError : public std::runtime_error
{
public:
	ProgrammingError (std::string file, int line, std::string message = "");
};

class TextEncodingError : public std::runtime_error
{
public:
	explicit TextEncodingError (std::string s)
		: std::runtime_error (s)
	{}
};

class OldFormatError : public std::runtime_error
{
public:
	explicit OldFormatError (std::string s)
		: std::runtime_error (s)
	{}
};

class KDMAsContentError : public std::runtime_error
{
public:
	KDMAsContentError ();
};

#endif
