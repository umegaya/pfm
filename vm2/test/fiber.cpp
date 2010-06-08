#include "fiber.h"
#include "ll.h"
#include "object.h"
#include "testutil.h"
#include "world.h"
#include "connector.h"

using namespace pfm;

class testfiber2 : public svnt::fiber {
public:
	static rpc::data m_d;
public:
	int respond(bool err, serializer &sr) {
		if (err) { return NBR_ESYSCALL; }
		sr.unpack_start(sr.p(), sr.len());
		return sr.unpack(m_d);
	};
};
class test_object2 : public object {
public:
	test_object2() : object() {}
	int save(char *&p, int &l) { return l; }
	int load(const char *p, int l) { return NBR_OK; }
	rpc::ll_exec_request &reqbuff() {
		ASSERT(sizeof(rpc::ll_exec_request) <= sizeof(object::m_buffer));
		return *(rpc::ll_exec_request *)buffer(); }
	int request(class serializer &sr) {
		sr.unpack_start(sr.p(), sr.len());
		return sr.unpack(reqbuff());
	}
	static int test_request(object *p, MSGID, ll *, serializer &sr) {
		return ((test_object2 *)p)->request(sr);
	}
};

class test_world2 : public world {
public:
	static rpc::create_object_request m_d;
	static UUID m_uuid_sent;
public:
	test_world2() : world() {}
	static int test_request(world *w, MSGID msgid,
			const UUID &uuid, serializer &sr) {
		m_uuid_sent = uuid;
		sr.unpack_start(sr.p(), sr.len());
		return sr.unpack(m_d);
	}
};
rpc::create_object_request test_world2::m_d;
rpc::data testfiber2::m_d;
UUID test_world2::m_uuid_sent;

#define MAX_THREAD 2

struct context {
	object_factory of;
	world_factory wf;
	fiber_factory<testfiber2> *ff;
	THREAD main;
	UUID wuuid;
	const char *scr_path;
	volatile THREAD ths[MAX_THREAD];
	bool result[MAX_THREAD];
};

static int fiber_test_thread(THREAD t, context *ctx);
void *fiber_test_launcher(void *p)
{
	THREAD th = (THREAD)p;
	context *ctx = (context *)nbr_thread_get_data(th);
	int r = fiber_test_thread(th, ctx);
	if (r < 0) {
		TTRACE("fiber_test_thread fail (%d)\n", r);
	}
	nbr_mutex_unlock(nbr_thread_signal_mutex(th));
	return NULL;
}

#define WAIT_SIGNAL nbr_mutex_unlock(nbr_thread_signal_mutex(t));				\
					::sched_yield();											\
					TEST((r = nbr_thread_wait_signal(t, 1, SIGNAL_WAIT)) < 0, 	\
						"wait signal fail (%d)\n", r)

#define CHECK_FIBER_EXIST(msgid)		TEST(!(fb = ff.find_fiber(msgid)),		\
						"fiber not found(%p:%d)\n", fb, r = NBR_ENOTFOUND);

#define CHECK_FIBER_FINISH(msgid)		TEST((fb = ff.find_fiber(msgid)), 		\
						"it wrongly yields...(%p)\n", fb);

#define SEND_SIGNAL_AND_WAIT(t)	TRACE("%s(%u) send signal start\n", __FILE__, __LINE__);		\
						TEST((r = nbr_thread_signal(t, 1)) < 0, 		\
						"send signal fails (%d)", r);							\
						::sched_yield();										\
						TRACE("enter thread mutex lock\n");						\
						nbr_mutex_lock(nbr_thread_signal_mutex(t));				\
						TRACE("eXit thread mutex lock\n");						\
						::sched_yield();						\
						nbr_mutex_unlock(nbr_thread_signal_mutex(t));			\
						::sched_yield();						\
						TRACE("%s(%u) send signal end\n", __FILE__, __LINE__)


