#if !defined(__MAC_H__)
#define __MAC_H__

#include "nbr.h"
#include <memory.h>
#include "dbm.h"

namespace pfm {
class mac_uuid {
public:
	U8      mac[6];
	U16     id1;
	U32     id2;
	static const int strkeylen = 24 + 1;
	static const int disaster_addid = 1000000;
public:
	bool operator == (const mac_uuid &uuid) const {
			U32 *p = (U32 *)&uuid, *q = (U32 *)this;
			return p[0] == q[0] && p[1] == q[1] && p[2] == q[2];
	}
	const mac_uuid &operator = (const mac_uuid &uuid) {
			U32 *p = (U32 *)&uuid, *q = (U32 *)this;
			q[0] = p[0]; q[1] = p[1]; q[2] = p[2];
			return *this;
	}
	const char *to_s(char *b, int bl) const {
			const U32 *p = (const U32 *)this;
			snprintf(b, bl, "%08x%08x%08x", p[0], p[1], p[2]);
			return b;
	}
public:
	mac_uuid() { U32 *p = (U32 *)this; p[0] = p[1] = p[2] = 0L; }
	~mac_uuid() {}
	static int init(dbm &db);
	static void fin(dbm &db);
	static int save(dbm &db);
	static int load(dbm &db);
	static void new_id(mac_uuid &uuid);
	static inline const mac_uuid invalid_id();
	static inline bool valid(const mac_uuid &uuid) {
		return uuid.id1 != 0 || uuid.id2 != 0; }
};

inline const mac_uuid
mac_uuid::invalid_id()
{
	return mac_uuid();
}
}

#endif

