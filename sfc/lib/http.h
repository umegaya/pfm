/***************************************************************
 * http.h : http implementaion (one example of using sfc)
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
#if !defined(__HTTP_H__)
#define __HTTP_H__

#include "sfc.hpp"

namespace sfc {
#include "httprc.h"
/*-------------------------------------------------------------*/
/* sfc::httpsession											   */
/*-------------------------------------------------------------*/
class httpsession : public session
{
public:
	enum method {
		method_get,
		method_post,
	};
	enum version {
		version_1_0 = 10,
		version_1_1 = 11,
	};
	class fsm {
	friend class httpsession;
	public:
		enum state { /* http fsm state */
			state_invalid,
			/* send state */
			state_send_body,		/* client */
			state_resp_send_body,	/* server */
			/* recv state */
			state_recv_header,
			state_recv_body,
			state_recv_body_nochunk,
			state_recv_bodylen,
			state_recv_footer,
			state_recv_comment,
			state_recv_finish,
			/* error */
			state_error = -1,
		};
		static const U16 lflf = 0x0a0a;
		static const U16 crlf = 0x0d0a;
		static const U32 crlfcrlf = 0x0d0a0d0a;
		static const int MAX_HEADER = 64;
	protected:
		struct context {
			U8		method, version, n_hd, padd;
			S16		state, res;
			const char	*hd[MAX_HEADER], *bd;
			U32		bl;
			U16		hl[MAX_HEADER];
		}	m_ctx;
		U32 m_len, m_max;
		const char *m_buf;
		char m_p[0];
	public:
		fsm() {}
		~fsm() {}
		state 	append(char *b, int bl);
		void 	reset(const config &cfg);
	public:
		state	get_state() const { return (state)m_ctx.state; }
		bool	error() const { return get_state() == state_error; }
		void	setrc(httprc rc) { m_ctx.res = (S16)rc; }
		void	setrc_from_close_reason(int reason);
	public:	/* for processing reply */
		int			version() const { return m_ctx.version; }
		char 		*hdrstr(const char *key, char *b, int l) const;
		int 		hdrint(const char *key, int &out) const;
		const char 	*body() const { return m_ctx.bd; }
		httprc		rc() const { return (httprc)m_ctx.res; }
		int			bodylen() const { return m_ctx.bl; }
		int			url(char *b, int l);
	protected:	/* for sending */
		void		set_send_body(const char *p, int l, bool resp = false) {
			m_buf = p; m_len = l;
			m_ctx.state = resp ? state_resp_send_body : state_send_body;
		}
		bool		send_phase() const {
			state s = get_state();
			return s == state_send_body || s == state_resp_send_body;
		}
		const char	*send_body_ptr() { return m_buf; }
		int			send_body_len() const { return m_len; }
		int			body_sent(int len);
	protected: /* receiving */
		state 	recv_header();
		state	recv_body_nochunk();
		state 	recv_body();
		state 	recv_bodylen();
		state 	recv_footer();
		state	recv_comment();
	protected:
		int		recv_lflf() const;
		int		recv_lf() const;
		char 	*current() { return m_p + m_len; }
		const char *current() const { return m_p + m_len; }
		context	&recvctx() { return m_ctx; }
		context &sendctx() { return m_ctx; }
		httprc	putrc();
	} *m_fsm;
	typedef fsm request, response;
public:
	template <class S>
	class factory : public session::factory_impl< S, arraypool<S> > {
	protected:
		ARRAY m_fsm_a;
	public:
		typedef session::factory_impl<S, arraypool<S> > super;
	public:
		factory() : super(), m_fsm_a() {}
		~factory() {}
		int init(const config &cfg);
		void fin();
		void attach_fsm(httpsession &s) {
			if (!s.has_fsm()) { s.set_fsm(nbr_array_alloc(m_fsm_a)); }
		}
		ARRAY allocator() { return m_fsm_a; }
		static int on_open(SOCK sk);
		static int on_connect(SOCK sk, void *p);
		static int on_recv(SOCK sk, char *p, int l);
	};
public:
	httpsession() : session() {}
	~httpsession() {}
	class fsm &fsm() { return *m_fsm; }
	void set_fsm(void *p) { m_fsm = (class fsm *)p; }
	bool has_fsm() const { return m_fsm != NULL; }
public:
	void fin();
	pollret poll(UTIME ut, bool from_worker);
	int on_open(const config &cfg);
	int on_close(int r);
public:	/* callback */
	int process(class fsm &r) { return NBR_OK; }
	int send_request() { return NBR_OK; }
public:	/* operation */
	char *host(char *buff, int len) const;
	int	get(const char *url, const char *hd[], const char *hv[],
			int n_hd, bool chunked = false);
	int	post(const char *url, const char *hd[], const char *hv[],
			const char *body, int blen, int n_hd, bool chunked = false);
	int send_request_common(const char *method,
			const char *url, const char *body, int blen,
			const char *hd[], const char *hv[], int n_hd, bool chunked);
	int send_result_code(httprc rc, int cf/* close after sent? */);
	int send_result_and_body(httprc rc, const char *b, int bl, const char *mime);
	inline int send_lf() { return send("\r\n", 2); }
protected:
	int send_body();
	int send_header(const char *hd[], const char *hv[], int n_hd, bool chunked);
};

