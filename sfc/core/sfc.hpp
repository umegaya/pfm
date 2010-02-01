/***************************************************************
 * sfc.hpp : common header for using sfc framework
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
#if !defined(__SFC_H__)
#define __SFC_H__

#include "nbr.h"
#include <stdlib.h>

namespace sfc {

#include "defs.h"

namespace util {
/*-------------------------------------------------------------*/
/* sfc::util::address										   */
/*-------------------------------------------------------------*/
class address {
public:
	static const size_t SIZE = 32;
private:
	U8 m_len, padd[3];
	char m_a[SIZE];
public:
	address(const char *p) { from(p); }
	address() : m_len(0) { m_a[0] = '\0'; }
	operator const char*() const { return m_a; }
	char *a() { return m_a; }
	int len() const { return m_len; }
	void setlen(int len) { m_len = (U8)len; }
	int from(const char *a);
	int from(SOCKMGR skm) {
		int r = nbr_sockmgr_get_addr(skm, m_a, SIZE);
		if (r < 0) { return r; }
		else { m_len = (U8)r; }
		return r;
	}
	int from(SOCK sk) {
		int r = nbr_sock_get_addr(sk, m_a, SIZE);
		if (r < 0) { return r; }
		else { m_len = (U8)r; }
		return r;
	}
	/* address for ipv4, ipv6 specific... */
	int addrpart(char *p, int l) {
		const char *b = nbr_str_rchr(m_a, ':', SIZE);
		if (!b) { return NBR_EFORMAT; }
		if ((b - m_a) >= l) { return NBR_ESHORT; }
		nbr_mem_copy(p, m_a, (b - m_a));
		p[(b - m_a)] = '\0';
		return (b - m_a);
	}
};

/*-------------------------------------------------------------*/
/* sfc::util::lock											   */
/*-------------------------------------------------------------*/
class lock {
	RWLOCK m_lk;
	bool m_locked;
public:
	lock(RWLOCK lk, bool rl) : m_lk(lk) {
		int r;
		if (!lk)		{ m_locked = false; }
		else {
			if (rl)	{ r = nbr_rwlock_rdlock(m_lk); }
			else 	{ r = nbr_rwlock_wrlock(m_lk); }
			m_locked = (r >= 0);
		}
	}
	~lock() {
		if (m_locked) { nbr_rwlock_unlock(m_lk); }
	}
	int unlock() {
		if (m_locked) {
			nbr_rwlock_unlock(m_lk);
			m_locked = false;
		}
		return NBR_OK;
	}
};

/*-------------------------------------------------------------*/
/* sfc::util::ringbuffer									   */
/*-------------------------------------------------------------*/
class ringbuffer {
protected:
	U8	*m_p, *m_wp, *m_rp;
	U32	m_l;
	RWLOCK m_lk;
public:
	ringbuffer() : m_l(0), m_lk(NULL) {}
	~ringbuffer() { fin(); };
	void fin();
	bool init(U32 size);
public:
	inline const U8 *rp() const { return m_rp; }
	inline const U8 *wp() const { return m_wp; }
	inline int size() const { return m_l; }
	/* direct read/write */
	inline U8 *write(const U8 *p, U32 l, U32 &wsz);
	inline U8 *read(U8 *p, U32 l, U32 &rsz);
	/* write/read data with length information of data */
	inline U8 *write_chunk(const U8 *p, U32 l, U32 &wsz);
	inline U8 *read_chunk(U8 *p, U32 l, U32 &rsz);
};

