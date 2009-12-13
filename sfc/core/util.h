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

template<class E> int
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
			((element *)p)->~E();
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
		ASSERT(FALSE);
		return NULL;
	}
	e->set(v);
	return e;
}

template<class E> typename array<E>::element*
array<E>::alloc()
{
	return new(m_a)	element;
}

template<class E> typename array<E>::retval
array<E>::create()
{
	element *e = alloc();
	return e ? e->get() : NULL;
}

template<class E> void
array<E>::erase(iterator p)
{
	if (p != end()) {
		((value *)p)->~E();	/* call destructer only */
		nbr_array_free(m_a, p);	/* free its memory */
	}
	return;
}

template<class E> typename array<E>::iterator
array<E>::begin() const
{
	ASSERT(m_a);
	return (iterator)nbr_array_get_first(m_a);
}

template<class E> typename array<E>::iterator
array<E>::end() const
{
	return (iterator)NULL;
}

template<class E> typename array<E>::iterator
array<E>::next(iterator p) const
{
	ASSERT(m_a);
	return (iterator)nbr_array_get_next(m_a, p);
}


/*-------------------------------------------------------------*/
/* sfc::util::map											   */
/*-------------------------------------------------------------*/
template<class V, typename K>
map<V,K>::map()
: array<V>()
{
	m_s = NULL;
}

template<class V, typename K>
map<V,K>::~map()
{
	fin();
}

template<class V, typename K> int
map<V,K>::init(int max, int hashsz, int size/* = -1 */,
				int opt/* = NBR_PRIM_EXPANDABLE */)
{
	if (array<V>::init(max, size, opt)) {
		switch(kcont<K>::kind) {
		case KT_NORMAL:
			m_s = nbr_search_init_mem_engine(max, opt, hashsz, kcont<K>::ksz);
			break;
		case KT_PTR:
			m_s = nbr_search_init_mem_engine(max, opt, hashsz, kcont<K>::ksz);
			break;
		case KT_INT:
			m_s = nbr_search_init_int_engine(max, opt, hashsz);
			break;
		default:
			break;
		}
	}
	if (!m_s) {
		fin();
	}
	return m_a && m_s;
}

template<class V, typename K> void
map<V,K>::fin()
{
	if (m_s) {
		nbr_search_destroy(m_s);
		m_s = NULL;
	}
	array<V>::fin();
}

template<class V, typename K> typename map<V,K>::element *
map<V,K>::find(key k) const
{
	ASSERT(m_s);
	switch(kcont<K>::kind) {
	case KT_NORMAL:
		return (element *)nbr_search_mem_get(m_s, (char *)&k, kcont<K>::ksz);
	case KT_PTR:
		return (element *)nbr_search_mem_get(m_s, (char *)k, kcont<K>::ksz);
	case KT_INT:
		return (element *)nbr_search_int_get(m_s, (int)k);
	default:
		ASSERT(FALSE);
		return end();
	}
}

template<class V, typename K> typename map<V,K>::retval *
map<V,K>::find(key k) const
{
	element *e = find(k);
	return e ? e->get() : NULL;
}

template<class V, typename K> typename map<V,K>::iterator
map<V,K>::insert(value v, key k)
{
	element *e = alloc(k);
	if (!e) { return NULL; }
	e->set(v);
	return e;
}


template<class V, typename K> typename map<V,K>::element *
map<V,K>::alloc(key k)
{
	element *e = find(k);
	if (e) {	/* already exist */
		return e;
	}
	if (array<V>::use() >= array<V>::max()) {
		return NULL;	/* no mem */
	}
	element *a = new(m_a) element;
	if (!a) { return NULL; }
	int r;
	switch(kcont<K>::kind) {
	case KT_NORMAL:
		r = nbr_search_mem_regist(m_s, (char *)&k, kcont<K>::ksz, a); break;
	case KT_PTR:
		r = nbr_search_mem_regist(m_s, (char *)k, kcont<K>::ksz, a); break;
	case KT_INT:
		r = nbr_search_int_regist(m_s, k, a); break;
	default:
		r = -1; ASSERT(FALSE); break;
	}
	if (r < 0) {
		erase(k);
		return NULL;
	}
	return a;
}

template<class V, typename K> void
map<V,K>::erase(key k)
{
	ASSERT(m_s && m_a);
	element	*e = find(k);
	if (!e) {
		TRACE("key not found\n");
	}
	int r;
	switch(kcont<K>::kind) {
	case KT_NORMAL:
		r = nbr_search_mem_unregist(m_s, (char *)&k, kcont<K>::ksz); break;
	case KT_PTR:
		r = nbr_search_mem_unregist(m_s, (char *)k, kcont<K>::ksz); break;
	case KT_INT:
		r = nbr_search_int_unregist(m_s, k); break;
	default:
		r = -1; break;
	}
	if (r < 0) { ASSERT(FALSE); }
	array<E>::erase(e);
}
