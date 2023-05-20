/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "compose.hpp"
extern "C" {
#include <libavutil/pixfmt.h>
}
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <sqlite3.h>
#include <cstring>
#include <stdexcept>


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

	DecodeError (std::string function, std::string caller)
		: std::runtime_error (String::compose("%1 failed [%2]", function, caller))
	{}

	DecodeError (std::string function, std::string caller, int error)
		: std::runtime_error (String::compose("%1 failed [%2] (%3)", function, caller, error))
	{}

	DecodeError (std::string function, std::string caller, boost::filesystem::path file)
		: std::runtime_error (String::compose("%1 failed [%2] (%3)", function, caller, file.string()))
	{}

	DecodeError (std::string function, std::string caller, int error, boost::filesystem::path file)
		: std::runtime_error (String::compose("%1 failed [%2] (%3) (%4)", function, caller, error, file.string()))
	{}
};


class CryptoError : public std::runtime_error
{
public:
	explicit CryptoError (std::string s)
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

	explicit EncodeError (std::string function, std::string caller)
		: std::runtime_error (String::compose("%1 failed [%2]", function, caller))
	{}

	explicit EncodeError (std::string function, std::string caller, int error)
		: std::runtime_error (String::compose("%1 failed [%2] (%3)", function, caller, error))
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
		: std::runtime_error (String::compose("%1 with %2", m, f.string()))
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
	enum Mode {
		READ,
		WRITE,
		READ_WRITE
	};

	/** @param f File that we were trying to open.
	 *  @param error Code of error that occurred.
	 *  @param mode Mode that we tried to open the file in.
	 */
	OpenFileError (boost::filesystem::path f, int error, Mode mode);
};


class FileNotFoundError : public std::runtime_error
{
public:
	FileNotFoundError (boost::filesystem::path f);
	virtual ~FileNotFoundError () throw () {}

	/** @return name of the file that this exception concerns */
	boost::filesystem::path file () const {
		return _file;
	}

private:
	/** name of the file that this exception concerns */
	boost::filesystem::path _file;
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
	explicit NetworkError (std::string s, boost::optional<std::string> d = boost::optional<std::string>());

	std::string summary () const {
		return _summary;
	}

	boost::optional<std::string> detail () const {
		return _detail;
	}

private:
	std::string _summary;
	boost::optional<std::string> _detail;
};


/** @class KDMError
 *  @brief A problem with a KDM.
 */
class KDMError : public std::runtime_error
{
public:
	KDMError (std::string s, std::string d);
	~KDMError () throw() {}

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


/** @class ProjectFolderError
 *  @brief An attempt has been made to read a DCP from a directory, but it looks
 *  like the directory actually contains a DCP-o-matic project.
 */
class ProjectFolderError : public DCPError
{
public:
	/* Code which catches this exception will provide their own message */
	ProjectFolderError ()
		: DCPError ("dummy")
	{}
};


class CPLNotFoundError : public DCPError
{
public:
	CPLNotFoundError(std::string id)
		: DCPError(String::compose("CPL %1 not found", id))
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


class MetadataError : public std::runtime_error
{
public:
	explicit MetadataError (std::string s)
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


class GLError : public std::runtime_error
{
public:
	GLError (char const* last, int e);
	GLError (char const* message);
};


/** @class CopyError
 *  @brief An error which occurs when copying a DCP to a distribution drive.
 */
class CopyError : public std::runtime_error
{
public:
	CopyError (std::string s, boost::optional<int> ext4_error = boost::optional<int>(), boost::optional<int> platform_error = boost::optional<int>());
	virtual ~CopyError () throw () {}

	std::string message () const {
		return _message;
	}

	boost::optional<int> ext4_number() const {
		return _ext4_number;
	}

	boost::optional<int> platform_number() const {
		return _platform_number;
	}

private:
	std::string _message;
	boost::optional<int> _ext4_number;
	boost::optional<int> _platform_number;
};


/** @class CommunicationFailedError
 *  @brief Communication between dcpomatic2_disk and _disk_writer failed somehow.
 */
class CommunicationFailedError : public CopyError
{
public:
	CommunicationFailedError ();
};


/** @class VerifyError
 *  @brief An error which occurs when verifying a DCP that we copied to a distribution drive.
 */
class VerifyError : public std::runtime_error
{
public:
	VerifyError (std::string s, int n);
	virtual ~VerifyError () throw () {}

	std::string message () const {
		return _message;
	}

	int number () const {
		return _number;
	}

private:
	std::string _message;
	int _number;
};


class PrivilegeError : public std::runtime_error
{
public:
	explicit PrivilegeError (std::string s)
		: std::runtime_error (s)
	{}
};


class MissingConfigurationError : public std::runtime_error
{
public:
	explicit MissingConfigurationError(std::string s)
		: std::runtime_error(s)
	{}
};


class SQLError : public std::runtime_error
{
public:
	SQLError(sqlite3* db, char const* s)
		: std::runtime_error(get_message(db, s))
	{
		_filename = get_filename(db);
	}

	SQLError(sqlite3* db, int rc)
		: std::runtime_error(get_message(db, rc))
	{
		_filename = get_filename(db);
	}

	SQLError(sqlite3* db, int rc, std::string doing)
		: std::runtime_error(get_message(db, rc, doing))
	{
		_filename = get_filename(db);
	}

	boost::filesystem::path filename() const {
		return _filename;
	}

private:
	boost::filesystem::path get_filename(sqlite3* db)
	{
		if (auto filename = sqlite3_db_filename(db, "main")) {
			return filename;
		}

		return {};
	}

	std::string get_message(sqlite3* db, char const* s)
	{
		return String::compose("%1 (in %2)", s, get_filename(db));
	}

	std::string get_message(sqlite3* db, int rc)
	{
		return String::compose("%1 (in %2)", sqlite3_errstr(rc), get_filename(db));
	}

	std::string get_message(sqlite3* db, int rc, std::string doing)
	{
		return String::compose("%1 (while doing %2) (in %3)", sqlite3_errstr(rc), doing, get_filename(db));
	}

	boost::filesystem::path _filename;
};


#endif
