#include "dbm.h"
#include "serializer.h"
#include "common.h"
#include "testutil.h"

using namespace pfm;

class test_object {
	struct {
		char buf[128];
		U32 len;
	} a[3];
	int al;
	struct {
		char key[16], val[64];
	} m[4];
	int ml;
	double dec;
	bool b;
	U64 u64;
public:
	test_object() { memset(this, 0, sizeof(*this)); }
	int save(char *&p, int &l) {
		if (sizeof(test_object) > (size_t)l) {
			p = (char *)malloc(sizeof(test_object));
			l = sizeof(test_object);
		}
		serializer sr;
		sr.pack_start(p, l);
		sr.push_array_len(5);
		sr.push_array_len(al);
		for (int i = 0; i < al; i++) {
			sr.push_raw(a[i].buf, a[i].len);
		}
		sr.push_map_len(ml);
		for (int i = 0; i < ml; i++) {
			sr.push_string(m[i].key, sizeof(m[i].key) - 1);
			sr.push_raw(m[i].val, sizeof(m[i].val));
		}
		sr << dec;
		sr << b;
		sr << u64;
		return (l = sr.len());
	}
	int load(const char *p, int l) {
		serializer sr;
		serializer::data d, ary, dmp;
		sr.unpack_start(p, l);
		sr.unpack(d);
		ASSERT(d.type() == rpc::datatype::ARRAY && d.size() == 5);
		ary = d.elem(0);
		ASSERT(ary.type() == rpc::datatype::ARRAY && ary.size() <= 3);
		al = ary.size();
		for (int i = 0; i < al; i++) {
			ASSERT(ary.elem(i).type() == rpc::datatype::BLOB);
			a[i].len = ary.elem(i).len();
			memcpy(a[i].buf, ary.elem(i), a[i].len);
		}
		dmp = d.elem(1);
		ASSERT(dmp.type() == rpc::datatype::MAP && dmp.size() <= 4);
		ml = dmp.size();
		for (int i = 0; i < ml; i++) {
			ASSERT(dmp.key(i).type() == rpc::datatype::BLOB);
			ASSERT(dmp.val(i).type() == rpc::datatype::BLOB);
			strcpy(m[i].key, dmp.key(i));
			memcpy(m[i].val, dmp.val(i), dmp.val(i).len());
		}
		dec = d.elem(2);
		b = d.elem(3);
		u64 = d.elem(4);
		return sr.len();
	}
	void fill() {
		memset(this, 0, sizeof(*this));
		al = (nbr_rand32() % 3);
		for (int i = 0; i < al; i++) {
			a[i].len = (nbr_rand32() % sizeof(a[i].buf));
			rand_buffer(a[i].buf, a[i].len);
		}
		ml = (nbr_rand32() % 4);
		for (int i = 0; i < ml; i++) {
			rand_string(m[i].key, sizeof(m[i].key));
			rand_buffer(m[i].val, sizeof(m[i].val));
		}
		dec = (double)nbr_rand64();
		b = (nbr_rand32() % 2);
		u64 = nbr_rand64();
	}
	bool operator != (const test_object &o) { 
		return !(operator == (o));
	}
	bool operator == (const test_object &o) {
		if (al != o.al || ml != o.ml) { 
			TTRACE("record unmatch (%p)\n", this);
			return false; 
		}
		for (int i = 0; i < al; i++) {
			if (a[i].len != o.a[i].len) { 
				TTRACE("record unmatch (%p)\n", this);
				return false; 
			}
			if (memcmp(a[i].buf, o.a[i].buf, a[i].len) != 0) {
				TTRACE("record unmatch (%p)\n", this);
				return false;
			}
		}
		for (int i = 0; i < ml; i++) {
			if (memcmp(m[i].key, o.m[i].key, sizeof(m[i].key)) != 0 ||
				memcmp(m[i].val, o.m[i].val, sizeof(m[i].val)) != 0) {
				TTRACE("record unmatch (%p)\n", this);
				return false;
			}
		}
		return (u64 == o.u64 && dec == o.dec && b == o.b);
			
	}
};

int dbm_test(int argc, char *argv[])
{
	dbm db;
	char path[1024];
	const char *dbmopt = get_rcpath(path, sizeof(path), 
		argv[0], "rc/dbm/test.tch#bnum=1000000");
	if (db.init(dbmopt) < 0) {
		TTRACE("initialize fail with (%s)", dbmopt);
		return NBR_ESYSCALL;
	}
	char key[256], val[256]; 
	int kl, vl;
	for (int i = 0; i < 10000; i++) {
		kl = snprintf(key, sizeof(key), "%08x", i);
		vl = snprintf(val, sizeof(val), "%08x%08x%08x", i, i, i);
		if (!db.replace(key, kl, val, vl)) {
			TTRACE("replace fail at %u\n", i);
			return NBR_ESYSCALL;
		}
	}
	void *p;
	for (int i = 0; i < 10000; i++) {
		snprintf(key, sizeof(key), "%08x", i);
		snprintf(val, sizeof(val), "%08x%08x%08x", i, i, i);
		if (!(p = db.select(key, kl, vl)) || (memcmp(p, val, vl) != 0)) {
			TTRACE("select fail at %u\n", i);
			return NBR_ESYSCALL;
		}
	}
	db.fin();
	test_object	tests[1001], *pto;
	pmap<test_object, char[9]>	pm;
	const char *dbmopt2 = get_rcpath(path, sizeof(path), 
			argv[0], "rc/dbm/to.tch#bnum=1000");
	if (!pm.init(1000, 100, 0, dbmopt2)) {
		TTRACE("initialize fail with (%s)\n", dbmopt2);
		return NBR_ESYSCALL;
	}
	pm.clear();
	for (int i = 0; i < 1001; i++) {
		snprintf(key, sizeof(key), "%08x", i);
		tests[i].fill();
		if (!(pto = pm.insert(tests[i], key)) || (*pto != tests[i])) {
			TTRACE("pmap::create fail at %u\n", i);
			return NBR_EMALLOC;
		}
		pm.unload(key);
	}
	if (pm.cachesize() != 0) {
		TTRACE("pmap: unload fail? (%d)\n", pm.cachesize());
		return NBR_EINVAL;
	}
	for (int i = 0; i < 1000; i++) {
		snprintf(key, sizeof(key), "%08x", i);
		if (!(pto = pm.load(key)) || (*pto != tests[i])) {
			TTRACE("pmap::load fail at %u\n", i);
			return NBR_EINVAL;
		}
	}
	snprintf(key, sizeof(key), "%08x", 1000);
	if (pm.load(key)) {
		TTRACE("pmap::cache should expire (%s)\n", key);
		return NBR_EINVAL;
	}
	if (pm.rnum() != 1001 || pm.cachesize() != 1000) {
		TTRACE("pmap::cache not works (%d/%d)\n", pm.rnum(), pm.cachesize());
		return NBR_EINVAL;
	}
	pm.fin();
	if (!pm.init(1000, 100, 0, dbmopt2)) {
		TTRACE("pmap:: reopen db fail with (%s)\n", dbmopt2);
		return NBR_ESYSCALL;
	}
	for (int i = 0; i < 1000; i++) {
		snprintf(key, sizeof(key), "%08x", i);
		if ((pto = pm.insert(tests[i], key))) {
			TTRACE("pmap::insert should fail if once created (%d)\n", i);
			return NBR_EALREADY;
		}
		if (!(pto = pm.load(key))) {
			TTRACE("pmap::load should success (%d)\n", i);
			return NBR_ENOTFOUND;
		} 
	}
	return NBR_OK;
}