int fiber_test_thread(THREAD t, context *ctx)
{
	int r;
	testfiber2 *fb;
	object *o;
	test_object2 *to;
	UUID uuid;
	rpc::create_world_request cwr;
	fiber_factory<testfiber2> &ff = *(ctx->ff);
	serializer local_sr;
	const char *nodes[] = {
		"127.0.0.1:10101"
	};
	static const int SIGNAL_WAIT = 3 *1000;
	WAIT_SIGNAL;
	/* create world command */
	ctx->result[ctx->main == t ? 0 : 1] = false;

	/* 1. create request object */
	PREPARE_PACK(local_sr);
	MSGID msgid_dummy = 1000000;
	TEST((r = rpc::create_world_request::pack_header(local_sr, msgid_dummy,
		"test_world2", "", ctx->wuuid, ctx->scr_path,
		1, nodes)) < 0, "pack world_create fail (%d)\n", r);
	local_sr.unpack_start(local_sr.p(), local_sr.len());
	TEST((r = local_sr.unpack(cwr)) <= 0, "unpack world_create fail (%d)\n", r);

	/* 2. call world create request */
	/* it should yield because of world object creation then create world object */
	TEST((r = ff.call(t, cwr, true)) < 0, "fiber call world_create fail (%d)\n", r);
	if (ctx->main == t) {
		/* ff1's first fiber should registerd as msgid = 1 */
		CHECK_FIBER_EXIST(1);
		/* send it to ff_create_object and create actual world object
		(it should finish because it never yield inside) */
		TEST((r = ff.call(t, test_world2::m_d, true)) < 0,
			"world object_create fail(%d)\n", r);
		TEST((r = ff.resume(t, (rpc::response &)testfiber2::m_d)) < 0,
			"world_create resume fail (%d)\n", r);
		/* fiber should destroy and not registered any more. */
		CHECK_FIBER_FINISH(1);
		/* world object address */
		TEST(!(o = ff.of().find(ctx->wuuid)), "cannot find world object (%p)\n", o);
		TTRACE("world object (%p)\n", o);
		/* wait next signal */
		WAIT_SIGNAL;
	}
	else {/* wait until main thread finish world object creation
		 (it should be called twice) */
		CHECK_FIBER_EXIST(2);
		WAIT_SIGNAL;
		/* reply result to first fiber */
		TEST((r = ff.resume(t, (rpc::response &)testfiber2::m_d)) < 0,
			"world_create resume fail (%d)\n", r);
		CHECK_FIBER_FINISH(2);
	}
	if (ctx->main == t) {
		/* create world object again for another thread. */
		TEST((r = ff.call(t, test_world2::m_d, true)) < 0,
			"world object_create fail(%d)\n", r);
		WAIT_SIGNAL;
	}
	TEST(!(o = ff.of().find(ctx->wuuid)), "cannot find world object (%p)\n", o);
	TTRACE("world object (%p)\n", o);
	to = (test_object2 *)o;

	uuid.assign();
	if (ctx->main != t) {
		/* 3. create Player object (and resume inside of Player.new) */
		/* 3-1. call Player:new */
		/* create request */
		rpc::create_object_request cor;
		PREPARE_PACK(ff.sr());
		TEST((r = rpc::create_object_request::pack_header(
			ff.sr(), 10000001/* msgid_dummy */, uuid,
			ll::player_klass_name, ll::player_klass_name_len,
			"test_world2", sizeof("test_world2") - 1, 0)) < 0,
			"pack create object fails (%d)\n", r);
		ff.sr().unpack_start(ff.sr().p(), ff.sr().len());
		TEST((r = ff.sr().unpack(cor)) < 0,
				"create object request unpack fails (%d)\n", r);

		/* do actual call */
		TEST((r = ff.call(t, cor, true)) < 0,
				"create object fiber execution fails (%d)\n", r);
		WAIT_SIGNAL;
		/* reply result to first fiber (msgid = 1) */
		TEST((r = ff.resume(t, (rpc::response &)testfiber2::m_d)) < 0,
			"world_create resume fail (%d)\n", r);
		TEST(!(o = ff.of().find(uuid)), "Player object not found (%p)\n", o);
		TEST((0 != strcmp(o->klass(), "Player")),
				"klass type invalid (%s)\n", o->klass());

		/* call Player:get_id */
		{
			rpc::ll_exec_request ler;
			PREPARE_PACK(ff.sr());
			TEST((r = rpc::ll_exec_request::pack_header(
				ff.sr(), 10000002/* msgid_dummy */, *o,
				"get_id", sizeof("get_id") - 1,
				"test_world2", sizeof("test_world2") - 1, 0)) < 0,
				"pack ll exec fails (%d)\n", r);
			ff.sr().unpack_start(ff.sr().p(), ff.sr().len());
			TEST((r = ff.sr().unpack(ler)) <= 0, "ll exec unpack fails(%d)\n", r);
			TEST((r = ff.call(t, ler, true)) < 0, "get_id exec call fails(%d)\n", r);
		}
		rpc::ll_exec_response &rler = (rpc::ll_exec_response &)testfiber2::m_d;
		TEST(((ll::num)rler.ret() != ll::num(666)), "Player:get_id fails (%d)\n",
				(int)(ll::num)(666));
	}
	else {
		/* create world object again for another thread. */
		TEST((r = ff.call(t, to->reqbuff(), true)) < 0,
			"call World:get_id fail(%d)\n", r);
		/* wait for another thread */
		WAIT_SIGNAL;
	}

	/* last!! */
	ctx->result[ctx->main == t ? 0 : 1] = true;
	return NBR_OK;
}

