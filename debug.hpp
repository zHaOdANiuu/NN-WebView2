#ifdef DEBUG
#  include <cassert>
#  define DEBUG_ASSERT(condition, msg) assert((condition) && (msg))
#endif

#ifndef DEBUG_ASSERT
#  define DEBUG_ASSERT(condition, msg) ((void)0)
#endif