/*-------------------------------------------------------------*/
/* sfc::util::array											   */
/*-------------------------------------------------------------*/
enum {
	opt_not_set	   = 0,
	opt_threadsafe = NBR_PRIM_THREADSAFE,
	opt_expandable = NBR_PRIM_EXPANDABLE,
};
template <class E>
class array {
public:
public:
	/* specialization */
	template<typename T>
	struct 	vcont {
		typedef T &value;
		typedef T retval;
		class	element : public T {
		public:
			element() : T() {}
			~element() {}
			void	fin() { delete this; }
			void	*operator	new		(size_t, ARRAY a) {
				return nbr_array_alloc(a);
			}
			void	operator	delete	(void*) {}
			void	set(value v) { ((value)*this) = v; }
			retval	*get() { return this; }
		};
	};
	template<typename T>
	struct 	vcont<T*> {
		typedef T *value;
		typedef T retval;
		class	element {
		public:
			T	*data;
		public:
			element() : data(NULL) {}
			~element() {}
			void	fin() { delete this; }
			void	*operator	new		(size_t, ARRAY a) {
				return nbr_array_alloc(a);
			}
			void	operator	delete	(void*) {}
			void	set(value v) { data = v; }
			retval	*get() { return data; }
		};
	};
	template<typename T, size_t N>
	struct 	vcont<T[N]> {
		typedef const T *value;
		typedef T retval;
		class	element {
		public:
			T	data[N];
		public:
			element() {}
			~element() {}
			void	fin() { delete this; }
			void	*operator	new		(size_t, ARRAY a) {
				return nbr_array_alloc(a);
			}
			void	operator	delete	(void*) {}
			void	set(value v) {
				for (int i = 0; i < N; i++) { data[i] = v[i]; }
			}
			retval	*get() { return data; }
		};
	};
	typedef typename vcont<E>::element	element;
	typedef typename vcont<E>::value 	value;
	typedef typename vcont<E>::retval 	retval;
	class iterator {
	public:
		element *m_e;
	public:
		iterator() : m_e(NULL) {}
		iterator(element *e) : m_e(e) {}
		inline retval	&operator 	* 		() 		{ return *(m_e->get()); }
		inline retval	*operator	->		()		{ return m_e->get(); }
		inline bool	operator == (const iterator &i) const { return m_e == i.m_e; }
		inline bool	operator != (const iterator &i) const { return m_e != i.m_e; }
	};
protected:
	ARRAY	m_a;
public:
	array();
	~array();
	inline bool		init(int max, int size = -1,
						int opt = opt_expandable);
	inline void 	fin();
	inline iterator insert(value v);
	inline retval	*create();
	inline void 	erase(iterator p);
	inline int		size() const;
	inline int		use() const;
	inline int		max() const;
public:
	inline iterator	begin() const;
	inline iterator	end() const;
	inline iterator	next(iterator p) const;
	inline element	*alloc();
	inline bool		initialized() { return m_a != NULL; }
#if defined(_DEBUG)
	ARRAY get_a() { return m_a; }
#endif
private:
	array(const array &a);
};

/*-------------------------------------------------------------*/
/* sfc::util::map											   */
/*-------------------------------------------------------------*/
template <class V, typename K>
class map : public array<V> {
public:
	typedef array<V> super;
	typedef typename super::iterator iterator;
	typedef typename super::value	value;
	typedef typename super::retval	retval;
	typedef typename super::element element;
	/* specialization */
	enum { KT_NORMAL = 0, KT_PTR = 1, KT_INT = 2 };
	template <class C, typename T>
	struct 	kcont {
		typedef const T &type;
		static SEARCH init(int max, int opt, int hashsz) {
			return nbr_search_init_mem_engine(max, opt, hashsz, sizeof(T));
		}
		static int regist(SEARCH s, type t, element *v) {
			return nbr_search_mem_regist(s, &t, sizeof(T), v);
		}
		static void unregist(SEARCH s, type t) {
			nbr_search_mem_unregist(s, &t, sizeof(T));
		}
		static element *get(SEARCH s, type t) {
			return (element *)nbr_search_mem_get(s, &t, sizeof(T));
		}
	};
	template <class C>
	struct 	kcont<C,address> {
		typedef const address &type;
		static SEARCH init(int max, int opt, int hashsz) {
			return nbr_search_init_mem_engine(max, opt, hashsz, address::SIZE);
		}
		static int regist(SEARCH s, type t, element *v) {
			return nbr_search_mem_regist(s, t, t.len(), v);
		}
		static void unregist(SEARCH s, type t) {
			nbr_search_mem_unregist(s, t, t.len());
		}
		static element *get(SEARCH s, type t) {
			return (element *)nbr_search_mem_get(s, t, t.len());
		}
	};
	template <class C, typename T, size_t N>
	struct 	kcont<C,T[N]> {
		typedef const T type[N];
		static SEARCH init(int max, int opt, int hashsz) {
			return nbr_search_init_mem_engine(max, opt, hashsz, sizeof(T) * N);
		}
		static int regist(SEARCH s, type t, element *v) {
			return nbr_search_mem_regist(s, t, sizeof(T) * N, v);
		}
		static void unregist(SEARCH s, type t) {
			nbr_search_mem_unregist(s, t, sizeof(T) * N);
		}
		static element *get(SEARCH s, type t) {
			return (element *)nbr_search_mem_get(s, t, sizeof(T) * N);
		}
	};
	template <class C, typename T>
	struct 	kcont<C,T*> {
		typedef const T *type;
		static SEARCH init(int max, int opt, int hashsz) {
			return nbr_search_init_mem_engine(max, opt, hashsz, sizeof(T));
		}
		static int regist(SEARCH s, type t, element *v) {
			return nbr_search_mem_regist(s, t, sizeof(T), v);
		}
		static void unregist(SEARCH s, type t) {
			nbr_search_mem_unregist(s, t, sizeof(T));
		}
		static element *get(SEARCH s, type t) {
			return (element *)nbr_search_mem_get(s, t, sizeof(T));
		}
	};
	template <class C>
	struct	kcont<C,U32> {
		typedef U32 type;
		static SEARCH init(int max, int opt, int hashsz) {
			return nbr_search_init_int_engine(max, opt, hashsz);
		}
		static int regist(SEARCH s, type t, element *v) {
			return nbr_search_int_regist(s, (int)t, v);
		}
		static void unregist(SEARCH s, type t) {
			nbr_search_int_unregist(s, (int)t);
		}
		static element *get(SEARCH s, type t) {
			return (element *)nbr_search_int_get(s, (int)t);
		}
	};
	template <class C, size_t N>
	struct	kcont<C,char[N]> {
		typedef const char *type;
		static SEARCH init(int max, int opt, int hashsz) {
			return nbr_search_init_str_engine(max, opt, hashsz, N);
		}
		static int regist(SEARCH s, type t, element *v) {
			return nbr_search_str_regist(s, t, v);
		}
		static void unregist(SEARCH s, type t) {
			nbr_search_str_unregist(s, t);
		}
		static element *get(SEARCH s, type t) {
			return (element *)nbr_search_str_get(s, t);
		}
	};
	typedef typename kcont<V,K>::type key;
protected:
	SEARCH	m_s;
	RWLOCK 	m_lk;
public:
	map();
	~map();
	inline bool		init(int max, int hashsz,
						int size = -1,
						int opt = opt_expandable);
	inline void 	fin();
	inline iterator	insert(value v, key k);
	inline retval	*find(key k) const;
	inline retval	*create(key k);
	inline void		erase(key k);
	inline bool		initialized() { return super::initialized() && m_s != NULL; }
#if defined(_DEBUG)
	SEARCH get_s() { return m_s; }
#endif
protected:
	inline element	*findelem(key k) const;
	inline element	*alloc(key k);
private:
	map(const map &m);
};

