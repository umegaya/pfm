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
/* sfc::base												   */
/*-------------------------------------------------------------*/
/* factory_impl */
template <class S, class P>
int factory_impl<S,P>::init(const config &cfg)
{
	if (!init_pool(cfg)) {
		return NBR_ESHORT;
	}
	return factory::init(cfg,
			factory_impl<S,P>::on_open,
			factory_impl<S,P>::on_close,
			factory_impl<S,P>::on_recv,
			factory_impl<S,P>::on_event,
			NULL, /* use default */
			factory_impl<S,P>::on_connect,
			factory_impl<S,P>::on_poll);
}

template <class S, class P>
void factory_impl<S,P>::fin()
{
	m_skm = NULL;
	m_container.fin();
}

template <class S, class P>
int factory_impl<S,P>::broadcast(char *p, int l)
{
	int r;
	iterator it = pool().begin();
	for (;it != pool().end(); it = pool().next(it)) {
		if (!it->valid()) {
			continue;
		}
		if ((r = it->send(p, l)) < 0) {
			return r;
		}
	}
	return NBR_OK;
}

template <class S, class P>
bool factory_impl<S,P>::checkping(class session &s, UTIME ut)
{
	if (!cfg().client()) { return true; }
	UTIME intv = (ut - s.last_ping());
//	log(kernel::INFO, "intv=%llu,ut=%llu,cfg=%llu\n",
//			intv, ut, cfg().m_ping_intv * 1000 * 1000);
	if (intv > cfg().m_ping_intv) {
		int r;
		if ((r = S::sendping(s, ut)) < 0) {
			ASSERT(false);
			return false;
		}
		s.update_ping(ut);
	}
	return intv < cfg().m_ping_timeo;
}


template <class S, class P>
void factory_impl<S,P>::poll(UTIME ut)
{
	iterator p = pool().begin(), tmp;
	for (;p != pool().end();) {
		tmp = p;
		p = pool().next(p);
		if (!tmp->valid()) {
			if (cfg().client()) {
				/* no-recconection or poll failure */
				if (tmp->poll(ut, false) < 0 || tmp->cfg().m_ld_wait <= 0) {
					goto finalize_session;
				}
				else if ((ut - tmp->last_access()) > tmp->cfg().m_ld_wait) {
					S &rs = *tmp;
					if (connect(&rs, NULL, tmp->cfg().proto_p()) < 0) {
						log(ERROR, "reconnection fails! (%d)\n", rs.conn_retry());
						rs.incr_conn_retry();
						continue;
					}
					else {
						ASSERT(tmp->valid());
						tmp->update_access();
					}
				}
				continue;
			}
			else if (tmp->poll(ut, false) > 0) {
				continue;
			}
finalize_session:
			m_container.erase(tmp);
		}
	}
}

template <class S, class P>
int factory_impl<S,P>::on_open(SOCK sk)
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
	obj->clear_conn_retry();
	obj->update_ping(nbr_clock());
	obj->setstate(session::ss_connected);
	return NBR_OK;
}

template <class S, class P>
int factory_impl<S,P>::on_close(SOCK sk, int reason)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	S *obj = *s;
	obj->on_close(reason);
	*s = NULL;
	obj->clear_sock();
	obj->setstate(session::ss_closed);
	return NBR_OK;

}

template <class S, class P>
int factory_impl<S,P>::on_connect(SOCK sk, void *p)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	factory_impl<S,P> *f =
		(factory_impl<S,P> *)nbr_sockmgr_get_data(nbr_sock_get_mgr(sk));
	if (s == NULL || f == NULL) {
		return NBR_ENOTFOUND;
	}
	*s = (S *)(p ? p : f->create(sk));
	if (!(*s)) {
		return NBR_EEXPIRE;
	}
	(*s)->setstate(session::ss_connecting);
	(*s)->set(sk, f);
	(*s)->setaddr();
	return NBR_OK;
}

template <class S, class P>
int factory_impl<S,P>::on_recv(SOCK sk, char *p, int l)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	S *obj = *s;
	int r = S::recvping((**s), p, l);
	if (r <= 0) {
		return r;	/* invalid ping result received */
	}
	obj->update_access();
	return obj->on_recv(p, l);
}

template <class S, class P>
int factory_impl<S,P>::on_event(SOCK sk, char *p, int l)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	return (*s)->on_event(p, l);
}

template <class S, class P>
UTIME factory_impl<S,P>::on_poll(SOCK sk)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return 0LL;
	}
	S *obj = *s;
	UTIME ut = nbr_clock();
	factory_impl<S,P> *fc = (factory_impl<S,P> *)obj->f();
	if (fc->cfg().m_ping_timeo > 0 && !fc->checkping(*obj, ut)) {
		obj->log(ERROR, "pingtimeout: %u\n",fc->cfg().m_ping_timeo);
		obj->close();
		ASSERT(false);
		return ut + obj->cfg().m_taskspan;
	}
	obj->poll(ut, true);
	return ut + obj->cfg().m_taskspan;
}
