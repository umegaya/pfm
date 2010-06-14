#include "ll.h"
#include "object.h"
#include "fiber.h"
#include "world.h"
#include "testutil.h"

using namespace pfm;
using namespace pfm::rpc;

static int ll_resume_test(int argc, char *argv[]);
static int ll_call_test(int argc, char *argv[]);

int ll_test(int argc, char *argv[])
{
	int r;
	TEST((r = ll_call_test(argc, argv)) < 0, "ll_call_test fail (%d)\n", r);
	TEST((r = ll_resume_test(argc, argv)) < 0, "ll_resume_test fail (%d)\n", r);
	return NBR_OK;
}




/* script execution test */
class test_fiber : public fiber {
public:
	int m_result;
public:
	test_fiber(world_id wid) { fiber::m_wid = wid; }
	int respond(bool err, serializer &sr) {
		if (err) { return NBR_EINVAL; }
		sr.unpack_start(sr.p(), sr.len());
		rpc::ll_exec_response d;
		if (sr.unpack(d) < 0) {
			return NBR_ESHORT;
		}
		if (d.err().type() != rpc::datatype::NIL) {
			return NBR_ECBFAIL;
		}
		m_result = (int)(ll::num)(d.ret());
		return m_result;
	}
	static int test_respond(fiber *f, bool err, serializer &sr) {
		return ((test_fiber *)f)->respond(err, sr);
	}
};

class test_coroutine : public ll::coroutine {
public:
	test_coroutine() { ll::coroutine::m_exec = NULL; }
};

static void push_rpc_context(serializer &sr, msgid_generator &seed, 
				object *o, world_id wid, size_t wlen)
{
	ll_exec_request::pack_header(sr, seed.new_id(), *o, "test_function",
			sizeof("test_function") - 1, wid, wlen, 4);
	sr.push_map_len(3);
	PUSHSTR(sr,key_a);
	sr << 111;
	PUSHSTR(sr,key_b);
	sr << 222;
	PUSHSTR(sr,key_c);
	sr << 333;
	sr.push_map_len(3);
	sr << 1;
	sr << 444;
	sr << 2;
	sr << 555;
	sr << 3;
	sr << 666;
	sr << 777;
	PUSHSTR(sr, abcdefghikjlmn);

}

int ll_call_test(int argc, char *argv[])
{
	int r;
	serializer sr;
	char b[1024], path[1024];
	world_factory wf(NULL, MAKEPATH(path, "rc/ll/wf.tch"));
	object_factory of;
	msgid_generator seed;
	ll scr(of, wf, sr, NULL);
	test_fiber fb("test_world"), fb2("test_world2");
	test_coroutine co;
	object *o1, *o2;
	world *w1, *w2;
	UUID uuid1, uuid2;

	fiber::m_test_respond = test_fiber::test_respond;
	TEST(!(w1 = wf.create("test_world", 10, 10)), "world1 create fail (%p)\n", w1);
	TEST(!(w2 = wf.create("test_world2", 10, 10)), "world2 create fail (%p)\n", w2);
	if ((r = of.init(1000000, 100000, opt_expandable | opt_threadsafe, 
		MAKEPATH(path, "rc/ll/of.tch"))) < 0) {
		TTRACE("init_object_factory fail (%d)\n", r);
		return r;
	}
	if ((r = scr.init(10000)) < 0) {
		TTRACE("lua::init fail (%d)\n", r);
		return r;
	}
	of.clear();
	wf.clear();
	uuid1.assign();
	if (!(o1 = of.create(uuid1, w1, &scr, "World"))) {
		TTRACE("create world object for test_world fails (%p)\n", o1);
		return NBR_EEXPIRE;
	}
	uuid2.assign();
	if (!(o2 = of.create(uuid2, w2, &scr, "World"))) {
		TTRACE("create world object for test_world2 fails (%p)\n", o2);
		return NBR_EEXPIRE;
	}
	if ((r = scr.init_world("test_world", NULL, 
		get_rcpath(path, sizeof(path), 
			argv[0], "rc/ll/test_world/main.lua"))) < 0) { 
		TTRACE("lua::init_world fail (test_world:%d)\n", r);
		return r;
	}
	if ((r = scr.init_world("test_world2", NULL,
		get_rcpath(path, sizeof(path),
			argv[0], "rc/ll/test_world2/main.lua"))) < 0) {
		TTRACE("lua::init_world fail (test_world:%d)\n", r);
		return r;
	}
	fb.set_msgid(1000000);
	fb2.set_msgid(1000001);
	
	/* call 
	map = { key_a => 111, key_b => 222, key_c => 333 }
	array = [444, 555, 666]
	number = 777
	string = "abcdefghijklmn"
	test_world:
	test_function(map, array, number, string) {
		return map['key_a'] + array[1] + number + strlen(string);
	} => should be 1346
	test_world2:
	test_function(map, array, number, string) {
		return map['key_b'] + array[2] + number + strlen(string);
	} => should be 1568 */

	/* test_world : test_function call */
	sr.pack_start(b, sizeof(b));
	push_rpc_context(sr, seed, o1, "test_world", sizeof("test_world") - 1);
	/* create ll_exec_request object */
	sr.unpack_start(sr.p(), sr.len());/* eat my shit */
	serializer::data d;
	if ((r = sr.unpack(d)) < 0) {
		TTRACE("create ll_exec_request fail (%d)\n", r);
		return r;
	}
	rpc::ll_exec_request &rq = (rpc::ll_exec_request &)d;

	if ((r = co.init(&fb, &scr)) < 0) {
		TTRACE("fiber init fail (%d)\n", r);
		return r;
	}
	TEST((r = co.call(rq, true)) < 0, "execute coroutine fails (%d)\n", r);
	ASSERT(1346 == fb.m_result);

	/* test_world2 : test_function call */
	sr.pack_start(b, sizeof(b));
	push_rpc_context(sr, seed, o2, "test_world2", sizeof("test_world2") - 1);
	/* create ll_exec_request object */
	sr.unpack_start(sr.p(), sr.len());/* eat my shit */
	if ((r = sr.unpack(d)) < 0) {
		TTRACE("create ll_exec_request fail (%d)\n", r);
		return r;
	}	
	if ((r = co.init(&fb2, &scr)) < 0) {
		TTRACE("fiber init fail (%d)\n", r);
		return r;
	}
	TEST((r = co.call(rq, true)) < 0, "execute coroutine fails (%d)\n", r);
	ASSERT(1568 == fb2.m_result);

	scr.fin();

	return NBR_OK;
}


