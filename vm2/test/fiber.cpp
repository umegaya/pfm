#include "fiber.h"
#include "ll.h"
#include "object.h"
#include "testutil.h"
#include "world.h"
#include "connector.h"
#include "mstr/mstr.h"
#include "svnt/svnt.h"

using namespace pfm;


#define MAX_THREAD 2

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
	static int test_respond(pfm::fiber *f, bool e, serializer &sr) {
		return ((testmfiber *)f)->respond(e, sr);
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
	static int test_respond(pfm::fiber *f, bool e, serializer &sr) {
		return ((testsfiber *)f)->respond(e, sr);
	}
};
packet testsfiber::m_d;

class testsfiber2 : public testsfiber {};

struct node_data {
	conn_pool m_cp;
	connector_factory m_cf;
	object_factory m_of;
	world_factory m_wf;
	union {
	fiber_factory<testsfiber> *m_sff;
	fiber_factory<testmfiber> *m_mff;
	fiber_factory<testsfiber2> *m_sff2;
	};
	int m_type;
	bool m_have_world;
	node_data() : m_cp(), m_cf(), m_of(), m_wf(),
		m_sff(NULL) {}
	~node_data() { fin(); }
	void fin() {
		switch (m_type) {
		case type_servant: {
			if (m_sff) { delete m_sff; }
			m_sff = NULL;
		} break;
		case type_servant2: {
			if (m_sff2) { delete m_sff2; }
			m_sff2 = NULL;
		} break;
		case type_master: {
			if (m_mff) { delete m_mff; }
			m_mff = NULL;
		} break;
		}
		m_of.fin();
		m_wf.fin();
		m_cf.fin();
		m_cp.fin();
	}
	enum type {
		type_master,
		type_servant,
		type_servant2,
	};
};
struct test_context {
	node_data m_mnd;
	map<node_data, address> m_snd;
	node_data m_snd2;
	node_data *m_wond;
	UUID wuuid;
	THREAD thrd, workers[ffutil::max_cpu_core];
	int n_thread, n_return;
	test_context() : m_mnd(), m_snd() {}
	~test_context() {
		m_snd.fin();
	}
};

static void thevent(SWKFROM *frm, THREAD to, char *p, size_t l);
int init_node_data(node_data &d, const char *a, int type, char *argv[])
{
	int r;
	char path[1024], tmp[1024];
	TEST(!d.m_cp.pool().init(10, 10, -1, opt_threadsafe), "conn_pool init fail (%d)",
		r = NBR_EEXPIRE);
	TEST((r = d.m_cf.init(&(d.m_cp), 100, 100, 100)) < 0,
		"init connector factory fails (%d)\n", r);
	d.m_wf.set_cf(&(d.m_cf));
	d.m_type = type;
	switch (type) {
	case node_data::type_servant: {
		sprintf(tmp, "rc/fiber/%s.tch", a);
		TEST((r = d.m_of.init(10000, 1000, 0,
			MAKEPATH(path, tmp))) < 0,
			"object factory creation fail (%s)\n", path);
		sprintf(tmp, "rc/fiber/%s_w.tch", a);
		TEST((r = d.m_wf.init(64, 64, opt_expandable | opt_threadsafe,
			MAKEPATH(path, tmp))) < 0,
			"world factory creation fail (%d)\n", r);
		d.m_of.clear();
		TEST(!(d.m_sff = new fiber_factory<testsfiber>(d.m_of, d.m_wf)),
				"init servant fiber factory fails (%d)\n", r = NBR_EEXPIRE);
		TEST((r = d.m_sff->init(10000, 100, 10)) < 0,
				"fiber_factory init fails(%d)\n", r);
		TEST((r = d.m_sff->init_tls()) < 0, 
				"fiber_factory init_tls fails (%d)\n", r);
	} break;
	case node_data::type_servant2: {
		sprintf(tmp, "rc/fiber/%s.tch", a);
		TEST((r = d.m_of.init(10000, 1000, 0,
			MAKEPATH(path, tmp))) < 0,
			"object factory creation fail (%s)\n", path);
		sprintf(tmp, "rc/fiber/%s_w.tch", a);
		TEST((r = d.m_wf.init(64, 64, opt_expandable | opt_threadsafe,
			MAKEPATH(path, tmp))) < 0,
			"world factory creation fail (%d)\n", r);
		d.m_of.clear();
		TEST(!(d.m_sff2 = new fiber_factory<testsfiber2>(d.m_of, d.m_wf)),
				"init servant2 fiber factory fails (%d)\n", r = NBR_EEXPIRE);
		TEST((r = d.m_sff2->init(10000, 100, 10)) < 0,
				"fiber_factory init fails(%d)\n", r);
	} break;
	case node_data::type_master: {
		TEST((r = d.m_of.init(10000, 1000, 0,
			MAKEPATH(path, "rc/fiber/mof.tch"))) < 0,
			"object factory creation fail (%d)\n", r);
		sprintf(tmp, "rc/fiber/%s_w.tch", a);
		TEST((r = d.m_wf.init(64, 64, opt_expandable | opt_threadsafe,
			MAKEPATH(path, tmp))) < 0,
			"world factory creation fail (%d)\n", r);
		TEST(!(d.m_mff = new fiber_factory<testmfiber>(d.m_of, d.m_wf)),
				"init servant fiber factory fails (%d)\n", r = NBR_EEXPIRE);
		TEST((r = d.m_mff->init(10000, 100, 10)) < 0,
				"fiber_factory init fails(%d)\n", r);
	} break;
	}
	return NBR_OK;
}