int fiber_test_ll(int argc, char *argv[])
{
	context ctx;
	fiber_factory<testfiber2> ff(ctx.of, ctx.wf);
	int r; char path[1024];

	/* initialize modules */
	for (int i = 0; i < MAX_THREAD; i++) { ctx.ths[i] = NULL; }
	TEST((r = ctx.of.init(10000, 1000, 0, MAKEPATH(path, "rc/ll/of1.tch"))) < 0,
		"object factory creation fail (%d)\n", r);
	ctx.of.clear();
	TEST((r = ff.init(10000, 100, 10)) < 0, "fiber_factory init fails(%d)\n", r);
	ctx.wuuid.assign();
	TTRACE("wuuid = (%s)\n", ctx.wuuid.to_s(path, sizeof(path)));
	ctx.ff = &ff;
	ctx.scr_path = MAKEPATH(path, "rc/ll/test_world2/main.lua");

	THPOOL thp = nbr_thpool_create(MAX_THREAD);
	TEST(!thp, "thread pool creation fail (%p)\n", thp);
	for (int i = 0; i < MAX_THREAD; i++) {
		TEST(!(ctx.ths[i] = nbr_thread_create(thp, &ctx, fiber_test_launcher)),
			"thread launch fails (%d)\n", i);
		if (i == 0) { ctx.main = ctx.ths[i]; }
		::sched_yield();
	}
	/* i = 0 > main (world object created), i = 1 > slave */
	/* thread activate order is :
	 * 1. main (world object create finish)
	 * 2. slave (world object create yield)
	 * 3. main (world object create call again for slave)
	 * 4. slave (world object create resume -> finish ->
	 * 		call Player:new yield)
	 * 5. main (call World:get_id)
	 * 6. slave (Player:new resume -> finish -> Player:get_id call -> end)
	 * 7. main (end)
	 *  */
	SEND_SIGNAL_AND_WAIT(ctx.ths[0]);
	SEND_SIGNAL_AND_WAIT(ctx.ths[1]);
	SEND_SIGNAL_AND_WAIT(ctx.ths[0]);
	SEND_SIGNAL_AND_WAIT(ctx.ths[1]);
	SEND_SIGNAL_AND_WAIT(ctx.ths[0]);
	SEND_SIGNAL_AND_WAIT(ctx.ths[1]);
	SEND_SIGNAL_AND_WAIT(ctx.ths[0]);

	for (int i = 0; i < MAX_THREAD; i++) {
		TEST(0 != (r = nbr_thread_join(ctx.ths[i], 0, NULL)),
			"thread join fails (%d)\n", r);
	}
	for (int i = 0; i < MAX_THREAD; i++) {
		TEST(!ctx.result[i], "thread end in failure (%d:%d:%p)\n", 
			i, r = NBR_EINVAL, ctx.ths[i]);
	}
	return NBR_OK;
}

/*----------------------------------------------------------------------*/
/* typedef																*/
/*----------------------------------------------------------------------*/
struct packet {
	char *m_p;
	int m_l;
	address m_a;
	THREAD m_th;
	packet() : m_p(NULL), m_l(0), m_a(), m_th(NULL) {}
	~packet() { if (m_p) { nbr_free(m_p); m_p = NULL; } }
	int set(char *p, int l) {
		if (m_p) { nbr_free(m_p); }
		m_p = (char *)nbr_malloc(l);
		m_l = l;
		memcpy(m_p, p, m_l);
		return NBR_OK;
	}
	template <class T>
	int unpack(serializer &sr, T &d) {
		sr.unpack_start(m_p, m_l);
		return sr.unpack(d);
	}
};

