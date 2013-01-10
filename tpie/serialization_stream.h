// -*- mode: c++; tab-width: 4; indent-tabs-mode: t; eval: (progn (c-set-style "stroustrup") (c-set-offset 'innamespace 0)); -*-
// vi:set ts=4 sts=4 sw=4 noet :
// Copyright 2013, The TPIE development team
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

#ifndef __TPIE_SERIALIZATION_STREAM_H__
#define __TPIE_SERIALIZATION_STREAM_H__

///////////////////////////////////////////////////////////////////////////////
/// \file serialization_stream.h  Stream of serializable items.
///////////////////////////////////////////////////////////////////////////////

#include <tpie/serialization2.h>
#include <tpie/file_accessor/file_accessor.h>
#include <memory>
#include <tpie/access_type.h>

namespace tpie {

class serialization_stream {
	struct block_t;
	struct stream_header_t;

public:
	///////////////////////////////////////////////////////////////////////////
	/// \brief  Constructor.
	///////////////////////////////////////////////////////////////////////////
	serialization_stream();

	///////////////////////////////////////////////////////////////////////////
	/// \brief  Destructor.
	///
	/// Closes any open stream.
	///////////////////////////////////////////////////////////////////////////
	~serialization_stream();

	///////////////////////////////////////////////////////////////////////////
	/// \brief  Return the block size used by the stream.
	///
	/// Currently, this is fixed to 2 MB.
	///////////////////////////////////////////////////////////////////////////
	static stream_size_type block_size();

	///////////////////////////////////////////////////////////////////////////
	/// \brief  Open stream.
	///
	/// \param path  Path to the file containing the stream.
	/// \param accessType  Either access_read, access_write or
	/// access_read_write.
	/// \param requireCleanClose  When opening a stream for reading, check that
	/// the cleanClose bit in the header is set.
	///////////////////////////////////////////////////////////////////////////
	void open(std::string path, access_type accessType = access_read_write, bool requireCleanClose = true);

	///////////////////////////////////////////////////////////////////////////
	/// \brief  Close stream.
	///
	/// If no stream is open, does nothing.
	///////////////////////////////////////////////////////////////////////////
	void close();

	///////////////////////////////////////////////////////////////////////////
	/// \brief  Write n bytes from memory area s to stream.
	///
	/// This is used by tpie::serialize to write a serialized item to the
	/// stream.
	///
	/// \param s  Memory area with data to write.
	/// \param n  Number of bytes to write.
	///////////////////////////////////////////////////////////////////////////
	void write(const char * const s, const memory_size_type n);

	///////////////////////////////////////////////////////////////////////////
	/// \brief  Read n bytes from stream into buffer starting at s.
	///
	/// \param s  Buffer to contain the read data.
	/// \param n  Number of bytes to read.
	///////////////////////////////////////////////////////////////////////////
	void read(char * const s, const memory_size_type n);

	///////////////////////////////////////////////////////////////////////////
	/// \brief  Get the byte size of the stream data.
	///
	/// Does not include the stream header.
	///
	/// \returns Byte size of the stream data.
	///////////////////////////////////////////////////////////////////////////
	stream_size_type size();

	///////////////////////////////////////////////////////////////////////////
	/// \brief  Check if end of stream is reached.
	///
	/// \return True if there is more data to be read; false at end of stream.
	///////////////////////////////////////////////////////////////////////////
	bool can_read();

private:
	char * data();
	void write_block();
	void read_block(stream_size_type offset);
	void flush_block();
	void update_block();
	stream_size_type offset();

	void init_header(stream_header_t & header);
	void read_header(stream_header_t & header);
	memory_size_type header_size();
	void write_header(stream_header_t & header, bool cleanClose);
	void verify_header(stream_header_t & header);
	void verify_clean_close(stream_header_t & header);

	file_accessor::raw_file_accessor m_fileAccessor;

	std::auto_ptr<block_t> m_block;
	memory_size_type m_index;
	stream_size_type m_size;

	bool m_open;

	bool m_canRead;
	bool m_canWrite;

};

}

#endif // __TPIE_SERIALIZATION_STREAM_H__
