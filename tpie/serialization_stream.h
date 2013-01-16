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
#include <tpie/array.h>

namespace tpie {

///////////////////////////////////////////////////////////////////////////////
/// \class Stream accessor for serialization streams.
///
/// This class handles the stream header of a given file accessor.
///////////////////////////////////////////////////////////////////////////////
class serialization_header {
public:
	static memory_size_type header_size() {
		memory_size_type sz = sizeof(stream_header_t);
		memory_size_type align = 4096;
		return (sz + (align-1))/align * align;
	}

	serialization_header(file_accessor::raw_file_accessor & file)
		: m_fileAccessor(file)
	{
		m_header.magic = stream_header_t::magicConst;
		m_header.version = stream_header_t::versionConst;
		m_header.size = 0;
		m_header.cleanClose = 0;
	}

	void read() {
		m_fileAccessor.seek_i(0);
		m_fileAccessor.read_i(&m_header, sizeof(m_header));
	}

	void write(bool cleanClose) {
		m_header.cleanClose = cleanClose;

		tpie::array<char> headerArea(header_size());
		std::fill(headerArea.begin(), headerArea.end(), '\x42');
		char * headerData = reinterpret_cast<char *>(&m_header);
		std::copy(headerData, sizeof(m_header) + headerData,
				  headerArea.begin());

		m_fileAccessor.seek_i(0);
		m_fileAccessor.write_i(&headerArea[0], headerArea.size());
	}

	void verify() {
		if (m_header.magic != m_header.magicConst)
			throw stream_exception("Bad header magic");
		if (m_header.version < m_header.versionConst)
			throw stream_exception("Stream version too old");
		if (m_header.version > m_header.versionConst)
			throw stream_exception("Stream version too new");
		if (m_header.cleanClose != 1)
			throw stream_exception("Stream was not closed properly");
	}

	stream_size_type get_size() {
		return m_header.size;
	}

	void set_size(stream_size_type size) {
		m_header.size = size;
	}

	bool get_clean_close() {
		return m_header.cleanClose;
	}

private:
#pragma pack(push, 1)
	struct stream_header_t {
		static const uint64_t magicConst = 0xfa340f49edbada67ll;
		static const uint64_t versionConst = 1;

		uint64_t magic;
		uint64_t version;
		uint64_t size;
		// bool has funny semantics with regards to equality. we want to reject
		// invalid bool values (>1), but that is not easy to express with a C++
		// bool variable.
		char cleanClose;
		// TODO: add reverse
	};
#pragma pack(pop)

	stream_header_t m_header;

	file_accessor::raw_file_accessor & m_fileAccessor;
};

class serialization_writer_base {
public:
	static stream_size_type block_size() {
		return 2*1024*1024;
	}

private:
	file_accessor::raw_file_accessor m_fileAccessor;
	stream_size_type m_blocksWritten;
	stream_size_type m_size;
	bool m_open;

protected:
	serialization_writer_base()
		: m_blocksWritten(0)
		, m_size(0)
		, m_open(false)
	{
	}

	void open(std::string path, bool reverse) {
		close();
		m_fileAccessor.set_cache_hint(access_sequential);
		m_fileAccessor.open_wo(path);
		m_blocksWritten = 0;
		m_size = 0;
		m_open = true;

		serialization_header header(m_fileAccessor);
		header.write(false);
	}

	///////////////////////////////////////////////////////////////////////////
	/// \brief  Write n bytes from memory area s to next block in stream.
	///
	/// n must be less or equal to block_size().
	///
	/// \param s  Memory area with data to write.
	/// \param n  Number of bytes to write.
	///////////////////////////////////////////////////////////////////////////
	void write_block(const char * const s, const memory_size_type n) {
		assert(n <= block_size());
		stream_size_type offset = m_blocksWritten * block_size();
		m_fileAccessor.seek_i(serialization_header::header_size() + offset);
		m_fileAccessor.write_i(s, n);
		++m_blocksWritten;
		m_size = offset + n;
	}

	void close() {
		if (!m_open) return;
		serialization_header header(m_fileAccessor);
		header.set_size(m_size);
		header.write(true);
		m_fileAccessor.close_i();
		m_open = false;
	}
};

class serialization_writer : public serialization_writer_base {
private:
	typedef serialization_writer_base p_t;

	tpie::array<char> m_block;
	memory_size_type m_index;

	void write_block() {
		p_t::write_block(m_block.get(), m_index);
		m_index = 0;
	}

	class serializer {
		serialization_writer & wr;

	public:
		serializer(serialization_writer & wr) : wr(wr) {}

		void write(const char * const s, const memory_size_type n) {
			const char * i = s;
			memory_size_type written = 0;
			while (written != n) {
				if (wr.m_index >= wr.block_size()) wr.write_block();

				memory_size_type remaining = n - written;
				memory_size_type blockRemaining = wr.block_size() - wr.m_index;

				memory_size_type writeSize = std::min(remaining, blockRemaining);

				std::copy(i, i + writeSize, &wr.m_block[wr.m_index]);
				i += writeSize;
				written += writeSize;
				wr.m_index += writeSize;
			}
		}
	};

	friend class serializer;

public:
	void open(std::string path) {
		p_t::open(path, false);
		m_block.resize(block_size());
		m_index = 0;
	}