class testmfiber : public mstr::fiber {
public:
	static packet m_d;
public:
	int respond(bool err, serializer &sr) {
		if (err) { return NBR_ESYSCALL; }
		return m_d.set(sr.p(), sr.len());
	}
};
packet testmfiber::m_d;

class testsfiber : public svnt::fiber {
public:
	static packet m_d;
public:
	int respond(bool err, serializer &sr) {
		if (err) { return NBR_ESYSCALL; }
		return m_d.set(sr.p(), sr.len());
	}
};
packet testsfiber::m_d;

struct node_data {
	conn_pool m_cp;
	connector_factory m_cf;
	object_factory m_of;
	world_factory m_wf;
	union {
	fiber_factory<testsfiber> *m_sff;
	fiber_factory<testmfiber> *m_mff;
	};
	bool m_servant, m_have_world;
	node_data() : m_cp(), m_cf(), m_of(), m_wf(),
		m_sff(NULL) {}
	~node_data() {
		if (m_servant) {
			if (m_sff) { delete m_sff; }
			m_sff = NULL;
		}
		else {
			if (m_mff) { delete m_mff; }
			m_mff = NULL;
		}
	}
};
struct test_context {
	node_data m_mnd;
	map<node_data, address> m_snd;
	node_data *m_wond;
	UUID wuuid;
	THREAD thrd, workers[ffutil::max_cpu_core];
	int n_thread, n_return;
	test_context() : m_mnd(), m_snd() {}
	~test_context() {
		m_snd.fin();
	}
};

int init_node_data(node_data &d, const char *a, bool servant, char *argv[])
{
	int r;
	char path[1024], tmp[1024];
	TEST(!d.m_cp.pool().init(10, 10, -1, opt_threadsafe), "conn_pool init fail (%d)",
		r = NBR_EEXPIRE);
	TEST((r = d.m_cf.init(&(d.m_cp), 100, 100, 100)) < 0,
		"init connector factory fails (%d)\n", r);
	d.m_wf.set_cf(&(d.m_cf));
	d.m_servant = servant;
	if (servant) {
		sprintf(tmp, "rc/fiber/%s.tch", a);
		TEST((r = d.m_of.init(10000, 1000, 0,
			MAKEPATH(path, tmp))) < 0,
			"object factory creation fail (%s)\n", path);
		TEST(!(d.m_sff = new fiber_factory<testsfiber>(d.m_of, d.m_wf)),
				"init servant fiber factory fails (%d)\n", r = NBR_EEXPIRE);
		TEST((r = d.m_sff->init(10000, 100, 10)) < 0,
				"fiber_factory init fails(%d)\n", r);
	}
	else {
		TEST((r = d.m_of.init(10000, 1000, 0,
			MAKEPATH(path, "rc/fiber/mof.tch"))) < 0,
			"object factory creation fail (%d)\n", r);
		TEST(!(d.m_mff = new fiber_factory<testmfiber>(d.m_of, d.m_wf)),
				"init servant fiber factory fails (%d)\n", r = NBR_EEXPIRE);
		TEST((r = d.m_mff->init(10000, 100, 10)) < 0,
				"fiber_factory init fails(%d)\n", r);
	}
	return NBR_OK;
}



/*------------------------------------------------------------------*/
/* hook fiber response		 										*/
/*------------------------------------------------------------------*/
template <class T> static int mresponse(serializer &sr, T &p) {
	return testmfiber::m_d.unpack(sr, p);
}
template <class T> static int sresponse(serializer &sr, T &p) {
	return testsfiber::m_d.unpack(sr, p);
}



/*------------------------------------------------------------------*/
/* hook network packet send 										*/
/*------------------------------------------------------------------*/
class test_conn : public conn {
public:
	static map<packet, address> m_pktmap;
	static int test_send(conn *c, char *p, int l) {
		address a;
		packet *pkt = m_pktmap.create(c->get_addr(a));
		if (!pkt) { return NBR_EEXPIRE; }
		pkt->m_a = c->get_addr(a);
		pkt->set(p, l);
		TTRACE("packet %u byte to %s\n", l, (const char *)pkt->m_a);
		return NBR_OK;
	}
};
map<packet, address> test_conn::m_pktmap;

class test_conn_pool : public conn_pool {
public:
	static int test_connect(conn_pool *, conn *c, const address &a, void *) {
		c->set_addr(a);
		return NBR_OK;
	}
};


