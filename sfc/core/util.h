/***************************************************************
 * util.h : utility template class implementation like STL
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * This file is part of libnbr.
 * libnbr is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.
 * libnbr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * You should have received a copy of
 * the GNU Lesser General Public License along with libnbr;
 * if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 ****************************************************************/

/*-------------------------------------------------------------*/
/* sfc::util::ringbuffer									   */
/*-------------------------------------------------------------*/
inline U8 *
ringbuffer::write(const U8 *p, U32 l, U32 &wsz) {
	/* TODO: instead of using lock object,
	 * can I make it faster using gcc4 atomic operations? */
	lock lk(m_lk, false);
	if ((m_wp + l) <= (m_p + m_l)) {
		if ((m_wp + l) >= m_rp) { return NULL; }
		else if (p) {
			nbr_mem_copy(m_wp, p, l);
		}
		m_wp += l;
	}
	else /* overlap last of m_p */ {
		if ((m_wp + l - m_l) >= m_rp) { return NULL; }
		else if (p) {
			size_t s = (m_l - (m_wp - m_p));
			nbr_mem_copy(m_wp, p, s);
			nbr_mem_copy(m_p, p + s, (l - s));
		}
		m_wp = (m_wp + l - m_l);
	}
	wsz = l;
	return m_wp;
}
inline U8 *
ringbuffer::read(U8 *p, U32 l, U32 &rsz) {
	/* TODO: instead of using lock object,
	 * can I make it faster using gcc4 atomic operations? */
	lock lk(m_lk, false);
	if ((m_rp + l) <= (m_p + m_l)) {
		if ((m_rp + l) >= m_wp) { return NULL; }
		else if (p) {
			nbr_mem_copy(p, m_rp, l);
		}
		m_rp += l;
	}
	else /* overlap last of m_p */ {
		if ((m_rp + l - m_l) >= m_wp) { return NULL; }
		else if (p) {
			size_t s = (m_l - (m_rp - m_p));
			nbr_mem_copy(p, m_rp, s);
			nbr_mem_copy(p + s, m_p, (l - s));
		}
		m_rp = (m_rp + l - m_l);
	}
	rsz = l;
	return m_wp;
}

inline U8 *
ringbuffer::write_chunk(const U8 *p, U32 l, U32 &wsz) {
	/* TODO: instead of using lock object,
	 * can I make it faster using gcc4 atomic operations? */
	lock lk(m_lk, false);
	U32 al = l + sizeof(U16);
	if ((m_wp + al) <= (m_p + m_l)) {
		if ((m_wp + al) >= m_rp) { return NULL; }
		else if (p) {
			SET_16(m_wp, l);
			nbr_mem_copy(m_wp + sizeof(U16), p, l);
		}
		m_wp += al;
	}
	else /* overlap last of m_p */ {
		if ((m_wp + al - m_l) >= m_rp) { return NULL; }
		else if (p) {
			size_t s = (m_l - (m_wp - m_p));
			switch(s) {
			case 0:
				SET_16(m_p, l);
				nbr_mem_copy(m_p + sizeof(U16), p, l);
				break;
			case 1:
				SET_8(m_wp, l & 0x00FF);
				SET_8(m_p, ((l & 0xFF00)>>16));
				nbr_mem_copy(m_p + sizeof(U8), p, l);
				break;
			default:
				SET_16(m_wp, l);
				nbr_mem_copy(m_wp + sizeof(U16), p, s);
				nbr_mem_copy(m_p, p + s, (l - s));
				break;
			}
		}
		m_wp = (m_wp + al - m_l);
	}
	wsz = al;
	return m_wp;
}
inline U8 *
ringbuffer::read_chunk(U8 *p, U32 l, U32 &rsz) {
	/* TODO: instead of using lock object,
	 * can I make it faster using gcc4 atomic operations? */
	lock lk(m_lk, true);
	size_t s = (m_l - (m_rp - m_p));
	U32 r, t;
	switch(s) {
	case 0:
		r = GET_16(m_p);
		if (l < r) { return NULL; }
		if (p) { nbr_mem_copy(p, m_p + sizeof(U16), r); }
		m_rp = m_p + r + sizeof(U16);
		break;
	case 1:
		r = GET_8(m_rp);
		t = GET_8(m_p);
		r |= t;
		if (l < r) { return NULL; }
		if (p) { nbr_mem_copy(p, m_p + sizeof(U8), r); }
		m_rp = m_p + r + sizeof(U8);
		break;
	default:
		r = GET_16(m_rp);
		if (l < r) { return NULL; }
		if ((m_rp + r) <= (m_p + m_l)) {
			if (p) { nbr_mem_copy(p, m_rp + sizeof(U16), r); }
			m_rp += (r + sizeof(U16));
			break;
		}
		else {
			if (p) {
				nbr_mem_copy(p, m_rp + sizeof(U16), s);
				nbr_mem_copy(p + s, m_p, (r - s));
			}
			m_rp = m_p + (r - s);
		}
		break;
	}
	rsz = r + sizeof(U16);
	return m_rp;
}



/*-------------------------------------------------------------*/
/* sfc::util::array											   */
/*-------------------------------------------------------------*/
template<class E>
array<E>::array()
{
	m_a = NULL;
}

template<class E>
array<E>::~array()
{
	fin();
}

template<class E> bool
array<E>::init(int max, int size/* = -1 */,
		int opt/* = NBR_PRIM_EXPANDABLE */)
{
	if (size == -1) {
		size = sizeof(element);	/* default */
	}
	if (!(m_a = nbr_array_create(max, size, opt))) {
		fin();
	}
	return (m_a != NULL);
}

