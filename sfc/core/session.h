/*-------------------------------------------------------------*/
/* sfc::session												   */
/*-------------------------------------------------------------*/
template <class S>
int session::factory_impl<S>::init(const config &cfg)
{
	m_cfg = cfg;
	if (!(m_skm = nbr_sockmgr_create(cfg.m_rbuf, cfg.m_wbuf,
							cfg.m_max_connection,
							sizeof(S*),
							cfg.m_timeout,
							cfg.m_host,
							cfg.m_proto,
							cfg.proto_p(),
							cfg.m_option))) {
		return NBR_ESOCKET;
	}
	nbr_sockmgr_set_data(m_skm, this);
	nbr_sockmgr_set_callback(m_skm,
			cfg.m_parser,
			on_open, on_close, on_recv, on_event);
	m_sender = cfg.m_sender;
	if (!m_pool.init(cfg.m_max_connection, -1, cfg.m_option)) {
		return NBR_ESHORT;
	}
	return NBR_OK;
}

template <class S>
void session::factory_impl<S>::fin()
{
	m_skm = NULL;
	m_pool.fin();
}

template <class S>
void session::factory_impl<S>::poll()
{
	array<S>::iterator p = m_pool.begin(), tmp;
	for (;p != m_pool.end();) {
		tmp = p;
		p = m_pool.next(p);
		if (tmp->poll() < 0) {
			tmp->fin();
			m_pool.erase(tmp);
		}
	}
}

template <class S>
int session::factory_impl<S>::on_open(SOCK sk)
{
	int r;
	S **p = nbr_sock_get_data(sk), *obj;
	if (p == NULL) {
		return NBR_ENOTFOUND;
	}
	factory *f = nbr_sockmgr_get_data(nbr_sock_get_mgr(sk));
	obj = m_pool.alloc();
	obj->set(sk, f);
	if (!obj || obj->on_open(f->cfg()) < 0) {
		return NBR_EEXPIRE;
	}
	*p = obj;
	return NBR_OK;
}

template <class S>
int session::factory_impl<S>::on_close(SOCK sk, int reason)
{
	S **p = nbr_sock_get_data(sk);
	if (p == NULL) {
		return NBR_ENOTFOUND;
	}
	(*p)->on_close(reason);
	(*p)->clear_sock();
	return NBR_OK;

}

template <class S>
int session::factory_impl<S>::on_recv(SOCK sk, char *p, int l)
{
	S **p = nbr_sock_get_data(sk);
	if (p == NULL) {
		return NBR_ENOTFOUND;
	}
	if ((*p)->ping().recv((**p), p, l) < 0) {
		return NBR_EINVAL;	/* invalid ping result received */
	}
	(*p)->update_access();
	return (*p)->on_recv(p, l);
}

template <class S>
int session::factory_impl<S>::on_event(SOCK sk, char *p, int l)
{
	S **p = nbr_sock_get_data(sk);
	if (p == NULL) {
		return NBR_ENOTFOUND;
	}
	return (*p)->on_event(p, l);
}