/*-------------------------------------------------------------*/
/* sfc::kernel												   */
/*-------------------------------------------------------------*/
class kernel {
public:
	/* log level definition */
	typedef enum {
		DEBUG = 1,
		INFO = 2,
		WARN = 3,
		ERROR = 4,
		FATAL = 5,
	} loglevel;
};

/*-------------------------------------------------------------*/
/* sfc::util::config										   */
/*-------------------------------------------------------------*/
class config : public kernel {
public:
	typedef char *(*parser)(char*, int*, int*);
	typedef int (*sender)(SOCK, char *, int);
	static const int MAX_SESSION_NAME = 32;
	static const int MAX_HOST_NAME = 256;
	static const int MAX_VALUE_STR = 256;
	enum {
		cfg_flag_not_set = 0x0,
		cfg_flag_server = 0x00000001,
		cfg_flag_disabled = 0x00000002,
	};
public:
	char m_name[MAX_SESSION_NAME];
	char m_host[MAX_HOST_NAME];
	int m_max_connection;
	int m_timeout, m_option;
	int m_rbuf, m_wbuf;
	int m_ping_timeo, m_ping_intv;
	int m_max_query, m_query_bufsz;
	const char *m_proto_name;
	UTIME m_taskspan, m_ld_wait;
	loglevel m_verbosity;
	parser m_fnp;
	sender m_fns;
	U32 m_flag;
public:
	/* macro for inheritant class */
#define BASE_CONFIG_PLIST				\
	const char name[MAX_SESSION_NAME],	\
	const char host[MAX_HOST_NAME],		\
	int max_connection,					\
	int timeout, int option,			\
	int rbuf, int wbuf,					\
	int ping_timeo, int ping_intv,		\
	int max_query, int query_bufsz,		\
	const char *proto_name,				\
	UTIME taskspan, UTIME ld_wait,		\
	loglevel verbosity,					\
	parser fnp, sender fns, U32 flag
#define BASE_CONFIG_CALL				\
	name, host, max_connection,			\
	timeout, option, rbuf, wbuf, 		\
	ping_timeo, ping_intv,				\
	max_query, query_bufsz,				\
	proto_name, taskspan, ld_wait,		\
	verbosity,							\
	fnp, fns, flag
#define BASE_CONFIG_SETPARAM							\
	m_max_connection(max_connection),					\
	m_timeout(timeout), m_option(option),				\
	m_rbuf(rbuf), m_wbuf(wbuf),							\
	m_ping_timeo(ping_timeo), m_ping_intv(ping_intv),	\
	m_max_query(max_query), m_query_bufsz(query_bufsz),	\
	m_proto_name(proto_name),							\
	m_taskspan(taskspan), m_ld_wait(ld_wait),			\
	m_verbosity(verbosity),								\
	m_fnp(fnp),											\
	m_fns(fns), m_flag(flag)

