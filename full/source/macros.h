#ifndef __BLAT_MACROS_H__
#define __BLAT_MACROS_H__ 1

#if defined(_UNICODE) || defined(UNICODE)
#define _MY_FUNCTION_NAME   __FUNCTIONW__
#else
#define _MY_FUNCTION_NAME   __FUNCTION__
#endif

#ifndef FUNCTION_ENTRY
 #if INCLUDE_SUPERDEBUG && defined(_WIN64)
  #define FUNCTION_ENTRY() { \
        if ( CommonData.superDuperDebug == TRUE ) { \
            _TCHAR savedQuiet = CommonData.quiet; \
            CommonData.quiet = FALSE; \
            printMsg( CommonData, __T("superDuperDebug: %s %s()\n"), __T("entering"), _MY_FUNCTION_NAME ); \
            CommonData.quiet = savedQuiet; \
        } \
    }
 #else
 #define FUNCTION_ENTRY() {}
 #endif
#endif

#ifndef FUNCTION_EXIT
 #if INCLUDE_SUPERDEBUG && defined(_WIN64)
  #define FUNCTION_EXIT() { \
        if ( CommonData.superDuperDebug == TRUE ) { \
            _TCHAR savedQuiet = CommonData.quiet; \
            CommonData.quiet = FALSE; \
            printMsg( CommonData, __T("superDuperDebug: %s %s()\n"), __T("leaving"), _MY_FUNCTION_NAME ); \
            CommonData.quiet = savedQuiet; \
        } \
    }
 #else
 #define FUNCTION_EXIT() {}
 #endif
#endif

#endif      // #if !defined __BLAT_MACROS__
