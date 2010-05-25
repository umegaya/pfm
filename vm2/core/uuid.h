#if !defined(__UUID_H__)
#define __UUID_H__

#include "mac.h"
namespace pfm {
typedef mac_uuid UUID_impl;
class UUID : public UUID_impl {
public:
	void assign() { UUID::new_id(*this); }
};
}

#endif