	config();
	config(BASE_CONFIG_PLIST);
	virtual ~config();
	virtual int str(const char *k, const char *&v) const;
	virtual int	num(const char *k, int &v) const;
	virtual int	bignum(const char *k, U64 &v) const;
	virtual int	set(const char *k, const char *v);
	virtual void *proto_p() const { return NULL; }
	virtual void set(const config &cfg){ *this = cfg; }
public:
	int	load(const char *line);
	void fin() { delete this; }
	bool disabled() const { return (m_flag & cfg_flag_disabled) != 0; }
	bool client() const { return (m_flag & cfg_flag_server) == 0; }
	static int commentline(const char *line) { return (line[0] == '#' ? 1 : 0); }
	static int emptyline(const char *line) { return (line[0] == '\0' ? 1 : 0); }
public:
	static int cmp(const char *a, const char *b);
	static parser rparser_from(const char *str);
	static sender sender_from(const char *str);
};

#include "util.h"
}	//namespace util
using namespace util;



/*-------------------------------------------------------------*/
/* sfc::session												   */
/*-------------------------------------------------------------*/
class session : public kernel {
public:
	class factory : public kernel {
		friend class lock;
		static const U32	MSGID_LIMIT = 2000000000;
		static const U16	MSGID_COMPACT_LIMIT = 60000;
	public:
		struct query {
			char data[0];
		};
		typedef map<query,U32> qmap;
	public:
		const config	*m_cfg;
		SOCKMGR			m_skm;
		UTIME			m_last_poll;
		U32				m_msgid_seed;
		RWLOCK			m_lk;
		qmap			m_ql;
	public:
		factory() : m_cfg(NULL), m_skm(NULL), m_last_poll(0),
					m_msgid_seed(0), m_lk(NULL), m_ql() {}
		virtual ~factory() {}
		virtual int init(const config &cfg) = 0;
		virtual void fin() = 0;
		virtual void poll(UTIME ut) = 0;
		int log(loglevel lv, const char *fmt, ...);
		int base_init();
		void base_fin();
		RWLOCK lk() { return m_lk; }
		const config &cfg() const { return *m_cfg; }
		qmap &querymap() { return m_ql; }
		int connect(session *s, const char *addr = NULL, void *p = NULL);
		int get(address &a) const { return a.from((SOCKMGR)m_skm); }
		int mcast(const char *addr, char *p, int l);
		int event(int t, const char *p, int l) {
			return nbr_sockmgr_event(m_skm, t, (char *)p, l); }
		inline U32 msgid() {
			++m_msgid_seed;
			if (m_msgid_seed > MSGID_LIMIT) { m_msgid_seed = 1; }
			return m_msgid_seed;
		}
		inline U16 compact_msgid() {
			++m_msgid_seed;
			if (m_msgid_seed > MSGID_COMPACT_LIMIT) { m_msgid_seed = 1; }
			return m_msgid_seed;
		}
	protected:
		int init(const config &cfg,
					int (*aw)(SOCK),
					int (*cw)(SOCK, int),
					int (*pp)(SOCK, char*, int),
					int (*eh)(SOCK, char*, int),
					void (*meh)(SOCKMGR, int, char*, int),
					int (*oc)(SOCK, void*),
					UTIME (*poll)(SOCK));
	};
	template <class S>
	class arraypool {
	public:
		typedef array<S> sspool;
		typedef typename sspool::iterator iterator;
	private:
		sspool m_pool;
	public:
		arraypool() : m_pool() {}
		~arraypool() { fin(); }
		sspool &pool() { return m_pool; }
		void erase(iterator it) {
			it->fin();
			m_pool.erase(it);
		}
		S *create(SOCK sk) {
			return m_pool.create();
		}
		bool init(const config &cfg) {
			return m_pool.init(cfg.m_max_connection,
				sizeof(S), cfg.m_option);
		}
		void fin() { m_pool.fin(); }
	};
	template <class S>
	class mappool {
	public:
		typedef map<S, address> sspool;
		typedef typename sspool::iterator iterator;
	private:
		sspool m_pool;
	public:
		mappool() : m_pool() {}
		~mappool() { fin(); }
		sspool &pool() { return m_pool; }
		void erase(iterator it) {
			address a = it->addr();
			it->fin();
			pool().erase(a);
		}
		S *create(SOCK sk) {
			address a;
			return a.from(sk) > 0 ? m_pool.create(a) : NULL;
		}
		bool init(const config &cfg) {
			return m_pool.init(cfg.m_max_connection, cfg.m_max_connection,
				sizeof(S), cfg.m_option);
		}
		void fin() { m_pool.fin(); }
	};
	template < class S, class P = mappool<S> >
	class factory_impl : public factory {
	protected:
		P m_container;
	public:
		typedef	typename P::sspool sspool;
		typedef typename P::iterator iterator;
	public:
		factory_impl() : factory(), m_container() {}
		~factory_impl() { fin(); }
		sspool &pool() { return m_container.pool(); }
		S* create(SOCK sk) { return m_container.create(sk); }
		int init_pool(const config &cfg) { return m_container.init(cfg); }
		int broadcast(char *p, int l);
		bool checkping(class session &s, UTIME ut) {
			if (!cfg().client()) { return true; }
			int intv = (int)(ut - s.last_ping());
			if (intv > cfg().m_ping_intv) {
				if (S::sendping(s, ut) < 0) { return false; }
			}
			return intv > cfg().m_ping_timeo;
		}
	public:
		int init(const config &cfg);
		void fin();
		void poll(UTIME ut);
		static int on_open(SOCK);
		static int on_close(SOCK, int);
		static int on_recv(SOCK, char*, int);
		static int on_event(SOCK, char*, int);
		static int on_connect(SOCK, void *);
		static UTIME on_poll(SOCK);
	};
	class textprotocol {
	public:
		static const char cmd_ping[];
	public:
		static int sendping(class session &s, UTIME ut);
		static int recvping(class session &s, char *p, int l);
	protected:
		static bool cmp(const char *a, const char *b);
		static char *chop(char *p) { return nbr_str_chop(p); }
		static int hexdump(char *out, int olen, const char *in, int ilen);
	};
	class binprotocol {
	public:
		enum {
			cmd_ping = 0,
			cmd_last,
		};
	public:
		static int sendping(class session &s, UTIME ut);
		static int recvping(class session &s, char *p, int l);
	};
	typedef config property;
public:
	typedef enum {
		ss_invalid,
		ss_connecting,
		ss_connected,
		ss_closing,
		ss_closed,
	} ssstate;
	typedef enum {
		pr_continue = 1,
		pr_server_stop_client_continue = 0,
		pr_stop = -1,
	} pollret;
	static const size_t SOCK_ADDR_SIZE = address::SIZE;
protected:
	SOCK 	m_sk;
	factory *m_f;
	UTIME	m_last_access, m_last_ping;
	U32		m_latency;
	U8		m_conn_retry, m_state, m_padd[2];
	address	m_addr;
public:	/* usually dont need to touch */
	session() : m_f(NULL), m_last_access(0LL), m_last_ping(0LL), m_latency(0),
		m_conn_retry(0), m_state(ss_invalid), m_addr() {
		clear_sock();
	}
	~session() {}
	void set(SOCK sk, factory *f) 	{ m_sk = sk; m_f = f; }
	const config &cfg() const 		{ return f()->cfg(); }
	factory *f() 					{ return m_f; }
	const factory *f() const		{ return m_f; }
public: /* attributes */
	void update_access() 			{ m_last_access = nbr_clock(); }
	void update_ping(UTIME ut)		{ m_last_ping = ut; }
	void update_latency(U32 ut)		{ m_latency = ut; }
	UTIME last_access() const		{ return m_last_access; }
	UTIME last_ping() const			{ return m_last_ping; }
	U32  latency() const			{ return m_latency; }
	bool valid() const				{ return m_state != ss_invalid && m_state != ss_closed; }
	bool closed() const				{ return m_state == ss_closed; }
	void setstate(ssstate s)		{ m_state = s; }
	void setaddr(char *a = NULL);
	int senddata(U32 msgid, char *p, int l) { UNUSED(msgid); return send(p, l); }
	const address &addr() const		{ return m_addr; }
	address &addr() 				{ return m_addr; }
	int localaddr(address &a)		{
		int r;
		if ((r = nbr_sock_get_local_addr(m_sk, a.a(), address::SIZE)) > 0) {
			a.setlen(r);
		}
		return r;
	}
	void clear_sock()				{ nbr_sock_clear(&m_sk); }
	void incr_conn_retry()			{ if (m_conn_retry < 0xFF) { m_conn_retry++; } }
	void clear_conn_retry()			{ m_conn_retry = 0; }
	int conn_retry() const			{ return m_conn_retry; }
public: /* operation */
	int log(loglevel lv, const char *fmt, ...);
	int close() {
		int r = nbr_sock_close(m_sk);
		if (r >= 0) { setstate(ss_closing); }
		return r;
	}
	int writable() const			{ return nbr_sock_writable(m_sk); }
	U32 msgid()						{ return f()->msgid(); }
	int send(const char *p, int l) const	{ return cfg().m_fns(m_sk, (char *)p, l); }
	int event(const char *p, int l) const	{ return nbr_sock_event(m_sk, (char *)p, l); }
public: /* callback */
	pollret poll(UTIME ut, bool from_worker) { return pr_server_stop_client_continue; }
	void fin()						{}
	int on_open(const config &cfg)	{
		log(INFO, "%s %s\n", cfg.client() ? "connect to" : "accept from",(const char *)addr());
		return NBR_OK;
	}
	int on_close(int reason)		{
		log(INFO, "%s %s\n", cfg().client() ? "disconnected from" : "close",(const char *)addr());
		return NBR_OK;
	}
	int on_recv(char *p, int l)		{ return NBR_OK; }
	int on_event(char *p, int l)	{ return NBR_OK; }
};



