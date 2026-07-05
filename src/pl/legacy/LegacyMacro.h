#pragma once

#ifdef __cplusplus
#define PL_LEGACY_MAYBE_UNUSED [[maybe_unused]]
#else
#define PL_LEGACY_MAYBE_UNUSED
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PL_LEGACY_EXPORT                                                       \
  PL_LEGACY_MAYBE_UNUSED __attribute__((visibility("default")))
#else
#define PL_LEGACY_EXPORT PL_LEGACY_MAYBE_UNUSED
#endif

