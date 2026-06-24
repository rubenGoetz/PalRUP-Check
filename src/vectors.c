
#include "utils/palrup_utils.h"  // for u64, u8
#include "drup_clause.h"
#include "drup_check.h"

#define TYPE u8
#define TYPED(THING) u8_ ## THING
#include "vec.c"
#undef TYPED
#undef TYPE

#define TYPE signed char
#define TYPED(THING) i8_ ## THING
#include "vec.c"
#undef TYPED
#undef TYPE

#define TYPE int
#define TYPED(THING) int_ ## THING
#include "vec.c"
#undef TYPED
#undef TYPE

#define TYPE unsigned
#define TYPED(THING) unsigned_ ## THING
#include "vec.c"
#undef TYPED
#undef TYPE

#define TYPE u64
#define TYPED(THING) u64_ ## THING
#include "vec.c"
#undef TYPED
#undef TYPE

#define TYPE drup_clause
#define TYPED(THING) drup_clause_##THING
#include "vec.c"
#undef TYPED
#undef TYPE

#define TYPE watcher
#define TYPED(THING) watcher_##THING
#include "vec.c"
#undef TYPED
#undef TYPE

