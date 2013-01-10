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

#include <tpie/serialization_stream.h>
#include <tpie/array.h>

namespace tpie {

struct serialization_stream::block_t {
	memory_size_type size;
	stream_size_type offset;
	bool dirty;
	tpie::array<char> data;
};

#pragma pack(push, 1)
struct serialization_stream::stream_header_t {
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

void serialization_stream::init_header(stream_header_t & header) {
	header.magic = stream_header_t::magicConst;
	header.version = stream_header_t::versionConst;
	header.size = m_size;
	header.cleanClose = 0;
}

void serialization_stream::read_header(stream_header_t & header) {
	assert(m_canRead);

	m_fileAccessor.seek_i(0);
	m_fileAccessor.read_i(&header, sizeof(header));
}

memory_size_type serialization_stream::header_size() {
	memory_size_type sz = sizeof(stream_header_t);
	memory_size_type align = 4096;
	return (sz + (align-1))/align * align;
}

void serialization_stream::write_header(stream_header_t & header, bool cleanClose) {
	assert(m_canWrite);

	header.cleanClose = cleanClose;

	tpie::array<char> headerArea(header_size());
	std::fill(headerArea.begin(), headerArea.end(), '\x42');
	char * headerData = reinterpret_cast<char *>(&header);
	std::copy(headerData, sizeof(header) + headerData,
			  headerArea.begin());

	m_fileAccessor.seek_i(0);
	m_fileAccessor.write_i(&headerArea[0], headerArea.size());
}

void serialization_stream::verify_header(stream_header_t & header) {
	if (header.magic != header.magicConst)
		throw stream_exception("Bad header magic");
	if (header.version < header.versionConst)
		throw stream_exception("Stream version too old");
	if (header.version > header.versionConst)
		throw stream_exception("Stream version too new");
}

void serialization_stream::verify_clean_close(stream_header_t & header) {
	if (header.cleanClose != 1)
		throw stream_exception("Stream was not closed properly");
}

serialization_stream::serialization_stream()
	: m_block(new block_t)
	, m_index(0)
	, m_size(0)
	, m_open(false)
{
}

serialization_stream::~serialization_stream() {
	close();
}

/*static*/ stream_size_type serialization_stream::block_size() {
	return 2*1024*1024;
}

// TODO: add bit flags for read, write, clean close, reverse
void serialization_stream::open(std::string path, access_type accessType /*= access_read_write*/, bool requireCleanClose /*= true*/) {
	close();

	m_canRead = accessType == access_read || accessType == access_read_write;
	m_canWrite = accessType == access_write || accessType == access_read_write;

	m_size = 0;

	m_fileAccessor.set_cache_hint(access_sequential);
	stream_header_t header;
	init_header(header);
	switch (accessType) {
		case access_read_write:
			m_canRead = true;
			m_canWrite = true;
			if (m_fileAccessor.try_open_rw(path)) {
				read_header(header);
				verify_header(header);
				if (requireCleanClose)
					verify_clean_close(header);
			} else {
				m_fileAccessor.open_rw_new(path);
			}
			break;
		case access_read:
			m_canRead = true;
			m_canWrite = false;
			m_fileAccessor.open_ro(path);
			read_header(header);
			verify_header(header);
			break;
		case access_write:
			m_canRead = false;
			m_canWrite = true;
			m_fileAccessor.open_wo(path);
			break;
	}
	m_size = header.size;
	if (m_canWrite) {
		write_header(header, false);
	}
	m_block->data.resize(block_size());
	m_open = true;
	read_block(0);
}

void serialization_stream::close() {
	if (m_open) {
		flush_block();
		if (m_canWrite) {
			stream_header_t header;
			init_header(header);
			write_header(header, true);
		}
		m_fileAccessor.close_i();
	}
	m_block->data.resize(0);
	m_open = false;
}

char * serialization_stream::data() {
	return &m_block->data[0];
}

void serialization_stream::write_block() {
	assert(m_open);
	assert(m_canWrite);
	m_fileAccessor.seek_i(header_size() + m_block->offset);
	m_fileAccessor.write_i(data(), m_block->size);
}

void serialization_stream::read_block(stream_size_type offset) {
	assert(m_open);
	m_block->offset = offset;
	m_block->size = block_size();
	if (m_block->size + m_block->offset > size())
		m_block->size = size() - m_block->offset;
	m_fileAccessor.seek_i(header_size() + m_block->offset);
	if (m_block->size > 0) {
		assert(m_canRead);
		m_fileAccessor.read_i(data(), m_block->size);
	}
	m_index = 0;
	m_block->dirty = false;
}

void serialization_stream::flush_block() {
	if (m_block->dirty) write_block();
	m_block->dirty = false;
}

void serialization_stream::update_block() {
	flush_block();

	read_block(m_block->offset + block_size());
}

stream_size_type serialization_stream::offset() {
	return m_index + m_block->offset;
}

void serialization_stream::write(const char * const s, const memory_size_type count) {
	assert(m_canWrite);

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
		m_block->dirty = true;
		m_block->size = std::max(m_block->size, m_index);
		m_size = std::max(m_size, m_block->offset+m_index);
	}
}

void serialization_stream::read(char * const s, const memory_size_type count) {
	assert(m_canRead);

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

stream_size_type serialization_stream::size() {
	return m_size;
}

bool serialization_stream::can_read() {
	if (m_index < m_block->size)
		return true;
	else
		return offset() < size();
}

} // namespace tpie
