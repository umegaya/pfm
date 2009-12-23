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
	return new(m_a)	element;
}

template<class E> typename array<E>::retval*
array<E>::create()
{
	element *e = alloc();
	return e ? e->get() : NULL;
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
		m_s = kcont<V,K>::init(max, opt, hashsz);
	}
	if (!m_s) {
		fin();
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
	array<V>::fin();
}

template<class V, typename K> typename map<V,K>::element *
map<V,K>::findelem(key k) const
{
	ASSERT(m_s);
	return kcont<V,K>::get(m_s, k);
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


template<class V, typename K> typename map<V,K>::element *
map<V,K>::alloc(key k)
{
	element *e = findelem(k);
	if (e) {	/* already exist */
		return e;
	}
	if (array<V>::use() >= array<V>::max()) {
		return NULL;	/* no mem */
	}
	element *a = new(super::m_a) element;
	if (!a) { return NULL; }
	int r;
	if ((r = kcont<V,K>::regist(m_s, k, a)) < 0) {
		erase(k);
		return NULL;
	}
	return a;
}

template<class V, typename K> void
map<V,K>::erase(key k)
{
	ASSERT(m_s && super::m_a);
	element	*e = findelem(k);
	if (!e) {
		TRACE("key not found\n");
	}
	kcont<V,K>::unregist(m_s, k);
	super::erase(e);
}