/*------------------------------------------------------------------*/
/* hook thread event send 											*/
/*------------------------------------------------------------------*/
static test_context *g_context = NULL;
static void init_thread_msg_wait(test_context &ctx) {
	g_context = &ctx;
	ctx.n_return = 0;
}
static int config_thread_msg_wait(test_context &ctx) {
	return (ctx.n_thread = nbr_sock_get_worker(ctx.workers, ffutil::max_cpu_core));
}
static void thread_msg_wait(test_context &ctx) {
	while(false == __sync_bool_compare_and_swap(&(ctx.n_return), ctx.n_thread, 0)) {
		::sched_yield();
	}
}
static void emit_msg_return() {
	__sync_add_and_fetch(&(g_context->n_return), 1);
	TRACE("emit: %p: cnt = %u\n", g_context, g_context->n_return);
}
static map<packet, U64> m_thevmap;
static void thevent(THREAD from, THREAD to, char *p, size_t l) {
	packet *pkt = m_thevmap.create((U64)to);
	if (!pkt) { return; }
	TTRACE("thev %u byte to %p\n", l, pkt->m_th = to);
	pkt->set(p, l);
	emit_msg_return();
}


/*------------------------------------------------------------------*/
/* hook packet sending via world /object 							*/
/*------------------------------------------------------------------*/
static rpc::create_object_request &get_cor() { return test_world2::m_d; }
static UUID &get_req_uuid() { return test_world2::m_uuid_sent; }
static rpc::ll_exec_request &get_ler(test_object2 *o) { return o->reqbuff(); }


