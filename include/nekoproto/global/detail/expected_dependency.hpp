#pragma once
#include <nekoproto/global/config.h>
#include <version>

#ifndef NEKO_PROTO_USE_STD_EXPECTED
    #if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
        #define NEKO_PROTO_USE_STD_EXPECTED 1
    #else
        #define NEKO_PROTO_USE_STD_EXPECTED 0
    #endif
#endif

#if NEKO_PROTO_USE_STD_EXPECTED
    #include <expected>
#else
    #include <zeus/expected.hpp>
#endif
