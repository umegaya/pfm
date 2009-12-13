#if !defined(__SFC_H__)
#define __SFC_H__

#include "nbr.h"

namespace sfc {

namespace util {
/*-------------------------------------------------------------*/
/* sfc::util::array											   */
/*-------------------------------------------------------------*/
template <class E>
class array {
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
			void	*operator	new		(size_t, ARRAY a) {
				return nbr_array_alloc(a);
			}
			void	set(value v) { *this = v; }
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
			void	*operator	new		(size_t, ARRAY a) {
				return nbr_array_alloc(a);
			}
			void	set(value v) { data = v; }
			retval	*get() { return data; }
		};
	};
	template<typename T, size_t N>
	struct 	vcont<T[N]> {
		typedef T *value;
		typedef T retval;
		class	element {
		public:
			T	data[N];
		public:
			element() {}
			~element() {}
			void	*operator	new		(size_t, ARRAY a) {
				return nbr_array_alloc(a);
			}
			void	set(value v) {
				for (int i = 0; i < N; i++) { data[i] = v[i]; }
			}
			retval	*get() { return data; }
		};
	};
	typedef typename vcont<E>::element	element;
	typedef typename vcont<E>::value 	value;
	typedef typename vcont<E>::retval 	retval;
	typedef typename element 			*iterator;
protected:
	ARRAY	m_a;
public:
	array();
	~array();
	inline int 		init(int max, int size = -1,
						int opt = NBR_PRIM_EXPANDABLE);
	inline void 	fin();
	inline iterator insert(value v);
	inline retval	*create();
	inline void 	erase(iterator p);
	inline int		size();
	inline int		use();
	inline int		max();
public:
	inline iterator	begin() const;
	inline iterator	end() const;
	inline iterator	next(iterator p) const;
protected:
	inline element	*alloc();
};

/*-------------------------------------------------------------*/
/* sfc::util::map											   */
/*-------------------------------------------------------------*/
template <class V, typename K>
class map : public array<V> {
public:
	typedef array<V> super;
	typedef typename super::iterator iterator;
	/* specialization */
	enum { KT_NORMAL = 0, KT_PTR = 1, KT_INT = 2 };
	template<typename T>
	struct 	kcont {
		typedef const T &type;
		static const int ksz = sizeof(T);
		static const int kind = KT_NORMAL;
	};
	template<typename T, size_t N>
	struct 	kcont<T[N]> {
		typedef const T type[N];
		static const int ksz = sizeof(T) * N;
		static const int kind = KT_PTR;
	};
	template<typename T>
	struct 	kcont<T*> {
		typedef const T *type;
		static const int ksz = sizeof(T);
		static const int kind = KT_PTR;
	};
	template<>
	struct	kcont<int> {
		typedef int type;
		static const int ksz = sizeof(int);
		static const int kind = KT_INT;
	};
	typedef typename kcont<K>::type key;
	typedef typename super::value	value;
	typedef typename super::retval	retval;
	typedef typename super::element element;
protected:
	SEARCH	m_s;
public:
	map();
	~map();
	inline int 		init(int max, int hashsz,
						int size = -1,
						int opt = NBR_PRIM_EXPANDABLE);
	inline void 	fin();
	inline int 		insert(value v, key k);
	inline retval	*find(key k);
	inline int 		erase(key k);
protected:
	inline element	*find(key k);
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
public:
	char m_name[MAX_SESSION_NAME];
	char m_host[MAX_HOST_NAME];
	int m_max_connection;
	int m_timeout, m_option;
	int m_rbuf, m_wbuf;
	int m_ping_timeo, m_ping_intv;
	PROTOCOL *m_proto;
	UTIME m_taskspan, m_ld_wait;
	parser m_fnp;
	sender m_fns;

	static const int MAX_VALUE_STR = 256;
public:
	config();
	~config();

	virtual int str(const char *k, char *&v) const;
	virtual int	num(const char *k, int &v) const;
	virtual int	bignum(const char *k, U64 &v) const;
	virtual int	set(const char *k, const char *v);
	virtual void *proto_p() { return NULL; }
	virtual void set(const config &cfg){ *this = cfg; }
public:
	int	load(const char *line);
	static int commentline(const char *line) { return (line[0] == '#' ? 1 : 0); }
	static int emptyline(const char *line) { return (line[0] == '\0' ? 1 : 0); }
protected:
	int cmp(char *a, char *b);
	parser rparser_from(const char *str);
	sender sender_from(const char *str);
};

