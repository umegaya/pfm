#include "serializer.h"
#include "object.h"
#include "proto.h"
#include "ll.h"

#if !defined(_OLD_OBJECT)
int pfm::rpc::world_request::pack_object(
		pfm::serializer &sr, const UUID &uuid, const char *klass)
{
	return pfm::ll::coroutine::pack_object(sr, uuid, klass);
}
#else
int pfm::rpc::world_request::pack_object(
		pfm::serializer &sr, pfm::object &o)
{
	return pfm::ll::coroutine::pack_object(sr, o); 
}
#endif