/*-------------------------------------------------------------*/
/* sfc::finder												   */
/*-------------------------------------------------------------*/
class finder : public kernel {
public:
	static const char 	MCAST_GROUP[];
	static const size_t MAX_OPT_LEN = 1024;
	static const size_t SYM_SIZE = 32;
	static const int	DEFAULT_REGISTER = 3;
	static const int 	DEFAULT_HASHSZ = 16;
public:
	class factory : public session::factory {
	public:
		typedef map<session::factory*, char[SYM_SIZE]> smap;
		typedef session::factory super;
	protected:
		smap	m_sl;
	public:
		factory() : super(), m_sl() {}
		~factory() {}
		int init(const config &cfg);
		int init(const config &cfg, int (*pp)(SOCK, char*, int));
		void poll(UTIME ut) {}
		void fin() {}
		int inquiry(const char *sym);
		session::factory *find_factory(const char *sym) { return m_sl.find(sym); }
		bool register_factory(session::factory *fc, const char *sym) {
			return m_sl.insert(fc, sym) != m_sl.end();
		}
		template <class F> static int on_recv(SOCK, char*, int);
	};
	class property : public config {
	public:
		char 	m_mcastaddr[session::SOCK_ADDR_SIZE];
		U16		m_mcastport, m_padd;
		UDPCONF m_mcastconf;
	public:
		property(BASE_CONFIG_PLIST, const char *mcastgrp, U16 mcastport, int ttl);
		virtual int	set(const char *k, const char *v);
		virtual void *proto_p() const;
	};
protected:
	SOCK m_sk;
	factory *m_f;
public:
	finder(SOCK s, factory *f) : m_sk(s), m_f(f) {}
	~finder(){}
	int send(char *p, int l) { return m_f->cfg().m_fns(m_sk, p, l); }
	int close() { return nbr_sock_close(m_sk); }
	int log(loglevel lv, const char *fmt, ...);
public: /* callback */
	/* another node inquiry you. please reply if you have capability
	 * correspond to sym. opt is additional data to process with on_reply */
	int on_inquiry(const char *sym, char *opt, int omax) {
		log(INFO, "default: not respond to <%s>\n", sym);
		return NBR_ENOTFOUND;
	}
	/* some node reply to your inquiry. */
	int on_reply(const char *sym, const char *opt, int olen) { return NBR_OK; }
};



