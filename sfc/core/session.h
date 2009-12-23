/***************************************************************
 * session.h : template implementation part of sfc::session
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
/* sfc::session												   */
/*-------------------------------------------------------------*/
inline const char *
session::remoteaddr(char *b, int bl) const
{
	return nbr_sock_get_addr(m_sk, b, bl);
}

template <class S>
int session::factory_impl<S>::init(const config &cfg)
{
	if (!m_pool.init(cfg.m_max_connection, sizeof(S), cfg.m_option)) {
		return NBR_ESHORT;
	}
	return session::factory::init(cfg,
			session::factory_impl<S>::on_open,
			session::factory_impl<S>::on_close,
			session::factory_impl<S>::on_recv,
			session::factory_impl<S>::on_event);
}

template <class S>
void session::factory_impl<S>::fin()
{
	m_skm = NULL;
	m_pool.fin();
}

template <class S>
void session::factory_impl<S>::poll(UTIME ut)
{
	typename array<S>::iterator p = pool().begin();
	for (;p != pool().end();) { p->poll(ut); }
}

template <class S>
int session::factory_impl<S>::on_open(SOCK sk)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL), *obj;
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	factory_impl<S> *f =
		(factory_impl<S> *)nbr_sockmgr_get_data(nbr_sock_get_mgr(sk));
	obj = f->pool().alloc();
	obj->set(sk, f);
	int r = NBR_EEXPIRE;
	if (!obj || (r = obj->on_open(f->cfg())) < 0) {
		return r;
	}
	*s = obj;
	return NBR_OK;
}

template <class S>
int session::factory_impl<S>::on_close(SOCK sk, int reason)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	(*s)->on_close(reason);
	(*s)->clear_sock();
	return NBR_OK;

}

template <class S>
int session::factory_impl<S>::on_recv(SOCK sk, char *p, int l)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	if ((*s)->ping().recv((**s), p, l) < 0) {
		return NBR_EINVAL;	/* invalid ping result received */
	}
	(*s)->update_access();
	return (*s)->on_recv(p, l);
}

template <class S>
int session::factory_impl<S>::on_event(SOCK sk, char *p, int l)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	return (*s)->on_event(p, l);
}

