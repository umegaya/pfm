#include "fiber.h"
#include "ll.h"
#include "object.h"
#include "testutil.h"
#include "world.h"

using namespace pfm;

class testfiber2 : public fiber {
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
	static int test_request(object *p, serializer &sr) {
		return ((test_object2 *)p)->request(sr);
	}
};

class test_object_factory2 : public object_factory {
public:
	rpc::create_object_request m_d;
public:
	test_object_factory2() : object_factory() {}
	int request_create(const UUID &uuid, serializer &sr) {
		sr.unpack_start(sr.p(), sr.len());
		return sr.unpack(m_d);
	}
};
rpc::data testfiber2::m_d;

int world_create(const char *path,
	fiber_factory<testfiber2> &ff, test_object_factory2 &tof,
	fiber_factory<testfiber2> &ff_create_object, UUID &wuuid)
{
	int r;
	testfiber2 *fb;
 	const char *nodes[] = {
 		"127.0.0.1:10101"
 	};
	/* for second world, use same wuuid for world object (then can share it) */
	PREPARE_PACK(ff.sr());
	MSGID msgid_dummy = 1000000;
	TEST((r = rpc::create_world_request::pack_header(ff.sr(), msgid_dummy,
		"test_world2", "", wuuid, path,
		1, nodes)) < 0, "pack world_create fail (%d)\n", r);
	ff.sr().unpack_start(ff.sr().p(), ff.sr().len());
	rpc::create_world_request cwr;
	TEST((r = ff.sr().unpack(cwr)) <= 0, "unpack world_create fail (%d)\n", r);
	TEST((r = ff.call(cwr, true)) < 0, "fiber call world_create fail (%d)\n", r);
	/* it should yield because of world object creation
	* then create world object */
	/* ff1's first fiber (now registerd as msgid = 1) 
	must have object creation information */
	TEST(!(fb = ff.find_fiber(1)), "world_create fiber not found(%p)\n", fb);
	/* send it to ff_create_object and create actual world object 
	(it should finish because it never yield inside) */
	TEST((r = ff_create_object.call(tof.m_d, true)) < 0,
		"world object_create fail(%d)\n", r);
	/* reply result to first fiber (msgid = 1) */
	TEST((r = ff.resume((rpc::response &)testfiber2::m_d)) < 0,
		"world_create resume fail (%d)\n", r);
	/* fiber should destroy and not registered any more. */	
	TEST((fb = ff.find_fiber(1)), "it wrongly yields...(%p)\n", fb);
	return NBR_OK;
}

#define MAX_THREAD 2

struct context {
	test_object_factory2 of;
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
	TEST((r = ff.call(cwr, true)) < 0, "fiber call world_create fail (%d)\n", r);
	if (ctx->main == t) {
		/* ff1's first fiber should registerd as msgid = 1 */
		CHECK_FIBER_EXIST(1);
		/* send it to ff_create_object and create actual world object
		(it should finish because it never yield inside) */
		TEST((r = ff.call(ctx->of.m_d, true)) < 0,
			"world object_create fail(%d)\n", r);
		TEST((r = ff.resume((rpc::response &)testfiber2::m_d)) < 0,
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
		TEST((r = ff.resume((rpc::response &)testfiber2::m_d)) < 0,
			"world_create resume fail (%d)\n", r);
		CHECK_FIBER_FINISH(2);
	}
	if (ctx->main == t) {
		/* create world object again for another thread. */
		TEST((r = ff.call(ctx->of.m_d, true)) < 0,
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
		TEST((r = ff.call(cor, true)) < 0,
				"create object fiber execution fails (%d)\n", r);
		CHECK_FIBER_EXIST(3);
		WAIT_SIGNAL;
		/* reply result to first fiber (msgid = 1) */
		TEST((r = ff.resume((rpc::response &)testfiber2::m_d)) < 0,
			"world_create resume fail (%d)\n", r);
		CHECK_FIBER_FINISH(3);
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
			TEST((r = ff.call(ler, true)) < 0, "get_id exec call fails(%d)\n", r);
		}
		rpc::ll_exec_response &rler = (rpc::ll_exec_response &)testfiber2::m_d;
		TEST(((ll::num)rler.ret() != ll::num(666)), "Player:get_id fails (%d)\n",
				(int)(ll::num)(666));
	}
	else {
		/* create world object again for another thread. */
		TEST((r = ff.call(to->reqbuff(), true)) < 0,
			"call World:get_id fail(%d)\n", r);
		/* wait for another thread */
		WAIT_SIGNAL;
	}

	/* last!! */
	ctx->result[ctx->main == t ? 0 : 1] = true;
	return NBR_OK;
}

int fiber_test(int argc, char *argv[])
{
	context ctx;
	fiber_factory<testfiber2> ff(ctx.of, ctx.wf);
	int r; char path[1024];
	dbm db;

	/* initialize modules */
	for (int i = 0; i < MAX_THREAD; i++) { ctx.ths[i] = NULL; }
	object::m_test_request = test_object2::test_request;
	TEST((r = db.init(MAKEPATH(path, "rc/uuid/uuid.tch"))) < 0,
		"uuid DB init fail (%d)\n",r);
	TEST((r = UUID::init(db)) < 0, "UUID init fail (%d)\n", r);
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
	}
	::sched_yield();
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
