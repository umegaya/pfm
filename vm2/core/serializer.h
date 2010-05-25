#if !defined(__SERIALIZER_H__)
#define __SERIALIZER_H__

#include "mp.h"
namespace pfm {
typedef mp serializer_impl;
class serializer : public serializer_impl {};
namespace rpc {
namespace datatype {
static const U32 NIL = serializer::NIL;
static const U32 BOOLEAN = serializer::BOOLEAN;
static const U32 ARRAY = serializer::ARRAY;
static const U32 MAP = serializer::MAP;
static const U32 BLOB = serializer::BLOB;
static const U32 DOUBLE = serializer::DOUBLE;
static const U32 INTEGER = serializer::INTEGER;
static const U32 STRING = serializer::STRING;
}
}
}

#endif
