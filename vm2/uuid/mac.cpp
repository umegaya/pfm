#include "mac.h"
#include "dbm.h"

using namespace pfm;

static mac_uuid UUID_INVALID;
static mac_uuid UUID_SEED;

int mac_uuid::init(dbm &db) 
{
	int r;
	if ((r = load(db)) < 0) {
		if (r != NBR_ENOTFOUND) {
			return r;	/* not found means need initialize */
		}
		if ((r = nbr_osdep_get_macaddr("eth0", UUID_SEED.mac)) < 0) {
			return r;
		}
	}
	char buf[256];
	fprintf(stderr,"UUID>current seed %s\n", UUID_SEED.to_s(buf, sizeof(buf)));
	return NBR_OK;
}

void mac_uuid::fin(dbm &db)
{
	save(db);
	int flag = 0;
	db.replace("_flag_", 6, (void *)&flag, sizeof(flag));
}

void
mac_uuid::new_id(mac_uuid &uuid)
{
	if (0 == (uuid.id2 = __sync_add_and_fetch(&(UUID_SEED.id2), 1))) {
		uuid.id1 = UUID_SEED.id1++;
	}
}

int
mac_uuid::save(dbm &db)
{
	return db.replace("_uuid_", 6, &UUID_SEED, sizeof(UUID_SEED)) < 0 ? 
		NBR_ESYSCALL : NBR_OK;
}

int
mac_uuid::load(dbm &db)
{
	int seedl, flagl;
	void *seed = db.select("_uuid_", 6, seedl);
	if (!seed) {
		return NBR_ENOTFOUND;	/* initial. */
	}
	if (seedl != sizeof(UUID_SEED)) {
		return NBR_EINVAL;
	}
	void *f = db.select("_flag_", 6, flagl);
	if (flagl != sizeof(int)) {
		return NBR_EINVAL;
	}
	UUID_SEED = *((mac_uuid *)seed);
	nbr_free(seed);
	if (*((int *)f)) {
		nbr_free(f);
		/* disaster recovery : add 1M to seed */
		if (((U64)UUID_SEED.id2 + disaster_addid) > 0x00000000FFFFFFFF) {
			UUID_SEED.id1++;
		}
		UUID_SEED.id2 += disaster_addid;
		if (save(db) < 0) {
			return NBR_ESYSCALL;
		}
	}
	flagl = 1;
	db.replace("_flag_", 6, (void *)&flagl, sizeof(flagl));
	return NBR_OK;
}

