/* Project-supplied translation unit (not upstream): the amalgamation is C99, and
   MSVC rejects two of its compound literals in C++ mode, so compile it as C. Its
   public API is already extern "C" when included from C++. */
#define PURISM_CORE_IMPLEMENTATION
#define PSM_COMPAT_VERSION 0x05010000L
#define PSM_NO_STDIO 1
#include "PurismCoreBundle.h"