#include "util.h"
}	//namespace util
using namespace util;



/*-------------------------------------------------------------*/
/* sfc::session												   */
/*-------------------------------------------------------------*/
class session {
public:
	class factory {
		static const U32	MSGID_LIMIT = 2000000000;
		static const U16	MSGID_COMPACT_LIMIT = 60000;
	public:
		const config	&m_cfg;
		SOCKMGR			m_skm;
		UTIME			m_last_poll;
		U32				m_msgid_seed;
	public:
		factory() : m_skm(NULL), m_last_poll(0), m_msgid_seed(0) {}
		~factory() {}
		virtual int init(const config &cfg) = 0;
		virtual void fin() = 0;
		virtual void poll() = 0;
		const config &cfg() const { return m_cfg; }
		int connect(const char *addr = NULL, void *p = NULL);
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
	};
	template <class S>
	class factory_impl : public factory {
		array<S>		m_pool;
	public:
		factory_impl() : factory(), m_pool() {}
		~factory_impl() { fin(); }
		int init(const config &cfg);
		void fin();
		void poll();
		static int on_open(SOCK);
		static int on_close(SOCK, int);
		static int on_recv(SOCK, char*, int);
		static int on_event(SOCK, char*, int);
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
			U32 intv = (U32)(ut - m_last_sent);
			if (intv > s.cfg().m_ping_intv) {
				if (send(s) < 0) { return 0; }
			}
			return intv > m_attached.cfg().m_ping_timeo ? 0 : 1;
		}
	};
protected:
	SOCK 	m_sk;
	factory *m_f;
	pingmgr	m_ping;
	UTIME	m_last_access;
public:	/* usually dont need to touch */
	session() : m_ping() {}
	~session() {}
	void set(SOCK sk, factory *f) 	{ m_sk = sk; m_f = f; }
	pingmgr &ping()					{ return m_ping; }
	void update_access() 			{ m_last_access = nbr_clock(); }
	void clear_sock()				{ nbr_sock_clear(&m_sk); }
public: /* operation */
	const config &cfg() const 		{ return m_f->cfg(); }
	UTIME last_access() const		{ return m_last_access; }
	int valid() const				{ return nbr_sock_valid(m_sk) ? 1 : 0; }
	int send(char *p, int l) 		{ return m_f->m_cfg.m_fns(m_sk, p, l); }
	int close() 					{ return nbr_sock_close(m_sk); }
	int event(char *p, int l)		{ return nbr_sock_event(m_sk, p, l); }
	U32 msgid()						{ return m_f->msgid(); }
public: /* callback */
	int poll(UTIME ut);
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
enum {
	LOG_DEBUG = 1,
	LOG_INFO = 2,
	LOG_WARN = 3,
	LOG_ERROR = 4,
	LOG_FATAL = 5,
};
class daemon {
protected:
	typedef map<session::factory*, char[MAX_SESSION_NAME]> 	sslist;
	typedef map<config*, char[MAX_SESSION_NAME]>			cfglist;
	sslist 		m_sl;
	cfglist		m_cl;
	static U32	m_sigflag;
public:
	static const int DEFAULT_SIZE = 16;
public:
	daemon() : m_sl(), m_cl() {}
	~daemon() {}
	int run();
	int	init(int argc, char *argv[], config &list[], int n_list);
	int read_config(int argc, char *argv[], config &list[], int n_list);
	void fin();
public:
	int log(int prio, const char *fmt, ...);
	int daemonize() { return nbr_osdep_daemonize(); }
	static void sigfunc(int signo);
	int alive();
public:
	virtual int					on_signal(int signo);
	virtual int					boot(int argc, char *argv[]);
	virtual int				 	initlib(CONFIG &c);
	virtual config				*create_config(const char *sname);
	virtual session::factory 	*create_factory(const char *sname);
};

}	//namespace sfc

using namespace sfc;

#endif	//__SFC_H__