	void close() {
		if (m_index > 0) write_block();
		m_block.resize(0);
		m_index = 0;
		p_t::close();
	}

	///////////////////////////////////////////////////////////////////////////
	/// \brief  Serialize a serializable item and write it to the stream.
	///
	/// The code stream.serialize(v) just calls serialize(stream, v) via ADL.
	///////////////////////////////////////////////////////////////////////////
	template <typename T>
	void serialize(const T & v) {
		using tpie::serialize;
		serializer s(*this);
		serialize(s, v);
	}

	///////////////////////////////////////////////////////////////////////////
	/// \brief  Serialize a sequence of serializable items and write them to
	/// the stream.
	///
	/// The code stream.serialize(a, b) just calls serialize(stream, a, b) via
	/// ADL.
	///////////////////////////////////////////////////////////////////////////
	template <typename IT>
	void serialize(IT a, IT b) {
		using tpie::serialize;
		serializer s(*this);
		serialize(s, a, b);
	}
};

class serialization_reader_base {
public:
	static stream_size_type block_size() {
		return serialization_writer_base::block_size();
	}

private:
	file_accessor::raw_file_accessor m_fileAccessor;
	bool m_open;

protected:
	tpie::array<char> m_block;
	stream_size_type m_size;
	memory_size_type m_index;
	memory_size_type m_blockSize;

	serialization_reader_base()
		: m_open(false)
		, m_size(0)
		, m_index(0)
		, m_blockSize(0)
	{
	}

	void open(std::string path, bool reverse) {
		close();
		m_fileAccessor.set_cache_hint(reverse ? access_normal : access_sequential);
		m_fileAccessor.open_ro(path);
		m_open = true;
		m_block.resize(block_size());

		serialization_header header(m_fileAccessor);
		header.read();
		header.verify();
		m_size = header.get_size();
		// TODO: use raii to ensure we set m_open to false and don't leak fds
		// when verification fails.
	}

	void read_block(const stream_size_type blk) {
		stream_size_type from = blk * block_size();
		stream_size_type to = std::min(from + block_size(), m_size);
		if (to <= from) throw end_of_stream_exception();
		m_index = 0;
		m_blockSize = to-from;
		m_fileAccessor.seek_i(serialization_header::header_size()
							  + from);
		m_fileAccessor.read_i(m_block.get(), m_blockSize);
	}

	// Check if EOF is reached, call read_block(blk) to reset m_index/m_blockSize.
	virtual void next_block() = 0;

public:
	void close() {
		if (!m_open) return;
		m_fileAccessor.close_i();
		m_open = false;
		m_block.resize(0);
	}

	///////////////////////////////////////////////////////////////////////////
	/// \brief  Read n bytes from stream into buffer starting at s.
	///
	/// \param s  Buffer to contain the read data.
	/// \param n  Number of bytes to read.
	///////////////////////////////////////////////////////////////////////////
	void read(char * const s, const memory_size_type n) {
		// TODO: inline some of this
		char * i = s;
		memory_size_type written = 0;
		while (written != n) {
			if (m_index >= m_blockSize) {
				// virtual invocation
				next_block();
			}

			memory_size_type remaining = n - written;
			memory_size_type blockRemaining = block_size() - m_index;

			memory_size_type readSize = std::min(remaining, blockRemaining);

			i = std::copy(m_block.get() + m_index,
						  m_block.get() + (m_index + readSize),
						  i);

			written += readSize;
			m_index += readSize;
		}
	}

	///////////////////////////////////////////////////////////////////////////
	/// \brief  Unserialize an unserializable item from the stream.
	///
	/// An item of the given type must exist at the current point in the
	/// stream.
	///
	/// The code stream.unserialize(v) just calls unserialize(stream, v) via
	/// ADL.
	///////////////////////////////////////////////////////////////////////////
	template <typename T>
	void unserialize(T & v) {
		using tpie::unserialize;
		unserialize(*this, v);
	}

	///////////////////////////////////////////////////////////////////////////
	/// \brief  Unserialize a sequence of unserializable items from the stream.
	///
	/// A sequence of the given item type must exist at the current point in
	/// the stream.
	///
	/// The code stream.unserialize(a, b) just calls unserialize(stream, a, b)
	/// via ADL.
	///////////////////////////////////////////////////////////////////////////
	template <typename IT>
	void unserialize(IT a, IT b) {
		using tpie::unserialize;
		unserialize(*this, a, b);
	}
};

class serialization_reader : public serialization_reader_base {
	typedef serialization_reader_base p_t;
	stream_size_type m_blockNumber;

	void read_from(stream_size_type offset) {
		read_block(offset);
	}

protected:
	virtual void next_block() /*override*/ {
		if (m_blockSize == 0) {
			m_blockNumber = 0;
		} else {
			++m_blockNumber;
		}
		read_block(m_blockNumber);
	}

public:
	serialization_reader()
		: m_blockNumber(0)
	{
	}

	void open(std::string path) {
		p_t::open(path, false);
		m_blockNumber = 0;
	}

	bool can_read() {
		if (m_index < m_blockSize) return true;
		return m_blockNumber * block_size() + m_index < m_size;
	}
};

}

#endif // __TPIE_SERIALIZATION_STREAM_H__