/*-------------------------------------------------------------*/
/* sfc::httpsession::factory											   */
/*-------------------------------------------------------------*/
template <class S> int
httpsession::factory<S>::init(const config &cfg)
{
	if (!super::init_pool(cfg)) {
		return NBR_ESHORT;
	}
	if (!(m_fsm_a = nbr_array_create(cfg.m_max_connection,
			sizeof(class httpsession::fsm) + cfg.m_rbuf,
			cfg.m_option))) {
		return NBR_ESHORT;
	}
	return session::factory::init(cfg,
			httpsession::factory<S>::on_open,
			httpsession::factory<S>::on_close,
			httpsession::factory<S>::on_recv,
			httpsession::factory<S>::on_event,
			NULL, /* use default */
			httpsession::factory<S>::on_connect,
			httpsession::factory<S>::on_poll);
}

template <class S> void
httpsession::factory<S>::fin()
{
	if (m_fsm_a) {
		nbr_array_destroy(m_fsm_a);
	}
	super::fin();
}

template <class S> int
httpsession::factory<S>::on_recv(SOCK sk, char *p, int l)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		ASSERT(false);
		return NBR_ENOTFOUND;
	}
	S *obj = *s;
	obj->update_access();
	switch(obj->fsm().append(p, l)) {
	case S::fsm::state_recv_finish:
	case S::fsm::state_error:
		obj->process(obj->fsm());
		break;
	default:
		break;
	}
	return NBR_OK;
}

template <class S> int
httpsession::factory<S>::on_connect(SOCK sk, void *p)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	httpsession::factory<S> *f =
		(httpsession::factory<S> *)nbr_sockmgr_get_data(nbr_sock_get_mgr(sk));
	if (s == NULL || f == NULL) {
		ASSERT(false);
		return NBR_ENOTFOUND;
	}
	*s = (S *)(p ? p : f->pool().alloc());
	if (!(*s)) {
		ASSERT(false);
		return NBR_EEXPIRE;
	}
	(*s)->setstate(session::ss_connecting);
	(*s)->set(sk, f);
	(*s)->setaddr();
	f->attach_fsm(**s);
	return NBR_OK;
}


template <class S> int
httpsession::factory<S>::on_open(SOCK sk)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL), *obj;
	if (s == NULL) {
		ASSERT(false);
		return NBR_ENOTFOUND;
	}
	obj = *s;
	int r;
	if ((r = obj->on_open(obj->cfg())) < 0) {
		ASSERT(false);
		return r;
	}
	obj->fsm().reset(obj->cfg());
	obj->setstate(session::ss_connected);
	obj->clear_conn_retry();
	if (obj->cfg().client()) {
		return obj->send_request();
	}
	return NBR_OK;
}

}

#endif//__HTTP_H__
