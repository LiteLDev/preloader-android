#pragma once

#ifdef __cplusplus
#define PRELOADER_MAYBE_UNUSED [[maybe_unused]]
#else
#define PRELOADER_MAYBE_UNUSED
#endif

#ifdef PRELOADER_EXPORT
#define PLAPI PRELOADER_MAYBE_UNUSED __declspec(dllexport)
#else
#define PLAPI PRELOADER_MAYBE_UNUSED __declspec(dllimport)
#endif

#ifdef __cplusplus
#define PLCAPI extern "C" PLAPI
#else
#define PLCAPI extern PLAPI
#endif