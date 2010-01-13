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

namespace sfc {

#if defined(_DEBUG)
#include <assert.h>
#define ASSERT(c) 	assert(c)
#define TRACE(...)	fprintf(stderr, __VA_ARGS__)
#else
#define ASSERT(c)
#define TRACE(...)
#endif

namespace util {
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
	inline int 		init(int max, int size = -1,
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
	struct	kcont<C,int> {
		typedef int type;
		static SEARCH init(int max, int opt, int hashsz) {
			return nbr_search_init_int_engine(max, opt, hashsz);
		}
		static int regist(SEARCH s, type t, element *v) {
			return nbr_search_int_regist(s, t, v);
		}
		static void unregist(SEARCH s, type t) {
			nbr_search_int_unregist(s, t);
		}
		static element *get(SEARCH s, type t) {
			return nbr_search_int_get(s, t);
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
public:
	map();
	~map();
	inline int 		init(int max, int hashsz,
						int size = -1,
						int opt = opt_expandable);
	inline void 	fin();
	inline iterator	insert(value v, key k);
	inline retval	*find(key k) const;
	inline void		erase(key k);
	inline bool		initialized() { return super::initialized() && m_s != NULL; }
protected:
	inline element	*findelem(key k) const;
	inline element	*alloc(key k);
};

/*-------------------------------------------------------------*/
/* sfc::util::config										   */
/*-------------------------------------------------------------*/
class config {
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
	const char *m_proto_name;
	UTIME m_taskspan, m_ld_wait;
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
	const char *proto_name,				\
	UTIME taskspan, UTIME ld_wait,		\
	parser fnp, sender fns, U32 flag
#define BASE_CONFIG_CTOR()				\
	config(name, host, max_connection,	\
	timeout, option, rbuf, wbuf, 		\
	ping_timeo, ping_intv,				\
	proto_name, taskspan, ld_wait,		\
	fnp, fns, flag)

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
protected:
	static int cmp(const char *a, const char *b);
	parser rparser_from(const char *str);
	sender sender_from(const char *str);
};

#include "util.h"
}	//namespace util
using namespace util;


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
/* sfc::session												   */
/*-------------------------------------------------------------*/
class session : public kernel {
public:
	class factory : public kernel {
		static const U32	MSGID_LIMIT = 2000000000;
		static const U16	MSGID_COMPACT_LIMIT = 60000;
	public:
		const config	*m_cfg;
		SOCKMGR			m_skm;
		UTIME			m_last_poll;
		U32				m_msgid_seed;
	public:
		factory() : m_cfg(NULL), m_skm(NULL), m_last_poll(0), m_msgid_seed(0) {}
		virtual ~factory() {}
		virtual int init(const config &cfg) = 0;
		virtual void fin() = 0;
		virtual void poll(UTIME ut) = 0;
		int log(loglevel lv, const char *fmt, ...);
		const config &cfg() const { return *m_cfg; }
		int connect(session *s, const char *addr = NULL, void *p = NULL);
		int mcast(const char *addr, char *p, int l);
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
					int (*oc)(SOCK, void*),
					void (*poll)(SOCK));
	};
	template <class S>
	class factory_impl : public factory {
	public:
		typedef	array<S> sspool;
		sspool		m_pool;
	public:
		factory_impl() : factory(), m_pool() {}
		~factory_impl() { fin(); }
		sspool &pool() { return m_pool; }
		int init(const config &cfg);
		void fin();
		void poll(UTIME ut);
		static int on_open(SOCK);
		static int on_close(SOCK, int);
		static int on_recv(SOCK, char*, int);
		static int on_event(SOCK, char*, int);
		static int on_connect(SOCK, void *);
		static void on_poll(SOCK);
	};
	class pingmgr {
	protected:
		U32				m_last_msgid;
		UTIME			m_last_sent;
	public:
		pingmgr() : m_last_msgid(0), m_last_sent(0) {}
		~pingmgr() {}
		int send(class session &s);
		int recv(class session &s, char *p, int l);
		int validate(class session &s, UTIME ut) {
			int intv = (int)(ut - m_last_sent);
			if (intv > s.cfg().m_ping_intv) {
				if (send(s) < 0) { return 0; }
			}
			return intv > s.cfg().m_ping_timeo ? 0 : 1;
		}
	};
protected:
	enum {
		attr_opened	= 0x0001,/* on_open called and on_close not called yet */
		attr_ping_fail = 0x0002,/* remote peer does not reply ping */
	};
	static const size_t SOCK_ADDR_SIZE = 32;
protected:
	SOCK 	m_sk;
	factory *m_f;
	pingmgr	m_ping;
	UTIME	m_last_access;
	U16		m_attr;
	U8		m_conn_failure, m_padd;
	char	m_addr[SOCK_ADDR_SIZE];
public:	/* usually dont need to touch */
	session() : m_f(NULL), m_ping(), m_last_access(0LL),
		m_attr(0), m_conn_failure(0) {
		m_addr[0] = '\0';
		clear_sock();
	}
	~session() {}
	void set(SOCK sk, factory *f) 	{ m_sk = sk; m_f = f; }
	pingmgr &ping()					{ return m_ping; }
public: /* attributes */
	void update_access() 			{ m_last_access = nbr_clock(); }
	void setattr(U32 a, bool on)	{ if (on) { m_attr |= a; } else { m_attr &= ~(a); } }
	bool attr(U32 a) const			{ return (m_attr & a); }
	void setaddr()					{ nbr_sock_get_addr(m_sk, m_addr, sizeof(m_addr)); }
	const char *addr() const		{ return m_addr; }
	void clear_sock()				{ nbr_sock_clear(&m_sk); }
	factory *f() 					{ return m_f; }
	const factory *f() const		{ return m_f; }
	void set_factory(factory *f)	{ m_f = f; }
	void incr_conn_failure()		{ if (m_conn_failure < 0xFF) { m_conn_failure++; } }
	void clear_conn_failure()		{ m_conn_failure = 0; }
	int conn_failure() const		{ return m_conn_failure; }
public: /* operation */
	int log(loglevel lv, const char *fmt, ...);
	const config &cfg() const 		{ return f()->cfg(); }
	UTIME last_access() const		{ return m_last_access; }
	bool valid() const				{ return attr(attr_opened); }
	int close() const				{ return nbr_sock_close(m_sk); }
	int writable() const			{ return nbr_sock_writable(m_sk); }
	U32 msgid()						{ return f()->msgid(); }
	int send(const char *p, int l) const	{ return cfg().m_fns(m_sk, (char *)p, l); }
	int event(const char *p, int l) const	{ return nbr_sock_event(m_sk, (char *)p, l); }
	const char *remoteaddr(char *b, int bl) const;
public: /* callback */
	int poll(UTIME ut, bool from_worker) { return NBR_OK; }
	void fin()						{}
	int on_open(const config &cfg)	{ return NBR_OK; }
	int on_close(int reason)		{ return NBR_OK; }
	int on_recv(char *p, int l)		{ return NBR_OK; }
	int on_event(char *p, int l)	{ return NBR_OK; }
};

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
	template <class S> S *find_session(const char *name) {
		return (S *)m_sl.find(name);
	}
	template <class C> C *find_config(const char *name) {
		return (C *)m_cl.find(name);
	}
public:
	static int log(loglevel lv, const char *fmt, ...);
	static int bg() { return nbr_osdep_daemonize(); }
	static int fork(char *cmd, char *argv[], char *envp[]) {
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