/* for fiber resume test */
struct ll_resume_thread_context {
	int argc;
	char **argv;
	int result;
};

class testfiber : public fiber
{
public:
	rpc::ll_exec_response m_d;
public:
	testfiber() {}
	testfiber(world_id wid) { fiber::m_wid = wid; }
	rpc::ll_exec_response &result() { return m_d; }
	int respond(bool err, serializer &sr) {
		sr.unpack_start(sr.p(), sr.len());
		return sr.unpack(m_d);
	}
	static int test_respond(fiber *f, bool err, serializer &sr) {
		return ((testfiber *)f)->respond(err, sr);
	}
};

class test_object : public object {
public:
	test_object() : object() {}
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
		return ((test_object *)p)->request(sr);
	}
};

class test_world : public world {
public:
	static create_object_request m_d;
	static UUID m_uuid;
public:
	static int test_request(world *, MSGID msgid,
			const UUID &uuid, serializer &sr) {
		sr.unpack_start(sr.p(), sr.len());
		int r;
		TEST((r = sr.unpack(m_d)) < 0, "of:request invalid (%d)", r);
		m_uuid = m_d.object_id();
		return r;
	}
};
create_object_request test_world::m_d;
UUID test_world::m_uuid;

static void *ll_resume_test_thread(THREAD th);
static int ll_resume_test_thread_main(THREAD th, int argc, char *argv[]);


int ll_resume_test(int argc, char *argv[])
{
	THPOOL thp;
	THREAD th;
	int r;
	ll_resume_thread_context ctx;
	ctx.result = NBR_OK;
	ctx.argc = argc;
	ctx.argv = argv;
	TEST(!(thp = nbr_thpool_create(1)), "error create thread pool (%p)\n", thp);
	TEST(!(th = nbr_thread_create(thp, &ctx, ll_resume_test_thread)),
		"error create thread (%p)\n", th);

	nbr_thread_join(th, 0, NULL);

	return ctx.result;
}

void *ll_resume_test_thread(THREAD th)
{
	ll_resume_thread_context *ctx = 
		(ll_resume_thread_context *)nbr_thread_get_data(th);
	ASSERT(ctx);
	ctx->result = ll_resume_test_thread_main(th, ctx->argc, ctx->argv);
	return NULL;
}

