#pragma once

#ifndef VA_EXPAND
#define VA_EXPAND(...) __VA_ARGS__
#endif

#ifdef __cplusplus
#define PRELOADER_MAYBE_UNUSED [[maybe_unused]]
#else
#define PRELOADER_MAYBE_UNUSED
#endif

#define PL_SHARED_EXPORT                                                       \
  PRELOADER_MAYBE_UNUSED __attribute__((visibility("default")))

#ifdef PRELOADER_EXPORT
#define PLAPI PL_SHARED_EXPORT
#else
#define PLAPI PRELOADER_MAYBE_UNUSED
#endif

#ifdef __cplusplus
#define PLCAPI extern "C" PLAPI
#else
#define PLCAPI extern PLAPI
#endif
