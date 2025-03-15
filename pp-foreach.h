#pragma once

#define __PARENS ()

#define __EXPAND(...) __EXPAND4(__EXPAND4(__EXPAND4(__EXPAND4(__VA_ARGS__))))
#define __EXPAND4(...) __EXPAND3(__EXPAND3(__EXPAND3(__EXPAND3(__VA_ARGS__))))
#define __EXPAND3(...) __EXPAND2(__EXPAND2(__EXPAND2(__EXPAND2(__VA_ARGS__))))
#define __EXPAND2(...) __EXPAND1(__EXPAND1(__EXPAND1(__EXPAND1(__VA_ARGS__))))
#define __EXPAND1(...) __VA_ARGS__

#define FOR_EACH(macro, ...) \
    __VA_OPT__(__EXPAND(__FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define __FOR_EACH_HELPER(macro, a1, ...) \
    macro(a1)                             \
        __VA_OPT__(__FOR_EACH_AGAIN __PARENS(macro, __VA_ARGS__))
#define __FOR_EACH_AGAIN() __FOR_EACH_HELPER