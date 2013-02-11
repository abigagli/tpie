// -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup"; -*-
// vi:set ts=4 sts=4 sw=4 noet :
// Copyright 2009, 2010, The TPIE development team
//
// This file is part of TPIE.
//
// TPIE is free software: you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.
//
// TPIE is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
// License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with TPIE.  If not, see <http://www.gnu.org/licenses/>
#ifndef _TPIE_FILE_ACCESSOR_FILE_ACCESSOR_CRTP_H
#define _TPIE_FILE_ACCESSOR_FILE_ACCESSOR_CRTP_H

///////////////////////////////////////////////////////////////////////////////
/// \file stream_accessor.h  Reads and writes stream headers, user data and
/// blocks from TPIE streams.
///////////////////////////////////////////////////////////////////////////////

#include <tpie/file_accessor/file_accessor.h>
#include <tpie/stream_header.h>
#include <tpie/cache_hint.h>

namespace tpie {
namespace file_accessor {

template <typename file_accessor_t>
class TPIE_PUBLIC stream_accessor {
private:
	inline void validate_header(const stream_header_t & header);
	inline void fill_header(stream_header_t & header, bool clean);

	bool m_open;
	bool m_write;

	file_accessor_t m_fileAccessor;

	/** Number of logical items in stream. */
	stream_size_type m_size;

	/** Size (in bytes) of user data. */
	memory_size_type m_userDataSize;

	/** Maximum size (in bytes) of the user data. */
	memory_size_type m_maxUserDataSize;

	/** Size (in bytes) of a single logical item. */
	memory_size_type m_itemSize;

	/** Size (in bytes) of a single logical block. */
	memory_size_type m_blockSize;

	/** Number of logical items in a logical block. */
	memory_size_type m_blockItems;

	/** Path of the file currently opened. */
	std::string m_path;

	///////////////////////////////////////////////////////////////////////////
	/// \brief Read stream header into the file accessor properties and
	/// validate the type of the stream.
	///////////////////////////////////////////////////////////////////////////
	inline void read_header();

	///////////////////////////////////////////////////////////////////////////
	/// \brief Write the file accessor properties into the stream.
	///////////////////////////////////////////////////////////////////////////
	inline void write_header(bool clean);

	///////////////////////////////////////////////////////////////////////////
	/// \brief Returns the boundary on which we align blocks.
	///////////////////////////////////////////////////////////////////////////
	inline memory_size_type boundary() const { return 4096; }

	///////////////////////////////////////////////////////////////////////////
	/// \brief Given a memory offset, rounds up to the nearest alignment
	/// boundary.
	///////////////////////////////////////////////////////////////////////////
	inline memory_size_type align_to_boundary(memory_size_type z) const { return (z+boundary()-1)/boundary()*boundary(); }

	///////////////////////////////////////////////////////////////////////////
	/// \brief The size of header and user data with padding included. This is
	/// the offset at which the first logical block begins.
	///////////////////////////////////////////////////////////////////////////
	inline memory_size_type header_size() const { return align_to_boundary(sizeof(stream_header_t)+m_maxUserDataSize); }
public:
	inline stream_accessor()
		: m_open(false)
		, m_write(false)
	{
	}

	inline ~stream_accessor() {close();}

	///////////////////////////////////////////////////////////////////////////
	/// \brief Open file for reading and/or writing.
	///////////////////////////////////////////////////////////////////////////
	inline void open(const std::string & path,
					 bool read,
					 bool write,
					 memory_size_type itemSize,
					 memory_size_type blockSize,
					 memory_size_type maxUserDataSize,
					 cache_hint cacheHint);

	inline void close();

	///////////////////////////////////////////////////////////////////////////
	/// \brief Read the given number of items from the given block into the
	/// given buffer.
	/// \param data Buffer in which to store data. Must be able to hold at
	/// least sizeof(T)*itemCount bytes.
	/// \param blockNumber Number of block in which to begin reading.
	/// \param itemCount Number of items to read from beginning of given block.
	/// Must be less than m_blockItems.
	///////////////////////////////////////////////////////////////////////////
	inline memory_size_type read_block(void * data, stream_size_type blockNumber, memory_size_type itemCount);

	///////////////////////////////////////////////////////////////////////////
	/// \brief Write the given number of items from the given buffer into the
	/// given block.
	/// \param data Buffer from which to write data. Must hold at least
	/// sizeof(T)*itemCount bytes of data.
	/// \param blockNumber Number of block in which to write.
	/// \param itemCount Number of items to write to beginning of given block.
	/// Must be less than m_blockItems.
	///////////////////////////////////////////////////////////////////////////
	inline void write_block(const void * data, stream_size_type blockNumber, memory_size_type itemCount);

	///////////////////////////////////////////////////////////////////////////
	/// \brief Read user data into the given buffer.
	/// \param data Buffer in which to store user data.
	/// \param count Size of buffer, in bytes. The method will read at most
	/// this number of bytes.
	/// \returns Number of bytes of user data actually read, in case this is
	/// less than count.
	///////////////////////////////////////////////////////////////////////////
	inline memory_size_type read_user_data(void * data, memory_size_type count);

	///////////////////////////////////////////////////////////////////////////
	/// \brief Write user data to the stream.
	/// \param data Buffer from which to write user data.
	/// \param count Number of bytes to write. Cannot exceed
	/// max_user_data_size().
	///////////////////////////////////////////////////////////////////////////
	inline void write_user_data(const void * data, memory_size_type count);

	///////////////////////////////////////////////////////////////////////////
	/// \brief Return memory usage of this file accessor.
	///////////////////////////////////////////////////////////////////////////
	static inline memory_size_type memory_usage() {return sizeof(stream_accessor<file_accessor_t>);}

	///////////////////////////////////////////////////////////////////////////
	/// \brief Number of items in stream.
	///////////////////////////////////////////////////////////////////////////
	inline stream_size_type size() const {return m_size;}

	///////////////////////////////////////////////////////////////////////////
	/// \brief Path of the file currently open.
	///////////////////////////////////////////////////////////////////////////
	inline const std::string & path() const {return m_path;}

	///////////////////////////////////////////////////////////////////////////
	/// \brief Size (in bytes) of the user data.
	///////////////////////////////////////////////////////////////////////////
	inline memory_size_type user_data_size() const {return m_userDataSize;}

	///////////////////////////////////////////////////////////////////////////
	/// \brief Maximum size (in bytes) of the user data.
	///////////////////////////////////////////////////////////////////////////
	inline memory_size_type max_user_data_size() const {return m_maxUserDataSize;}

	///////////////////////////////////////////////////////////////////////////
	/// \brief Size (in bytes) of entire stream as laid out on disk after
	/// padding the final block to alignment boundary, including the header and
	/// user data.
	///////////////////////////////////////////////////////////////////////////
	inline stream_size_type byte_size() const {
		return ((m_size + m_blockItems - 1)/m_blockItems) * m_blockSize + header_size();
	}

	inline void truncate(stream_size_type items);
};

}
}

#include <tpie/file_accessor/stream_accessor.inl>
#endif //_TPIE_FILE_ACCESSOR_FILE_ACCESSOR_CRTP_H