/*------------------------------------------------------------------*/
/* hook fiber response		 										*/
/*------------------------------------------------------------------*/
//template <class T> static int mresponse(serializer &sr, T &p) {
//	return testmfiber::m_d.unpack(sr, p);
//}
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
static void thevent(SWKFROM *f, THREAD to, char *p, size_t l) {
	packet *pkt = m_thevmap.create((U64)to);
	if (!pkt) { return; }
	TTRACE("thev %u byte to %p\n", l, pkt->m_th = to);
	pkt->set(p, l);
	emit_msg_return();
}


/*------------------------------------------------------------------*/
/* hook packet sending via world /object 							*/
/*------------------------------------------------------------------*/
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
UUID test_world2::m_uuid_sent;
static rpc::create_object_request &get_cor() { return test_world2::m_d; }
static rpc::ll_exec_request &get_ler(test_object2 *o) { return o->reqbuff(); }


/*------------------------------------------------------------------*/
/* test : node_ctrl :: add 											*/
/*------------------------------------------------------------------*/
int 
get_world_object_node(test_context &ctx, node_data *&nd)
{
	int r;
	connector *ct;
	world *w;
	address a;
	TEST(!(w = ctx.m_mnd.m_wf.find("test_world2")), "world not found (%p:%d)\n", 
		w, r = NBR_ENOTFOUND);
	TEST(!(ct = (connector *)w->_connect_assigned_node(ctx.wuuid)),
		"connector not found (%p:%d)\n", ct, r = NBR_ENOTFOUND);
	a = ct->primary()->get_addr(a);
	TEST(!(nd = ctx.m_snd.find(a)),
		"node data which handle world object not found (%p:%d)\n", 
		nd, r = NBR_ENOTFOUND);
	return NBR_OK;
}
int fiber_test_add_node(test_context &ctx, int argc, char *argv[], void *p)
{
	int r;
	serializer local_sr;
	node_data *nd, *wnd;
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
	TEST((r = ctx.m_mnd.m_mff->recv(ctx.thrd,
		local_sr.p(), local_sr.len(), true)) < 0,
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
		nd->m_have_world = (bool)(nd->m_wf.find(test_world_id));
		TEST((r = nd->m_sff->call(ctx.thrd, req, true)) < 0,
			"call servant add node fails (%d)\n", r);
		/* if world is first created, then vm_init fiber will create. */
		if (!nd->m_have_world) { thread_msg_wait(ctx); }
		TEST(!(r = get_world_object_node(ctx, wnd)) < 0,
			"there is not node for world object (%p)\n", wnd);
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
			/* it will initiate object creation in wnd */
			TEST((r = wnd->m_sff->call(c, get_cor(), true)) < 0,
				"create world object fails (%d)\n", r);
			/* it will send back reply to each vm_init fiber */
			TEST((r = sresponse(nd->m_sff->sr(), resp)) <= 0,
				"response unpack fails (%d)\n", r);
			TEST((r = nd->m_sff->resume_nofw(c, resp)) < 0,
				"resume servant add node fails (%d)\n", r);
			/* it will reply to main add node fiber */
			TEST((r = sresponse(nd->m_sff->sr(), resp)) <= 0,
				"response unpack fails (%d)\n", r);
			SWKFROM from = { fiber::from_thread, ntpit->m_th };
			TEST((r = nd->m_sff->resume_nofw(&from, resp)) < 0,
				"resume servant add node fails (%d)\n", r);
			m_thevmap.erase((U64)ntpit->m_th);
		}
		/* all sock worker (vm_init: only first time) + add node fiber */
		TEST((r = nd->m_sff->use()) != (1 + (nd->m_have_world ? 0 : ctx.n_thread)),
			"not resume correctly(%d)\n", r);
		TEST((r = m_thevmap.use()) != 0,
			"unprocessed packet exist (%d)\n", r);

		/* add node fiber should finish and reply to master add node fiber */
		TEST(!(c = ctx.m_mnd.m_cf.get_by(npit->m_a)), 
			"cannot found connection from servant (%p:%d)\n", 
			c, r = NBR_ENOTFOUND);
		TEST((r = sresponse(nd->m_sff->sr(), resp)) <= 0,
			"response unpack fails (%d)\n", r);
		TEST((r = ctx.m_mnd.m_mff->resume_nofw(c, resp)) < 0,
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
		/* find connection to master */
		TEST(!(c = nd->m_cf.get_by("127.0.0.1:8000")), 
			"cannot find conn (%p:%d)\n", c, r = NBR_ENOTFOUND);
		init_thread_msg_wait(ctx);
		TEST((r = nd->m_sff->resume_nofw(c, resp)) < 0, 
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
			TEST((r = nd->m_sff->resume_nofw(ntpit->m_th, resp)) < 0,
				"resume servant add node fails (%d)\n", r);
			m_thevmap.erase((U64)ntpit->m_th);
		}
		TEST((r = m_thevmap.use()) != 0, "unprocessed packet exist (%d)\n", r);
		/* after global commit, no fiber should be running */
		TEST((r = nd->m_sff->use()) != 0, "wrongly resume (%d)\n", r);
		test_conn::m_pktmap.erase(npit->m_a);
	}
	TEST((r = test_conn::m_pktmap.use()) != 0, 
		"packet not processed correctly (%d)\n",r);
	TEST((ctx.m_mnd.m_mff->quorum_locked(test_world_id)),
		"context still locked (master) (%d)\n", r = NBR_EINVAL);
	TEST((r = ctx.m_mnd.m_mff->use()) != 0,
		"fiber not successfully finished (%d)\n", r);
	return NBR_OK;
}