/*------------------------------------------------------------------*/
/* test : node_ctrl :: add 											*/
/*------------------------------------------------------------------*/
int fiber_test_add_node(test_context &ctx, int argc, char *argv[], void *p)
{
	int r;
	serializer local_sr;
	node_data *nd;
	world *w;
	conn *c;
	char path[1024];
	address &a = *(address *)p;
	rpc::node_ctrl_cmd::add add_req;
	MSGID msgid_dummy = 1000000;
	char test_world_id[] = "test_world2";
	PREPARE_PACK(local_sr);
	/* call master's add node */
	TEST((r = rpc::node_ctrl_cmd::add::pack_header(local_sr, msgid_dummy,
		test_world_id, sizeof(test_world_id) - 1, (const char *)a, a.len(),
		"", ctx.wuuid, MAKEPATH(path, "rc/ll/test_world2/main.lua"),
		0, NULL)) < 0, "pack world_create fail (%d)\n", r);
	local_sr.unpack_start(local_sr.p(), local_sr.len());
	TEST((r = local_sr.unpack(add_req)) <= 0, "unpack world_create fail (%d)\n", r);
	TEST((r = ctx.m_mnd.m_mff->call(ctx.thrd, add_req, true)) < 0,
		"add node master fail(%d)\n", r);

	/* master's add node will call servant's */
	map<packet, address>::iterator pit = test_conn::m_pktmap.begin(), npit;
	rpc::request req;
	rpc::response resp;
	for (;pit != test_conn::m_pktmap.end();) {
		npit = pit;
		pit = test_conn::m_pktmap.next(pit);
		TEST(!(nd = ctx.m_snd.find(npit->m_a)),
			"node not found (%s)\n", (const char *)npit->m_a);
		TTRACE("process %s/%p\n", (const char *)npit->m_a, nd->m_sff);
		init_thread_msg_wait(ctx);
		TEST((r = npit->unpack(nd->m_sff->sr(), req)) <= 0, 
			"unpack packet fails (%d)\n", r);
		nd->m_have_world = (nd->m_wf.find(test_world_id) != NULL);
		TEST((r = nd->m_sff->call(ctx.thrd, req, true)) < 0,
			"call servant add node fails (%d)\n", r);
		/* if world is first created, then vm_init fiber will create. */
		if (!nd->m_have_world) { thread_msg_wait(ctx); }
		/* servant's add node will call servant thread's vm_init */
		map<packet, THREAD>::iterator tpit = m_thevmap.begin(), ntpit;
		for (;tpit != m_thevmap.end();) {
			TRACE("vm_init called...\n");
			ntpit = tpit;
			tpit = m_thevmap.next(tpit);
			TEST((r = ntpit->unpack(nd->m_sff->sr(), req)) <= 0, 
				"unpack packet fails (%d)\n", r);
			TEST((r = nd->m_sff->call(ctx.thrd, req, true)) < 0,
				"call servant add node fails (%d)\n", r);
			/* it will initiate object creation */
			TEST((r = sresponse(nd->m_sff->sr(), resp)) <= 0,
				"response unpack fails (%d)\n", r);
			TEST((r = nd->m_sff->resume(ntpit->m_th, resp)) < 0,
				"resume servant add node fails (%d)\n", r);
			m_thevmap.erase((U64)ntpit->m_th);
		}
		/* all sock worker (vm_init: only first time) + add node fiber */
		TEST((r = nd->m_sff->use()) != (1 + (nd->m_have_world ? 0 : ctx.n_thread)),
			"not resume correctly(%d)\n", r);
		TEST((r = m_thevmap.use()) != 0,
			"unprocessed packet exist (%d)\n", r);
			
		connector *cn;
		address tmp;
		TEST(!(w = nd->m_wf.find(test_world_id)),
			"cannot find world object (%p)\n", w);
		TEST(!(cn = (connector *)w->_connect_assigned_node(ctx.wuuid)), 
			"cannot find connector for world object (%p)\n", cn);
		TEST(!(ctx.m_wond = ctx.m_snd.find(cn->primary()->get_addr(tmp))), 
			"cannot find node for world object (%p)\n", ctx.m_wond);
		TEST((r = ctx.m_wond->m_sff->call(ctx.thrd, get_cor(), true)) < 0, 
			"create world object fails (%d)\n", r);
		TEST((r = sresponse(nd->m_sff->sr(), resp)) <= 0,
			"response unpack fails (%d)\n", r);
		TEST((r = nd->m_sff->resume(ctx.thrd, resp)) < 0,
			"resume servant add node fails (%d)\n", r);
		/* only main add node fiber remains */
		TEST((r = nd->m_sff->use()) != (1 + (nd->m_have_world ? 0 : ctx.n_thread)),
			"not stop correctly(%d)\n", r);
		TEST(!(c = ctx.m_mnd.m_cf.get_by(npit->m_a)), "cannot find conn (%p)\n", c);
		test_conn::m_pktmap.erase(npit->m_a);
		TEST((r = sresponse(nd->m_sff->sr(), resp)) <= 0,
			"response unpack fails (%d)\n", r);
		TEST((r = ctx.m_mnd.m_mff->resume(c, resp)) < 0,
			"resume add node master fails (%d)\n", r);
	}
	/* if add node finished, then global_commit will send to servants */
	pit = test_conn::m_pktmap.begin();
	for (;pit != test_conn::m_pktmap.end();) {
		npit = pit;
		pit = test_conn::m_pktmap.next(pit);
		TEST(!(nd = ctx.m_snd.find(npit->m_a)),
			"node not found (%s)\n", (const char *)npit->m_a);
		TTRACE("process2 %s/%p\n", (const char *)npit->m_a, nd->m_sff);
		TEST((r = npit->unpack(nd->m_sff->sr(), resp)) < 0, 
			"unpack fails (%d)\n", r);
		TEST(!(c = nd->m_cf.get_by(npit->m_a)), 
			"cannot find conn (%p)\n", c);
		init_thread_msg_wait(ctx);
		TEST((r = nd->m_sff->resume(c, resp)) < 0, 
			"resume add node servant fails (%d)\n", r);
		TEST((nd->m_sff->quorum_locked(test_world_id)),
			"context still locked (%d)\n", r = NBR_EINVAL);
		/* for add node, global commit should be sent to vm_init fibers
		 * (if world is not exist before this command) */
		if (!nd->m_have_world) { thread_msg_wait(ctx); }
		map<packet, THREAD>::iterator tpit = m_thevmap.begin(), ntpit;
		for (;tpit != m_thevmap.end();) {
			TRACE("vm_init resumed...\n");
			ntpit = tpit;
			tpit = m_thevmap.next(tpit);
			TEST((r = ntpit->unpack(nd->m_sff->sr(), resp)) <= 0,
				"unpack packet fails (%d)\n", r);
			TEST((r = nd->m_sff->resume(ntpit->m_th, resp)) < 0,
				"resume servant add node fails (%d)\n", r);
			m_thevmap.erase((U64)ntpit->m_th);
		}
		TEST((r = m_thevmap.use()) != 0, "unprocessed packet exist (%d)\n", r);
		/* after global commit, no fiber should be running */
		TEST((r = nd->m_sff->use()) != 0, "wrongly resume (%d)\n", r);
	}
	TEST((ctx.m_mnd.m_mff->quorum_locked(test_world_id)),
		"context still locked (master) (%d)\n", r = NBR_EINVAL);
	TEST((r = ctx.m_mnd.m_mff->use()) != 0,
		"fiber not successfully finished (%d)\n", r);
	return NBR_OK;
}

