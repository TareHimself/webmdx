#pragma once
#if defined(WEBMDX_STATIC)
  #define WEBMDX_API
#elif defined(_WIN32)
  #ifdef WEBM_DX_PRODUCER
    #define WEBMDX_API __declspec(dllexport)
  #else
    #define WEBMDX_API __declspec(dllimport)
  #endif
#else
  #define WEBMDX_API __attribute__((visibility("default")))
#endif