/*------------------------------------------------------------------*/
/* test : login			 											*/
/*------------------------------------------------------------------*/
int fiber_test_login(test_context &ctx, int argc, char *argv[])
{
	int r;
	packet *p;
	rpc::login_request req;
	rpc::login_response res;
	rpc::create_object_request req2;
	rpc::ll_exec_response res2;
	node_data *svnt, *nd, *wnd;
	int idx = nbr_rand32() % ctx.m_snd.use();
	map<node_data, address>::iterator it = ctx.m_snd.begin();
	for (int i = 0; i < idx; i++) {
		it = ctx.m_snd.next(it);
	}
	conn c;
	svnt = &(*it);
	PREPARE_PACK(svnt->m_sff->sr());
	TEST((r = rpc::login_request::pack_header(svnt->m_sff->sr(), 1000006,
		"test_world2", sizeof("test_world2") - 1,
		"user", "password", sizeof("password") - 1)) < 0,
		"pack login command fails (%d)\n", r);
	PREPARE_UNPACK(svnt->m_sff->sr());
	TEST((r = svnt->m_sff->sr().unpack(req)) <= 0,
		"unpack login command fails (%d)\n", r);
	TEST((r = svnt->m_sff->call(&c, req, false)) < 0,
		"call login command fails (%d)\n", r);
	TEST((r = test_conn::m_pktmap.use()) != 1, 
		"packet send too much (%d)\n", r);
	TEST(!(p = test_conn::m_pktmap.find("127.0.0.1:8000")),
		"packet not send correctly (%p:%d)\n", p, r = NBR_ENOTFOUND);
	TEST((r = p->unpack(svnt->m_sff->sr(), req)) < 0,
		"packet unpack fails (%d)\n", r);
	TEST((r = ctx.m_mnd.m_mff->call(&c, req, true)) < 0,
		"call login command (mstr) fails (%d)\n", r);
	TEST((r = sresponse(svnt->m_sff->sr(), res)) < 0,
		"invalid login response from mstr (%d)\n", r);
	test_conn::m_pktmap.erase("127.0.0.1:8000");
	connector *ct;
	world *w;
	address a;
	TEST(!(w = ctx.m_mnd.m_wf.find("test_world2")), "world not found (%p)\n", w);
	TEST(!(ct = (connector *)w->_connect_assigned_node(res.object_id())),
			"connector not found (%p)\n", ct);
	a = ct->primary()->get_addr(a);
	TEST(!(nd = ctx.m_snd.find(a)),
			"node data which handle world object not found (%p)\n", nd);
	TEST((r = svnt->m_sff->resume_nofw(&c, res)) < 0,
		"resume login response fails (%d)\n", r);
	/* will call Player:new */
	TEST((r = nd->m_sff->call(&c, get_cor(), true)) < 0,
		"create object call fails (%d)\n", r);
	/* will call World:get_id */
	object *o;
	TEST(!(o = nd->m_of.find(ctx.wuuid)), 
		"world object not found (%p:%d)\n", o, r = NBR_ENOTFOUND);
	test_object2 *to = (test_object2 *)o;
	TEST(!(ct = (connector *)w->_connect_assigned_node(ctx.wuuid)),
		"connector not found (%p)\n", ct);
	a = ct->primary()->get_addr(a);
	TEST(!(wnd = ctx.m_snd.find(a)),
		"node data which handle world object not found (%p)\n", wnd);
	TEST((r = wnd->m_sff->call(&c, get_ler(to), true)) < 0,
		"World:get_id call fails (%d)\n", r);
	/* resume Player:new */
	TEST((r = sresponse(nd->m_sff->sr(), res2)) < 0,
		"invalid login response from svnt (%d)\n", r);
	TEST((r = nd->m_sff->resume_nofw(&c, res2)) < 0,
		"resume login response fails (%d)\n", r);
	/* resume player login (now object creation finish) */
	TEST((r = sresponse(svnt->m_sff->sr(), res2)) < 0,
		"invalid login response from svnt (%d)\n", r);
	TEST((r = svnt->m_sff->resume_nofw(&c, res2)) < 0,
		"resume login response fails (%d)\n", r);
	/* will call Player:login and will return 788 */
	TEST((r = sresponse(svnt->m_sff->sr(), res2)) < 0,
		"invalid login response from svnt (%d)\n", r);
	TEST((ll::num(788) != (ll::num)res2.ret()),
		"retval invalid (%d)\n", (int)(ll::num)res2.ret());
	return NBR_OK;
}



