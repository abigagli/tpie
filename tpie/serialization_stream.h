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

namespace tpie {

class serialization_stream {
	file_accessor::raw_file_accessor m_fileAccessor;

	struct block_t {
		static const stream_size_type none = -1;

		memory_size_type size;
		stream_size_type number;
		bool dirty;
		tpie::array<char> data;
	};

	block_t m_block;
	memory_size_type m_index;
	stream_size_type m_nextBlock;
	memory_size_type m_nextIndex;
	static const memory_size_type no_index = -1;

	bool m_open;

	///////////////////////////////////////////////////////////////////////////
	// Stream header type and helper methods.
	///////////////////////////////////////////////////////////////////////////
	struct stream_header_t {
		static const uint64_t magicConst = 0xfa340f49edbada67ll;
		static const uint64_t versionConst = 1;

		uint64_t magic;
		uint64_t version;
		uint64_t blockSize;
		uint64_t size;
		uint64_t cleanClose;
	};

	stream_header_t m_streamHeader;

	void init_header() {
		m_streamHeader.magic = stream_header_t::magicConst;
		m_streamHeader.version = stream_header_t::versionConst;
		m_streamHeader.blockSize = block_size();
		m_streamHeader.size = 0;
		m_streamHeader.cleanClose = 0;
	}

	void read_header() {
		m_fileAccessor.seek_i(0);
		m_fileAccessor.read_i(&m_streamHeader, sizeof(m_streamHeader));
	}

	memory_size_type header_size() {
		memory_size_type sz = sizeof(stream_header_t);
		memory_size_type align = 4096;
		return (sz + (align-1))/align * align;
	}

	void write_header(bool cleanClose) {
		m_streamHeader.cleanClose = cleanClose;

		tpie::array<char> headerArea(header_size());
		std::fill(headerArea.begin(), headerArea.end(), '\x42');
		char * headerData = reinterpret_cast<char *>(&m_streamHeader);
		std::copy(headerData, sizeof(m_streamHeader) + headerData,
				  headerArea.begin());

		m_fileAccessor.seek_i(0);
		m_fileAccessor.write_i(&headerArea[0], headerArea.size());
	}

	void verify_header() {
		if (m_streamHeader.magic != m_streamHeader.magicConst)
			throw stream_exception("Bad header magic");
		if (m_streamHeader.version < m_streamHeader.versionConst)
			throw stream_exception("Stream version too old");
		if (m_streamHeader.version > m_streamHeader.versionConst)
			throw stream_exception("Stream version too new");
	}

	void verify_clean_close() {
		if (m_streamHeader.cleanClose != 1)
			throw stream_exception("Stream was not closed properly");
	}

public:
	serialization_stream()
		: m_index(no_index)
		, m_nextBlock(block_t::none)
		, m_nextIndex(no_index)
		, m_open(false)
	{
	}

	~serialization_stream() {
		close();
	}

	static stream_size_type block_size() {
		return 2*1024*1024;
	}

	void open(std::string path, bool requireCleanClose = true) {
		close();

		m_fileAccessor.set_cache_hint(access_sequential);
		if (m_fileAccessor.try_open_rw(path)) {
			read_header();
			verify_header();
			if (requireCleanClose)
				verify_clean_close();
			write_header(false);
		} else {
			m_fileAccessor.open_rw_new(path);
			init_header();
			write_header(false);
		}
		m_block.size = 0;
		m_block.number = block_t::none;
		m_block.dirty = false;
		m_block.data.resize(block_size());
		m_index = no_index;
		m_nextIndex = 0;
		m_nextBlock = 0;
		m_open = true;
	}

	void close() {
		if (m_open) {
			flush_block();
			update_vars();
			write_header(true);
			m_fileAccessor.close_i();
		}
		m_block.data.resize(0);
		m_open = false;
	}

private:
	void update_vars() {
		m_streamHeader.size = std::max(m_streamHeader.size, offset());
	}

	char * data() {
		return &m_block.data[0];
	}

	void write_block(stream_size_type blockNumber) {
		m_fileAccessor.seek_i(header_size() + block_size() * blockNumber);
		m_fileAccessor.write_i(data(), m_block.size);
	}

	void read_block(stream_size_type blockNumber) {
		m_block.number = blockNumber;
		m_block.size = block_size();
		if (m_block.size + m_block.number * block_size() > size())
			m_block.size = size() - m_block.number * block_size();
		m_fileAccessor.seek_i(header_size() + block_size() * m_block.number);
		if (m_block.size > 0) {
			m_fileAccessor.read_i(data(), m_block.size);
		}
	}

	///////////////////////////////////////////////////////////////////////////
	/// \brief Flushes m_block to disk.
	///////////////////////////////////////////////////////////////////////////
	void flush_block() {
		if (m_block.dirty) write_block(m_block.number);
		m_block.dirty = false;
	}

	///////////////////////////////////////////////////////////////////////////
	/// \brief Fetches block indicated by m_nextBlock.
	///
	/// Clears the dirty bit and updates m_nextBlock, m_nextIndex.
	///////////////////////////////////////////////////////////////////////////
	void fetch_next_block() {
		read_block(m_nextBlock);
		m_index = m_nextIndex;
		m_nextBlock = block_t::none;
		m_nextIndex = no_index;
		m_block.dirty = false;
	}

	void update_block() {
		if (m_nextBlock == block_t::none) {
			m_nextBlock = m_block.number + 1;
			m_nextIndex = 0;
		}

		flush_block();

		fetch_next_block();
	}

public:
	void write(const char * const s, const memory_size_type count) {
		const char * i = s;
		memory_size_type written = 0;
		while (written != count) {
			if (m_index >= block_size()) update_block();

			memory_size_type remaining = count - written;
			memory_size_type blockRemaining = block_size() - m_index;

			memory_size_type writeSize = std::min(remaining, blockRemaining);

			std::copy(i, i + writeSize, data()+m_index);
			i += writeSize;
			written += writeSize;
			m_index += writeSize;
			m_block.dirty = true;
			m_block.size = std::max(m_block.size, m_index);
		}
	}

	void read(char * const s, const memory_size_type count) {
		char * i = s;
		memory_size_type written = 0;
		while (written != count) {
			if (m_index >= block_size()) {
				stream_size_type offs = offset();
				if (offs >= size()
					|| offs + (count - written) > size()) {

					throw end_of_stream_exception();
				}

				update_block();
			}

			memory_size_type remaining = count - written;
			memory_size_type blockRemaining = block_size() - m_index;

			memory_size_type readSize = std::min(remaining, blockRemaining);

			i = std::copy(data() + m_index,
						  data() + (m_index + readSize),
						  i);

			written += readSize;
			m_index += readSize;
		}
	}

	stream_size_type offset() {
		if (m_nextBlock == block_t::none)
			return m_index + m_block.number * block_size();
		else
			return m_nextIndex + m_nextBlock * block_size();
	}

	stream_size_type size() {
		update_vars();
		return m_streamHeader.size;
	}

	bool can_read(memory_size_type bytes = 1) {
		if (m_index <= m_block.size
			&& (m_index + bytes) <= m_block.size)
			return true;
		else
			return offset() + bytes <= size();
	}
};

}

#endif // __TPIE_SERIALIZATION_STREAM_H__
