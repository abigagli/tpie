// -*- mode: c++; tab-width: 4; indent-tabs-mode: t; eval: (progn (c-set-style "stroustrup") (c-set-offset 'innamespace 0)); -*-
// vi:set ts=4 sts=4 sw=4 noet :
// Copyright 2012, The TPIE development team
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

#ifndef __TPIE_PIPELINING_TOKENS_H__
#define __TPIE_PIPELINING_TOKENS_H__

namespace tpie {

namespace pipelining {

struct pipe_segment;

struct segment_token;

enum segment_relation {
	pushes,
	pulls,
	depends
};

struct segment_map {
	typedef uint64_t id_t;
	typedef const pipe_segment * val_t;

	typedef std::map<id_t, val_t> map_t;
	typedef map_t::const_iterator mapit;

	typedef std::multimap<id_t, std::pair<id_t, segment_relation> > relmap_t;
	typedef relmap_t::const_iterator relmapit;

	typedef boost::shared_ptr<segment_map> ptr;
	typedef boost::weak_ptr<segment_map> wptr;

	static inline ptr create() {
		ptr result(new segment_map);
		result->self = wptr(result);
		return result;
	}

	inline id_t add_token(val_t token) {
		id_t id = nextId++;
		set_token(id, token);
		return id;
	}

	inline void set_token(id_t id, val_t token) {
		m_tokens.insert(std::make_pair(id, token));
	}

	// union-find link
	void link(ptr target) {
		if (target.get() == this) {
			// self link attempted
			// we must never have some_map->m_authority point to some_map,
			// since it would create a reference cycle
			return;
		}
		// union by rank
		if (target->m_rank > m_rank)
			return target->link(ptr(self));

		for (mapit i = target->begin(); i != target->end(); ++i) {
			set_token(i->first, i->second);
		}
		for (relmapit i = target->m_relations.begin(); i != target->m_relations.end(); ++i) {
			m_relations.insert(*i);
		}
		target->m_tokens.clear();
		target->m_authority = ptr(self);

		// union by rank
		if (target->m_rank == m_rank)
			++m_rank;
	}

	inline void union_set(ptr target) {
		find_authority()->link(target->find_authority());
	}

	inline val_t get(id_t id) {
		mapit i = m_tokens.find(id);
		if (i == m_tokens.end()) return 0;
		return i->second;
	}

	inline mapit begin() const {
		return m_tokens.begin();
	}

	inline mapit end() const {
		return m_tokens.end();
	}

	// union-find
	inline ptr find_authority() {
		if (!m_authority)
			return ptr(self);

		segment_map * i = m_authority.get();
		while (i->m_authority) {
			i = i->m_authority.get();
		}
		ptr result(i->self);

		// path compression
		segment_map * j = m_authority.get();
		while (j->m_authority) {
			segment_map * k = j->m_authority.get();
			j->m_authority = result;
			j = k;
		}

		return result;
	}

	inline void add_relation(id_t from, id_t to, segment_relation rel) {
		m_relations.insert(std::make_pair(from, std::make_pair(to, rel)));
	}

	inline const relmap_t & get_relations() const {
		return m_relations;
	}

private:
	map_t m_tokens;
	relmap_t m_relations;

	wptr self;

	// union rank structure
	ptr m_authority;
	size_t m_rank;

	inline segment_map()
		: m_rank(0)
	{
	}

	static id_t nextId;
};

struct segment_token {
	typedef segment_map::id_t id_t;

	// Use for the simple case in which a pipe_segment owns its own token
	inline segment_token(const pipe_segment * owner)
		: m_tokens(segment_map::create())
		, m_id(m_tokens->add_token(owner))
		, m_free(false)
	{
	}

	// This copy constructor has two uses:
	// 1. Simple case when a pipe_segment is copied (freshToken = false)
	// 2. Advanced case when a pipe_segment is being constructed with a specific token (freshToken = true)
	inline segment_token(const segment_token & other, const pipe_segment * newOwner, bool freshToken = false)
		: m_tokens(other.m_tokens)
		, m_id(other.id())
		, m_free(false)
	{
		if (freshToken) {
			tp_assert(other.m_free, "Trying to take ownership of a non-free token");
			tp_assert(m_tokens->get(m_id) == 0, "A token already has an owner, but m_free is true - contradiction");
		} else {
			tp_assert(!other.m_free, "Trying to copy a free token");
		}
		m_tokens->set_token(m_id, newOwner);
	}

	// Use for the advanced case when a segment_token is allocated before the pipe_segment
	inline segment_token()
		: m_tokens(segment_map::create())
		, m_id(m_tokens->add_token(0))
		, m_free(true)
	{
	}

	inline size_t id() const { return m_id; }

	inline segment_map::ptr map_union(const segment_token & with) {
		if (m_tokens == with.m_tokens) return m_tokens;
		m_tokens->union_set(with.m_tokens);
		return m_tokens = m_tokens->find_authority();
	}

	inline segment_map::ptr get_map() const {
		return m_tokens;
	}

private:
	segment_map::ptr m_tokens;
	size_t m_id;
	bool m_free;
};

segment_map::id_t segment_map::nextId = 0;

} // namespace pipelining

} // namespace tpie

#endif // __TPIE_PIPELINING_TOKENS_H__