/*------------------------------------------------------------------*/
/* test : ll_exec, creat_object										*/
/*------------------------------------------------------------------*/
int fiber_test_reg(test_context &ctx, int argc, char *argv[])
{
	return NBR_OK;
}

/*------------------------------------------------------------------*/
/* test : ll_exec, creat_object										*/
/*------------------------------------------------------------------*/
int fiber_test_ll(test_context &ctx, int argc, char *argv[])
{
	int r;
	object *o;
	test_object2 *to;
	rpc::response resp;
	connector *c;
	world *w;
	address a;
	node_data *nd;
	UUID uuid;
	THREAD t = nbr_thread_get_current();

	TEST(!(w = ctx.m_mnd.m_wf.find("test_world2")), "world not found (%p)\n", w);
	TEST(!(c = (connector *)w->_connect_assigned_node(ctx.wuuid)),
			"connector not found (%p)\n", c);
	TEST(!(nd = ctx.m_snd.find(c->primary()->get_addr(a))),
			"node data which handle world object not found (%p)\n", nd);
	fiber_factory<testsfiber> &ffw = *(nd->m_sff);
	/* 1. call Player:new (it will resume inside of it)*/
	/* find node to create Player object */
	uuid.assign();
	TEST(!(c = (connector *)w->_connect_assigned_node(uuid)),
		"connector not found (%p)\n", c);
	TEST(!(nd = ctx.m_snd.find(c->primary()->get_addr(a))),
		"node data which handle object creation not found (%p)\n", nd);
	fiber_factory<testsfiber> &ff = *(nd->m_sff);
	PREPARE_PACK(ff.sr());
	/* create request */
	rpc::create_object_request cor;
	TEST((r = rpc::create_object_request::pack_header(
		ff.sr(), 10000001/* msgid_dummy */, uuid,
		ll::player_klass_name, ll::player_klass_name_len,
		"test_world2", sizeof("test_world2") - 1, false, 0)) < 0,
		"pack create object fails (%d)\n", r);
	ff.sr().unpack_start(ff.sr().p(), ff.sr().len());
	TEST((r = ff.sr().unpack(cor)) <= 0, "unpack fails (%d)\n", r);
	TEST(!(o = ff.of().find(ctx.wuuid)), "world object not found (%p)\n", o);
	to = (test_object2 *)o;
	/* force set remote */
	to->set_flag(object::flag_local, false);
	TEST((r = ff.call(t, cor, true)) < 0,
		"create object fails (%d)\n", r);
	/* it should resume inside of it and call World object */
	TEST((r = ffw.call(t, get_ler(to), true)) < 0,
		"resume object creation fails (call World:get_id) (%d)\n", r);
	/* reply result to first fiber (msgid = 1) */
	TEST((r = sresponse(ff.sr(), resp)) < 0, "response unpack fails (%d)\n", r);
	TEST((r = ff.resume_nofw(t, resp)) < 0, "world_create resume fail (%d)\n", r);
	TEST(!(o = ff.of().find(uuid)), "Player object not found (%p)\n", o);
	TEST((0 != strcmp(o->klass(), "Player")),
			"klass type invalid (%s)\n", o->klass());
	/* call Player:get_id */
	rpc::ll_exec_request ler;
	TEST((r = rpc::ll_exec_request::pack_header(
		ff.sr(), 10000002/* msgid_dummy */, *o,
		"get_id", sizeof("get_id") - 1,
		"test_world2", sizeof("test_world2") - 1, 0)) < 0,
		"pack ll exec fails (%d)\n", r);
	ff.sr().unpack_start(ff.sr().p(), ff.sr().len());
	TEST((r = ff.sr().unpack(ler)) <= 0, "ll exec unpack fails(%d)\n", r);
	TEST((r = ff.call(t, ler, true)) < 0, "get_id exec call fails(%d)\n", r);
	/* check return value */
	rpc::ll_exec_response rler;
	TEST((r = sresponse(ff.sr(), rler)) < 0, "response unpack fails (%d)\n", r);
	TEST(((ll::num)rler.ret() != ll::num(666)), "Player:get_id fails (%d)\n",
			(int)(ll::num)(rler.ret()));
	return NBR_OK;
}



