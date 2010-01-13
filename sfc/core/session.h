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
			session::factory_impl<S>::on_event,
			session::factory_impl<S>::on_connect,
			session::factory_impl<S>::on_poll);
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
	typename array<S>::iterator p = pool().begin(), tmp;
	for (;p != pool().end();) {
		tmp = p;
		p = pool().next(p);
		if (!tmp->valid()) {
			int r;
			if ((r = tmp->poll(ut, false)) < 0) {}
			else if (cfg().client()) {
				if (tmp->cfg().m_ld_wait <= 0) {
					r = NBR_EEXPIRE;
				}
				else if ((ut - tmp->last_access()) > tmp->cfg().m_ld_wait) {
					S &rs = *tmp;
					if (connect(&rs, NULL, tmp->cfg().proto_p()) < 0) {
						rs.incr_conn_failure();
						continue;
					}
					else {
						tmp->update_access();
					}
				}
			}
			if (r != NBR_OK) {
				tmp->fin();
				pool().erase(tmp);
			}
		}
	}
}

template <class S>
int session::factory_impl<S>::on_open(SOCK sk)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL), *obj;
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	obj = *s;
	int r;
	if ((r = obj->on_open(obj->cfg())) < 0) {
		return r;
	}
	obj->clear_conn_failure();
	obj->setattr(session::attr_ping_fail, false);
	return NBR_OK;
}

template <class S>
int session::factory_impl<S>::on_close(SOCK sk, int reason)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	S *obj = *s;
	obj->on_close(reason);
	obj->setaddr();
	obj->clear_sock();
	obj->setattr(session::attr_opened, false);
	*s = NULL;
	return NBR_OK;

}

template <class S>
int session::factory_impl<S>::on_connect(SOCK sk, void *p)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	session::factory_impl<S> *f =
		(session::factory_impl<S> *)nbr_sockmgr_get_data(nbr_sock_get_mgr(sk));
	if (s == NULL || f == NULL) {
		return NBR_ENOTFOUND;
	}
	*s = (S *)(p ? p : f->pool().alloc());
	if (!(*s)) {
		return NBR_EEXPIRE;
	}
	(*s)->setattr(session::attr_opened, true);
	(*s)->set(sk, f);
	return NBR_OK;
}

template <class S>
int session::factory_impl<S>::on_recv(SOCK sk, char *p, int l)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	S *obj = *s;
	if (obj->ping().recv((**s), p, l) < 0) {
		obj->setattr(session::attr_ping_fail, true);
		return NBR_EINVAL;	/* invalid ping result received */
	}
	obj->update_access();
	return obj->on_recv(p, l);
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

template <class S>
void session::factory_impl<S>::on_poll(SOCK sk)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return;
	}
	S *obj = *s;
	UTIME ut = nbr_clock();
	if (obj->cfg().m_ping_timeo > 0 && !obj->ping().validate(*obj, ut)) {
		obj->close();
		return;
	}
	obj->poll(ut, true);
}