/*-------------------------------------------------------------*/
/* sfc::cluster										   		   */
/* provide n_servant x m_master mesh and master auto detection */
/*-------------------------------------------------------------*/
namespace cluster {
/* sfc::cluster::node */
class protocol : public session::binprotocol {
public:
	typedef enum {
		ncmd_invalid,
		ncmd_app,
		ncmd_update_node_state,
		ncmd_get_master_list,
		ncmd_broadcast,
		ncmd_unicast,
	} nodecmd;
	typedef enum {
		nev_finder_invalid,
		nev_finder_query_master_init,
		nev_finder_query_servant_init,
		nev_finder_query_poll,
		nev_finder_reply_init,
		nev_finder_reply_poll,
	} nodeevent;
};

class node : public session, public protocol {
public:
	class nodedata {
	public:
		U16	m_n_servant, m_padd;
	public:
		nodedata() : m_n_servant(0) {}
		int pack(char *p, int l);
		int unpack(const char *p, int l);
		void setdata(session::factory &f);
		static int cmp(nodedata *a, nodedata *b) {
			return (b->m_n_servant - a->m_n_servant);
		}
	};
	struct nodequery {
		char *p;
		int l;
	};
	typedef nodequery queryctx;
	class nodefinder : public finder {
	public:
		typedef enum  {
			fev_init_master_query,
			fev_init_servant_query,
			fev_poll_query,
			fev_init_reply,
			fev_poll_reply,
		} finderevent;
	public:
		nodefinder(SOCK sk, finder::factory *f) : finder(sk, f) {}
		int on_inquiry(const char *sym, char *opt, int omax);
		int on_reply(const char *sym, const char *opt, int olen);
		static bool check_sym(const char *sym_a, const char *sym_b);
	};
	class finder_mixin : public protocol {
	public:
		typedef enum {
			ns_invalid,
			ns_inquiry,		/* call inquiry but not replied yet */
			ns_found,		/* at least one node reply for inquiry */
			ns_establish,	/* connection done. */
		} nodestate;
	protected:
		U8 m_state, m_padd[3];
		UTIME m_last_inquiry;
		static finder::factory *m_finder;
		class node *m_primary;
		ringbuffer	m_pkt_bkup;
	public:
		finder_mixin() : m_state(ns_invalid),
			m_last_inquiry(0LL), m_primary(NULL), m_pkt_bkup() {}
		~finder_mixin() {}
		int init_finder(const config &cfg);
	public:
		void setstate(nodestate s) { m_state = s; }
		nodestate state() const { return (nodestate)m_state; }
		bool ready() const { return state() == ns_establish; }
		node *primary() { return m_primary; }
		const node *primary() const { return m_primary; }
		int writable() const { return primary() ? primary()->writable() : 0; }
		static finder::factory *finder() { return m_finder; }
		static inline nodeevent convert(nodefinder::finderevent e);
	public:
		int send(session &s, U32 msgid, char *p, int l);
		int unicast(const address &a, U32 msgid, char *p, int l);
		int broadcast(U32 msgid, char *p, int l);
		void switch_primary(node *new_node);
	};
	template <class S, class D = typename S::nodedata>
	class factory_impl : public session::factory_impl<S>, finder_mixin {
	public:
		typedef session::factory_impl<S> super;
	protected:
		class NODE : public S {
		public:
			class data : public D {
			public:
				data() : D() {}
				int pack(char *p, int l) const { return D::pack(p, l); }
				int unpack(const char *p, int l) const { return D::unpack(p, l); }
			} m_data;
		public:
			NODE() : m_data() {}
			data &get() { return m_data; }
			D *get() { return (D *)&m_data; }
			void setdata_from(session::factory &f) { D::setdata(f); }
			static int compare(void *a, void *b) {
				return D::cmp(((NODE *)a)->get(), ((NODE *)b)->get());
			}
			static void sort(NODE **list, int n_list) {
				::qsort(list, sizeof(NODE *), n_list, compare);
			}
		};
		typedef typename super::sspool sspool;
		typedef typename sspool::iterator iterator;
	public:
		factory_impl() : super(), finder_mixin() {}
		~factory_impl() { fin(); }
		sspool &pool() { return super::pool(); }
		inline bool master() const;
	public: /* operation */
		int update_nodestate(const address &a, const D &p);
	public:/* finder related */
		int on_finder_reply_init(char *p, int l);
		int on_finder_reply_poll(char *p, int l);
	public:/* callbacks */
		int init(const config &cfg);
		void fin();
		void poll(UTIME ut);
		static int on_open(SOCK);
		static int on_close(SOCK, int);
		static int on_recv(SOCK, char*, int);
		static int on_event(SOCK, char*, int);
		static void on_mgr(SOCKMGR, int, char*, int);
		static int on_connect(SOCK, void *);
	};
	class property : public config {
	public:
		const U8 m_master;
		U8 m_multiplex, m_padd[2];
		static finder::property m_mcast;
		char m_sym_servant_init[finder::SYM_SIZE];
		char m_sym_master_init[finder::SYM_SIZE];
		char m_sym_poll[finder::SYM_SIZE];
		int m_packet_backup_size;
	public:
		property(BASE_CONFIG_PLIST, const char *sym, int master,
				int multiplex, int packet_backup_size);
		virtual int	set(const char *k, const char *v);
	};
public:
	node() : session() {}
	~node() {}
};

/* sfc::cluster::servantsession */
class servantsession : public session {
public:
	template <class S>
	class factory_impl : public session::factory_impl<S>, node::protocol {
	protected:
		session::factory *m_mnode;
	public:
		typedef session::factory_impl<S> super;
	public:
		factory_impl(session::factory *f) : super(), m_mnode(f) {}
		~factory_impl() {}
		session::factory *master_node() { return m_mnode; }
		int init(const config &cfg);
		static int on_recv(SOCK, char*, int);
	};
public:
	servantsession() : session() {}
	~servantsession() {}
};

/* sfc::cluster::mastersession */
class mastersession : public session {
public:
	template <class S>
	class factory_impl : public session::factory_impl<S>, node::protocol {
	protected:
		session::factory *m_mnode, *m_svnt;
	public:
		typedef session::factory_impl<S> super;
	public:
		factory_impl(session::factory *mnode, session::factory *svnt) :
			super(), m_mnode(mnode), m_svnt(svnt) {}
		~factory_impl() {}
		session::factory *master_node() { return m_mnode; }
		session::factory *servant_session() { return m_svnt; }
		int init(const config &cfg);
		static int on_recv(SOCK, char*, int);
	};
public:
	mastersession() : session() {}
	~mastersession() {}
};

/* sfc::cluster::masternode */
class masternode : public node {
public:
	template <class M, class S, class N>
	class factory_impl : public node::factory_impl<N> {
	public:
		typedef node::factory_impl<N> super;
		typedef typename super::sspool sspool;
		mastersession::factory_impl<M> m_fcmstr;	/* master factory */
		servantsession::factory_impl<S> m_fcsvnt;	/* servant factory */
	public:
		factory_impl() : super(), m_fcmstr(this, &m_fcsvnt), m_fcsvnt(this) {}
		~factory_impl() {}
		sspool pool() { return super::pool(); }
		int init(const config &cfg);
		void fin();
		void poll(UTIME ut);
		static void on_mgr(SOCKMGR, int, char*, int);
	public:/* finder related */
		int on_finder_query_init(bool is_master, char *p, int l);
		int on_finder_query_poll(char *p, int l);
	};
	template <class M, class S>
	class property : public node::property {
	public:
		typedef typename M::property MC;
		typedef typename S::property SC;
	protected:
		U16	m_master_port, m_servant_port;
		MC m_master_conf;	/* master config */
		SC m_servant_conf;	/* servant config */
	public:
		property(BASE_CONFIG_PLIST, const char *sym,
			int multiplex, int master_port, int servant_port,
			const MC &master_conf, const SC &servant_conf);
		virtual int	set(const char *k, const char *v);
	};
public:
	masternode() : node() {}
	~masternode() {}
};

}
using namespace cluster;
#include "session.h"