int fiber_test_login(test_context &ctx, int argc, char *argv[])
{
	return NBR_OK;
}

int fiber_test(int argc, char *argv[])
{
	int r;

	const char *HOST_LIST[] = {
			"127.0.0.1:10101",
			"127.0.0.1:20202",
			"127.0.0.1:30303",
			"127.0.0.1:40404",
			"127.0.0.1:50505"
	};
	static const U32 N_HOST = 5;
	thread<test_context> ctx;
	char path[1024];
	conn *c;
	THPOOL thp;
	node_data *nd;
	dbm db;

	TEST((r = db.init(MAKEPATH(path, "rc/uuid/uuid.tch"))) < 0,
		"uuid DB init fail (%d)\n",r);
	TEST((r = UUID::init(db)) < 0, "UUID init fail (%d)\n", r);	
	TEST((r = testmfiber::init(10000, MAKEPATH(path, "rc/fiber/al.tch"))) < 0,
		"testmfiber::init fails(%d)\n", r);
	TEST((r = testsfiber::init()) < 0, "testsfiber::init fails(%d)\n",r);
	TEST(!(test_conn::m_pktmap.init(100, 100)), 
		"test_conn::pktmap init fail (%d)\n", r = NBR_EEXPIRE);
	TEST(!m_thevmap.init(100, 100, -1, opt_threadsafe | opt_expandable), 
		"thread event map init fails (%d)\n", r = NBR_EEXPIRE);
	TEST(!(thp = nbr_thpool_create(1)), "thread pool create fails (%p)\n", thp);
	TEST((r = init_node_data(ctx.m_mnd, "127.0.0.1:8000", false, argv)) < 0,
		"create master connector factory fail (%d)\n", r);
	TEST(!ctx.m_snd.init(N_HOST, N_HOST, -1, opt_expandable),
		"init servant nodes fail(%d)", N_HOST);
	for (U32 i = 0; i < N_HOST; i++) {
		nd = ctx.m_snd.create(HOST_LIST[i]);
		TEST(!nd, "create node data fail (%s)\n", HOST_LIST[i]);
		TEST((r = init_node_data(*nd, HOST_LIST[i], true, argv)) < 0,
			"init node data fail (%d)\n", r);
		/* emulate conneciton between master - servant */
		TEST(!(c = ctx.m_mnd.m_cp.create(HOST_LIST[i])),
			"reg node fail (%s)\n", HOST_LIST[i]);
		address addr(HOST_LIST[i]);
		c->set_addr(addr);
		TEST(!(c = nd->m_cp.create(HOST_LIST[i])), 
			"reg node fail (%s)\n", HOST_LIST[i]);
		c->set_addr(addr);
	}
	TEST((ctx.m_snd.use() != (int)N_HOST), "invalid servant node size (%d)\n", 
		ctx.m_snd.use());
	TEST((r = config_thread_msg_wait(ctx)) < 0, "get thread num fails (%d)\n", r);
	for (int i = 0; i < ctx.n_thread; i++) {
		nbr_sock_set_worker_data(ctx.workers[i], (void *)&ctx, thevent);
	}
	object::m_test_request = test_object2::test_request;
	world::m_test_request = test_world2::test_request;
	conn::m_test_send = test_conn::test_send;
	conn_pool::m_test_connect = test_conn_pool::test_connect;
	ctx.wuuid.assign();
	TTRACE("wuuid = (%s)\n", ctx.wuuid.to_s(path, sizeof(path)));

	for (U32 i = 0; i < N_HOST; i++) {
		address a;
		a.from(HOST_LIST[i]);
		TEST((r = EXEC_THREAD(thp, fiber_test_add_node, ctx, (void *)&a)) < 0,
			"test_create fails (%d)\n", r);
		TEST((r = ctx.result()) < 0, "test_create fails (%d)\n", r);
	}
	TEST((r = fiber_test_login(ctx, argc, argv)) < 0,
			"test_login fails (%d)\n", r);
	TEST((r = fiber_test_ll(argc, argv)) < 0, "test_ll fails (%d)\n", r);

	nbr_thpool_destroy(thp);
	return NBR_OK;
}

