#if !defined(__UUID_H__)
#define __UUID_H__

#include "mac.h"
namespace pfm {
typedef mac_uuid UUID_impl;
class UUID : public UUID_impl {
public:
	UUID() : UUID_impl() {}
	UUID(const UUID_impl &uuid) { *((UUID_impl *)this) = uuid; }
	void assign() { UUID_impl::new_id(*this); }
	bool valid() const { return UUID_impl::valid(*(UUID_impl *)this); }
	static inline const UUID invalid_id() { return (const UUID)UUID_impl::invalid_id(); }
};
}

#endif