int	ll_resume_test_thread_main(THREAD th, int argc, char *argv[])
{
	object_factory of;
	char path[1024], buf[65536];
	world_factory wf(NULL, MAKEPATH(path, "rc/ll/wf.tch"));
	fiber_factory<testfiber> ff(of, wf);
	serializer sr;
	msgid_generator seed;
	ll scr1(of, wf, sr, NULL), scr2(of, wf, sr, NULL);
	rpc::ll_exec_request req;
	rpc::ll_exec_response res;
	testfiber fb1("test_world"), fb2("test_world");
	test_coroutine co1, co2;
	int r;
	object *o1, *o2;
	test_object *to;
	UUID uuid1, uuid2;
	world *w;

	object::m_test_request = test_object::test_request;
	world::m_test_request = test_world::test_request;
	fiber::m_test_respond = testfiber::test_respond;
	TEST(!(w = wf.create("test_world", 10, 10)), "world create fail (%p)\n", w);
	TEST((r = scr1.init(10000)) < 0, "scr1 init fail (%d)\n", r);
	TEST((r = scr2.init(10000)) < 0, "scr2 init fail (%d)\n", r);
	TEST(!of.init(10000, 1000, 0, MAKEPATH(path,"rc/ll/of.tch")),
		"object factory creation fail (%d)\n", r = NBR_EINVAL);
	of.clear();
	wf.clear();
	TEST((r = scr1.init_world("test_world", NULL,
		MAKEPATH(path, "rc/ll/test_world/main.lua"))) < 0,
		"scr1 init_world fail (%d)\n", r);
	TEST((r = scr2.init_world("test_world", NULL,
		MAKEPATH(path, "rc/ll/test_world/main.lua"))) < 0,
		"scr2 init world fail (%d)\n", r);
	TEST((r = ff.init(10000, 100, 10)) < 0, "fiber initialize fail (%d)\n", r);
	TEST(!(ff.init_tls()), "fiber init tls fails (%d)\n", r = NBR_EPTHREAD);
	fb1.set_ff(&ff);
	fb1.set_msgid(seed.new_id());
	fb2.set_ff(&ff);
	fb2.set_msgid(seed.new_id());
	uuid1.assign();
	TEST(!(o1 = of.create(uuid1,w,&scr1,"Player")), "create o1 fail (%p)\n", o1);
	/* call Player:new */
	sr.pack_start(buf, sizeof(buf));
	TEST((r = pack_rpc_reqheader(sr, *o1, "new", "test_world", 1)) < 0,
		"pack_rpc_header fail (%d)\n", r);
	PUSHSTR(sr, umegaya);
	sr.unpack_start(sr.p(), sr.len());
	TEST((r = sr.unpack(req)) < 0, "unpack packed buf fails (%d)\n", r);
	/* Player:new should be yielding */
	TEST((r = co1.init(&fb1, &scr1)) < 0, "fb1 init fails (%d)\n", r);
	TEST((r = co1.call(req, true)) < 0, "fb1 call fails (%d)\n", r);/* should be yield */
	/* invoke fiber for creating Item object (co2) */
	TEST((r = co2.init(&fb2, &scr2)) < 0, "fb2 init fails (%d)\n", r);
	TEST((r = co2.call(test_world::m_d)) < 0, "fb2 call fails (%d)\n", r);
	/* HACK: force change o2 to remote object (to emulate RPC) */
	TEST((!(o2 = of.find(test_world::m_uuid)) || (o1 == o2)), "item object not created (%p:%p)", o1, o2);
	o2->set_flag(object::flag_local, false);
	to = (test_object *)o2;
	/* get retval of co2 and resume co1 */
	TEST((r = co1.resume(fb2.m_d)) < 0, "fb1 resume fails (%d)\n", r);
	/* it will finish! */
	TEST((fb1.result().ret().type() != rpc::datatype::ARRAY), "retval invalid (%d)\n",
		fb1.result().ret().type());
	/* fb1.result().ret() should be object [LUA_TUSERDATA, Klass, uuid] */
	TEST((o1 != of.find(fb1.result().ret().elem(2)) && strcmp("Player", fb1.result().ret().elem(1))), 
		"Player:new: retval invalid (%p:%s)\n", o1, (const char *)fb1.result().ret().elem(1));

	/* call Player:attack */
	sr.pack_start(buf, sizeof(buf));
	TEST((r = pack_rpc_reqheader(sr, *o1, "attack", "test_world", 0)) < 0,
		"pack_rpc_header fail (%d)\n", r);
	sr.unpack_start(sr.p(), sr.len());
	TEST((r = sr.unpack(req)) < 0, "unpack packed buf fails (%d)\n", r);
	/* it also should be yielding */
	TEST((r = co1.init(&fb1, &scr1)) < 0, "fb1 init fails (%d)\n", r);
	TEST((r = co1.call(req, true)) < 0, "fb1 call fails (%d)\n", r);
	/* HACK : to execute remote function, o2 should be back to local object */
	o2->set_flag(object::flag_local, true);
	/* execute Item:get_damage in scr2 (rpc request should be stored into o2) */
	TEST((r = co2.init(&fb2, &scr2)) < 0, "fb2 init fails (%d)\n", r);
	TEST((r = co2.call(to->reqbuff(), true)) < 0, "fb2 call fails (%d)\n", r);
	/* HACK : o2 again get back to remote */
	o2->set_flag(object::flag_local, false);
	/* scr2::m_sr should contains return value of get_damage.
	resume fiber with this return value. */
	TEST((r = co1.resume(fb2.m_d)) < 0, "fb1 resume fails (%d)\n", r);
	/* execute Item:calc_value in scr2 */
	o2->set_flag(object::flag_local, true);
	TEST((r = co2.init(&fb2, &scr2)) < 0, "fb2 init fails (%d)\n", r);
	TEST((r = co2.call(to->reqbuff(), true)) < 0, "fb2 call fails (%d)\n", r);
	/* co1 resume again */
	o2->set_flag(object::flag_local, false);
	TEST((r = co1.resume(fb2.m_d)) < 0, "fb1 resume fails (%d)\n", r);
	/* it will finish! */
	ll::num rv = fb1.result().ret();
	TEST((rv != ll::num(318)), "retval invalid (%d)\n", (int)rv);
	return NBR_OK;
}