template<class E> void
array<E>::fin()
{
	if (m_a) {
		void *p;
		ARRAY_SCAN(m_a, p) {
			/* call destructor */
			((element *)p)->fin();
		}
		nbr_array_destroy(m_a);
		m_a = NULL;
	}
}

template<class E> int
array<E>::use() const
{
	ASSERT(m_a >= 0);
	return nbr_array_use(m_a);
}

template<class E> int
array<E>::max() const
{
	ASSERT(m_a >= 0);
	return nbr_array_max(m_a);
}

template<class E> int
array<E>::size() const
{
	ASSERT(m_a >= 0);
	return nbr_array_get_size(m_a);
}

template<class E> typename array<E>::iterator
array<E>::insert(value v)
{
	ASSERT(m_a);
	element *e = alloc();
	if (!e) {
		ASSERT(false);
		return NULL;
	}
	e->set(v);
	return e;
}

template<class E> typename array<E>::element*
array<E>::alloc()
{
	if (nbr_array_full(m_a)) { return NULL; }
	return new(m_a)	element;
}

template<class E> typename array<E>::retval*
array<E>::create()
{
	element *e = alloc();
	return e ? e->get() : NULL;
}

template<class E> void
array<E>::destroy(retval *v)
{
	element *e = element::to_e(v);
	ASSERT(nbr_array_get_index(get_a(), e) < nbr_array_max(get_a()));
	if (e) { nbr_array_free(m_a, e); }
}

template<class E> void
array<E>::erase(iterator p)
{
	if (p != end()) {
		p.m_e->fin();	/* call destructer only */
		nbr_array_free(m_a, p.m_e);	/* free its memory */
	}
	return;
}

template<class E> typename array<E>::iterator
array<E>::begin() const
{
	ASSERT(m_a);
	return iterator((element *)nbr_array_get_first(m_a));
}

template<class E> typename array<E>::iterator
array<E>::end() const
{
	return iterator();
}

template<class E> typename array<E>::iterator
array<E>::next(iterator p) const
{
	ASSERT(m_a);
	return iterator((element *)nbr_array_get_next(m_a, p.m_e));
}


/*-------------------------------------------------------------*/
/* sfc::util::map											   */
/*-------------------------------------------------------------*/
template<class V, typename K>
map<V,K>::map()
: array<V>(), m_s(NULL), m_lk(NULL)
{}

template<class V, typename K>
map<V,K>::~map()
{
	fin();
}

template<class V, typename K> bool
map<V,K>::init(int max, int hashsz, int size/* = -1 */,
				int opt/* = opt_expandable */)
{
	if (array<V>::init(max, size, opt)) {
		m_s = kcont<V,K>::init(max, opt & (~(opt_threadsafe)), hashsz);
	}
	if (!m_s) {
		fin();
	}
	if (opt_threadsafe & opt) {
		m_lk = nbr_rwlock_create();
		if (!m_lk) { fin(); }
	}
	return super::m_a && m_s;
}

template<class V, typename K> void
map<V,K>::fin()
{
	if (m_s) {
		nbr_search_destroy(m_s);
		m_s = NULL;
	}
	if (m_lk) {
		nbr_rwlock_destroy(m_lk);
		m_lk = NULL;
	}
	array<V>::fin();
}

template<class V, typename K> typename map<V,K>::element *
map<V,K>::findelem(key k) const
{
	ASSERT(m_s);
	if (m_lk) { nbr_rwlock_rdlock(m_lk); }
	element *e = kcont<V,K>::get(m_s, k);
	if (m_lk) { nbr_rwlock_unlock(m_lk); }
	return e;
}

template<class V, typename K> typename map<V,K>::retval *
map<V,K>::find(key k) const
{
	element *e = findelem(k);
	return e ? e->get() : NULL;
}

template<class V, typename K> typename map<V,K>::iterator
map<V,K>::insert(value v, key k)
{
	element *e = alloc(k);
	if (!e) { return NULL; }
	e->set(v);
	return iterator(e);
}

template<class V, typename K> typename map<V,K>::retval	*
map<V,K>::create(key k)
{
	element *a = alloc(k);
	if (!a) {
		return NULL;
	}
	return a->get();
}

template<class V, typename K> typename map<V,K>::element *
map<V,K>::alloc(key k)
{
	element *e = findelem(k);
	if (e) {	/* already exist */
		return e;
	}
	return rawalloc(k);
}
template<class V, typename K> typename map<V,K>::element *
map<V,K>::rawalloc(key k)
{
	if (nbr_array_full(super::m_a)) {
		return NULL;	/* no mem */
	}
	if (m_lk) { nbr_rwlock_wrlock(m_lk); }
	element *a = new(super::m_a) element;
	if (!a) { goto end; }
	int r;
	if ((r = kcont<V,K>::regist(m_s, k, a)) < 0) {
		erase(k);
		a = NULL;
		goto end;
	}
end:
	if (m_lk) { nbr_rwlock_unlock(m_lk); }
	return a;
}

template<class V, typename K> void
map<V,K>::erase(key k)
{
	ASSERT(m_s && super::m_a);
	if (m_lk) { nbr_rwlock_wrlock(m_lk); }
	element	*e = kcont<V,K>::get(m_s, k);
	if (e) { super::erase(e); }
	else { TRACE("key not found\n"); }
	kcont<V,K>::unregist(m_s, k);
	if (m_lk) { nbr_rwlock_unlock(m_lk); }
}