/*-------------------------------------------------------------*/
/* sfc::daemon												   */
/*-------------------------------------------------------------*/
class daemon : public kernel {
protected:
	typedef map<session::factory*, char[config::MAX_SESSION_NAME]>	smap;
	typedef map<config*, char[config::MAX_SESSION_NAME]>			cmap;
	smap 		m_sl;
	cmap		m_cl;
	static U32	m_sigflag;
public:
	static const int MAX_CONFIG_LIST = 16;
public:
	daemon() : m_sl(), m_cl() {}
	virtual ~daemon() {}
	int run();
	int	init(int argc, char *argv[]);
	int read_config(int argc, char *argv[]);
	void fin();
	int alive();
	static void stop();
public:
	template <class S> S *find_factory(const char *name) {
		return (S *)m_sl.find(name);
	}
	template <class C> C *find_config(const char *name) {
		return (C *)m_cl.find(name);
	}
public:
	static int log(loglevel lv, const char *fmt, ...);
	static inline int pid() { return nbr_osdep_getpid(); }
	static inline int bg() { return nbr_osdep_daemonize(); }
	static inline UTIME clock() { return nbr_clock(); }
	static inline int fork(char *cmd, char *argv[], char *envp[]) {
		return nbr_osdep_fork(cmd, argv, envp);
	}
	static void sigfunc(int signo);
public:
	virtual int					on_signal(int signo);
	virtual int					boot(int argc, char *argv[]);
	virtual void				shutdown() {}
	virtual int				 	initlib(CONFIG &c);
	virtual int					create_config(config *cl[], int size);
	virtual session::factory 	*create_factory(const char *sname);
};

}	//namespace sfc
#endif	//__SFC_H__