/*------------------------------------------------------------------*/
/* test : main														*/
/*------------------------------------------------------------------*/
int fiber_test_thread(test_context &ctx, int argc, char *argv[])
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
	char path[1024];
	conn *c;
	node_data *nd;
	dbm db;
	packet *pkt;

	/* set callback to emulate network IO */
	object::m_test_request = test_object2::test_request;
	world::m_test_request = test_world2::test_request;
	conn::m_test_send = test_conn::test_send;
	conn_pool::m_test_connect = test_conn_pool::test_connect;
	fiber::m_test_respond = testsfiber::test_respond;
	rpc::response resp;
	rpc::request req;

	TEST((r = db.init(MAKEPATH(path, "rc/uuid/uuid.tch"))) < 0,
		"uuid DB init fail (%d)\n",r);
	TEST((r = UUID::init(db)) < 0, "UUID init fail (%d)\n", r);	
	TEST((r = testmfiber::init_global(10000, MAKEPATH(path, "rc/fiber/al.tch"))) < 0,
		"testmfiber::init fails(%d)\n", r);
	testmfiber::m_al.clear();
	TEST((r = testsfiber::init_global(1000)) < 0, "testsfiber::init fails(%d)\n",r);
	TEST(!(test_conn::m_pktmap.init(100, 100)), 
		"test_conn::pktmap init fail (%d)\n", r = NBR_EEXPIRE);
	TEST(!m_thevmap.init(100, 100, -1, opt_threadsafe | opt_expandable), 
		"thread event map init fails (%d)\n", r = NBR_EEXPIRE);
	TEST((r = init_node_data(ctx.m_mnd, "127.0.0.1:8000",
		node_data::type_master, argv)) < 0,
		"create master connector factory fail (%d)\n", r);
	TEST((r = ctx.m_mnd.m_mff->init_tls()) < 0,
		"init TLS fails (%d)\n", r);
	TEST((r = init_node_data(ctx.m_snd2, "127.0.0.1:9000",
		node_data::type_servant2, argv)) < 0,
		"create master connector factory fail (%d)\n", r);
	TEST(!ctx.m_snd.init(N_HOST, N_HOST, -1, opt_expandable),
		"init servant nodes fail(%d)", N_HOST);
	for (U32 i = 0; i < N_HOST; i++) {
		nd = ctx.m_snd.create(HOST_LIST[i]);
		TEST(!nd, "create node data fail (%s)\n", HOST_LIST[i]);
		TEST((r = init_node_data(*nd, HOST_LIST[i],
			node_data::type_servant, argv)) < 0,
			"init node data fail (%d)\n", r);
		/* emulate conneciton between master - servant */
		TEST(!(c = ctx.m_mnd.m_cp.create(HOST_LIST[i])),
			"reg node fail (%s)\n", HOST_LIST[i]);
		address addr(HOST_LIST[i]);
		c->set_addr(addr);
		connector *ct;
		TEST(!(ct = nd->m_cf.backend_connect("127.0.0.1:8000")),
			"backend connect fails (%p)\n", ct);
		TEST(!(c = nd->m_cp.find("127.0.0.1:8000")),
			"reg node fail (%s)\n", HOST_LIST[i]);
		PREPARE_PACK(nd->m_sff->sr());
		TEST((r = rpc::node_ctrl_cmd::regist::pack_header(
			nd->m_sff->sr(), 1000007,
			HOST_LIST[i], strlen(HOST_LIST[i]),
			servant_node)) < 0,
			"pack nodereg command fails (%d)\n", r);
		PREPARE_UNPACK(nd->m_sff->sr());
		TEST((r = nd->m_sff->sr().unpack(req)) <= 0,
			"unpack login command fails (%d)\n", r);
		THREAD th = NULL;
		TEST((r = nd->m_sff->call(th, req, true)) < 0,
			"call servant regnode fails (%d)\n", r);
		TEST(!(pkt = test_conn::m_pktmap.find("127.0.0.1:8000")),
			"packet not found (%d)\n", r = NBR_ENOTFOUND);
		TEST((r = pkt->unpack(ctx.m_mnd.m_mff->sr(), req)) < 0,
			"response unpack fails (%d)\n", r);
		TEST(!(c = ctx.m_mnd.m_cp.find(HOST_LIST[i])),
			"find node fail (%s)\n", HOST_LIST[i]);
		TEST((r = ctx.m_mnd.m_mff->call(c, req, true)) < 0,
			"call master reqnode fails (%d)\n", r);
		test_conn::m_pktmap.erase("127.0.0.1:8000");
		TEST((r = sresponse(nd->m_sff->sr(), resp)) < 0,
			"response unpack fails (%d)\n", r);
		TEST((r = nd->m_sff->resume_nofw(th, resp)) < 0,
			"resume servant regnode fails (%d)\n", r);
		TEST((r = nd->m_sff->use()) != 0,
			"fiber not stopped (%d)\n", r);
	}
	for (int i = 0; i < ctx.n_thread; i++) {
		nbr_sock_set_worker_data(ctx.workers[i], (void *)&ctx, thevent);
	}
	TEST((ctx.m_snd.use() != (int)N_HOST), "invalid servant node size (%d)\n", 
		ctx.m_snd.use());
	ctx.wuuid.assign();
	TTRACE("wuuid = (%s)\n", ctx.wuuid.to_s(path, sizeof(path)));

	for (U32 i = 0; i < N_HOST; i++) {
		address a;
		a.from(HOST_LIST[i]);
		TEST((r = fiber_test_add_node(ctx, argc, argv, (void *)&a)) < 0,
			"test_create fails (%d)\n", r);
	}
	TEST((r = fiber_test_login(ctx, argc, argv)) < 0,
			"test_login fails (%d)\n", r);
	TEST((r = fiber_test_ll(ctx, argc, argv)) < 0, "test_ll fails (%d)\n", r);

	return NBR_OK;
}

int fiber_test(int argc, char *argv[])
{
	thread<test_context> ctx;
	THPOOL thp;
	int r;
	TEST((r = config_thread_msg_wait(ctx)) < 0, "get thread num fails (%d)\n", r);
	TEST(!(thp = nbr_thpool_create(1)), "thread pool create fails (%p)\n", thp);
	for (int i = 0; i < ctx.n_thread; i++) {
		nbr_sock_set_worker_data(ctx.workers[i], (void *)&ctx, thevent);
	}
	TEST((r = EXEC_THREAD(thp, fiber_test_thread, ctx, true, NULL)) < 0,
		"fiber test thread create fail (%d)\n", r);
	nbr_thpool_destroy(thp);
	
	m_thevmap.fin();
	test_conn::m_pktmap.fin();
	mstr::fiber::fin_global();
	svnt::fiber::fin_global();
	ctx.m_mnd.fin();
	ctx.m_snd2.fin();
	ctx.m_snd.fin();

	return ctx.result();
}

