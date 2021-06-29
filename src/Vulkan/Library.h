#pragma once
#include <evol/common/ev_macros.h>
#include <evol/common/ev_types.h>

typedef GenericHandle LibraryHandle;
#define INVALID_LIBRARY_HANDLE (~0ull)

#define Library(T) struct {           \
    Map(evstring, LibraryHandle) map; \
    vec(T) list;                      \
    bool dirty;                       \
  }

HashmapDefine(evstring, LibraryHandle, evstring_free, NULL)

#define LibraryInit(...) EV_CONCAT(__LibraryInitImpl_, EV_VA_ARGS_NARG(__VA_ARGS__))(__VA_ARGS__)
#define __LibraryInitImpl_2(p_l, T)          __LibraryInitImpl_4(p_l, T, NULL, NULL)
#define __LibraryInitImpl_3(p_l, T, T_destr) __LibraryInitImpl_4(p_l, T, NULL, T_destr)
#define __LibraryInitImpl_4(p_l, T, T_cpy, T_destr) do {     \
  ((Library(T)*)p_l)->map = Hashmap(evstring, LibraryHandle).new(); \
  ((Library(T)*)p_l)->list = vec_init(T, T_cpy, T_destr);           \
  ((Library(T)*)p_l)->dirty = false;                                \
} while (0)

#define LibraryDestroy(l) do {                  \
  Hashmap(evstring, LibraryHandle).free(l.map); \
  vec_fini(l.list);                             \
} while (0)

