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
	public:
		enum state { /* http fsm state */
			/* send state */
			state_send_body,
			state_send_finish,
			/* recv state */
			state_recv_header,
			state_recv_body,
			state_recv_body_nochunk,
			state_recv_bodylen,
			state_recv_footer,
			state_recv_comment,
			state_recv_finish,
			state_recv_processed,
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
			U16		state, res;
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
		state	status() const { return (state)m_ctx.state; }
		void	setprocessed() { m_ctx.state = state_recv_processed; }
	public:	/* for processing reply */
		int			version() const { return m_ctx.version; }
		char 		*hdrstr(const char *key, char *b, int l) const;
		int 		hdrint(const char *key, int &out) const;
		const char 	*body() const { return m_ctx.bd; }
		int			bodylen() const { return m_ctx.bl; }
	public:	/* for sending */
		void		set_send_body(const char *p, int l) {
			m_buf = p; m_len = l;
			m_ctx.state = state_send_body;
		}
		const char	*send_body_ptr() { return m_buf; }
		int			send_body_len() const { return m_len; }
		int			bodysent(int len);
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
	} m_fsm;
	typedef fsm request, response;
public:
	httpsession() : session(), m_fsm() {}
	~httpsession() {}
	class fsm &fsm() { return m_fsm; }
public:
	void fin();
	void poll(UTIME ut);
	int on_open(const config &cfg);
	int on_close(int r);
public:	/* callback */
	int process(class fsm &r);
	int send_request();
public:	/* operation */
	int	get(const char *url, char *hd[], int hl[], int n_hd);
	int	post(const char *url, const char *body, int blen,
			char *hd[], int hl[], int n_hd);
	int send_result_code(httprc rc, int cf/* close after sent? */);
	int send_result_and_body(httprc rc,
			char *b, int bl, const char *mime);
protected:
	int send_body();
	int send_header(char *hd[], int hl[], int n_hd);
};

/*-------------------------------------------------------------*/
/* sfc::httpfactory											   */
/*-------------------------------------------------------------*/
template <class S>
class httpfactory : public session::factory_impl<S>
{
public:
	typedef session::factory_impl<S> super;
public:
	httpfactory() : super() {}
	~httpfactory() {}
	int init(const config &cfg);
	static int on_open(SOCK sk);
	static int on_close(SOCK sk, int r);
	static int on_recv(SOCK sk, char *p, int l);
};

template <class S> int
httpfactory<S>::init(const config &cfg)
{
	if (!super::pool().init(
		cfg.m_max_connection,
		sizeof(S) + cfg.m_rbuf, cfg.m_option)) {
		return NBR_ESHORT;
	}
	return session::factory::init(cfg,
			httpfactory<S>::on_open,
			httpfactory<S>::on_close,
			httpfactory<S>::on_recv,
			httpfactory<S>::on_event);
}

template <class S>
int httpfactory<S>::on_recv(SOCK sk, char *p, int l)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	S *obj = *s;
	obj->update_access();
	switch(obj->fsm().append(p, l)) {
	case S::fsm::state_recv_finish:
	case S::fsm::state_error:
		obj->process(obj->fsm());
		obj->fsm().setprocessed();
		break;
	default:
		break;
	}
	return NBR_OK;
}


template <class S>
int httpfactory<S>::on_close(SOCK sk, int reason)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL);
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	S *obj = *s;
	if (obj->fsm().status() == S::fsm::state_recv_finish) {
		obj->process(obj->fsm());
		obj->fsm().setprocessed();
	}
	obj->on_close(reason);
	obj->clear_sock();
	return NBR_OK;

}

template <class S>
int httpfactory<S>::on_open(SOCK sk)
{
	S **s = (S**)nbr_sock_get_data(sk, NULL), *obj;
	if (s == NULL) {
		return NBR_ENOTFOUND;
	}
	super *f = (super *)nbr_sockmgr_get_data(nbr_sock_get_mgr(sk));
	obj = f->pool().alloc();
	obj->set(sk, f);
	int r = NBR_EEXPIRE;
	if (!obj || (r = obj->on_open(f->cfg())) < 0) {
		return r;
	}
	*s = obj;
	obj->fsm().reset(f->cfg());
	if (f->cfg().client()) {
		return obj->send_request();
	}
	return NBR_OK;
}

}

#endif//__HTTP_H__
