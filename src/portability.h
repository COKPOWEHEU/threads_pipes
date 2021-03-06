/*
 *  Linux
 */
#include <inttypes.h>
#if defined(linux) || defined(__linux) || defined(__linux__) || defined(__GNU__) || defined(__GLIBC__)
  #ifndef linux
    #define linux 1 //чтобы была всего одна константа, а не десяток
  #endif
  //Linux
  #include <dlfcn.h>
  #define DynLoad(s) dlopen(s, RTLD_LAZY)
  #define DynFunc(lib, name)  dlsym(lib,name)
  #define DynClose(lib) dlclose(lib)
  #define pusleep(usec) usleep(usec)
/*
 *  Win 32
 */
#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
  #ifndef WIN32
    #define WIN32 1
  #endif
  //Win32
  #define _CRT_SECURE_NO_WARNINGS
  #include <windows.h>
  
  #define DynLoad(s) LoadLibrary(s)
  #define DynFunc(lib, name)   GetProcAddress((HINSTANCE)lib, name)
  #define DynClose(lib) FreeLibrary(lib)
  static inline void pusleep(uint64_t usec){
    HANDLE timer; 
    LARGE_INTEGER ft; 
    ft.QuadPart = -(10*usec); // Convert to 100 nanosecond interval, negative value indicates relative time
    timer = CreateWaitableTimer(NULL, TRUE, NULL); 
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0); 
    WaitForSingleObject(timer, INFINITE); 
    CloseHandle(timer); 
  }
 /*
  *  Other systems (unsupported)
  */
#else
  #error "Unsupported platform"
#endif
