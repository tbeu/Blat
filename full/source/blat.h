#ifndef BLAT_H
#define BLAT_H

#if !defined(is64Bit)
#define is64Bit     0
#endif

#if !defined(__WATCOMC__) && is64Bit && !defined(_WIN64)
    #define _WIN64
#endif
#if !defined(__WATCOMC__) && is64Bit && !defined(WIN64)
    #define WIN64
#endif

#include "buf.h"

#ifndef true
    #define true                (1==1)
    #define false               (1==0)
#endif
#ifndef TRUE
    #define TRUE                true
    #define FALSE               false
#endif

#if !defined(BLAT_LITE)
#define BLAT_LITE               FALSE
#endif
#if !defined(BASE_SMTP_ONLY)
#define BASE_SMTP_ONLY          FALSE
#endif

#if BLAT_LITE || BASE_SMTP_ONLY
    #define INCLUDE_NNTP        FALSE   // Must be FALSE for the compiler to work and deliver
    #define INCLUDE_POP3        FALSE   // Must be FALSE for the compiler to work and deliver
    #define SUPPORT_YENC        FALSE   // the desired Blat Lite results.
    #define SUPPORT_MULTIPART   FALSE   //
    #define SUPPORT_SIGNATURES  FALSE
    #define SUPPORT_TAGLINES    FALSE
    #define SUPPORT_POSTSCRIPTS FALSE
    #define SUPPORT_SALUTATIONS FALSE
    #define SMART_CONTENT_TYPE  FALSE
    #define INCLUDE_SUPERDEBUG  FALSE
    #define SUPPORT_GSSAPI      FALSE
    #define INCLUDE_IMAP        FALSE
#else

    #define INCLUDE_NNTP        TRUE    // Change to FALSE if you do not want NNTP support
    #define INCLUDE_POP3        TRUE    // Change to FALSE if you do not want POP3 support
    #define SUPPORT_YENC        FALSE
    #define SUPPORT_MULTIPART   TRUE
    #define SUPPORT_SIGNATURES  TRUE
    #define SUPPORT_TAGLINES    TRUE
    #define SUPPORT_POSTSCRIPTS TRUE
    #define SUPPORT_SALUTATIONS TRUE
    #define SMART_CONTENT_TYPE  TRUE
    #define INCLUDE_SUPERDEBUG  TRUE
    #define SUPPORT_GSSAPI      TRUE    // Change to FALSE if you do not want GSSAPI support
    #define INCLUDE_IMAP        TRUE
#endif

//#ifdef _WIN64
//    #undef  SUPPORT_GSSAPI
//    #define SUPPORT_GSSAPI      FALSE
//#endif

#if SUPPORT_GSSAPI  //Added 2003-11-07 Joseph Calzaretta
    #define SUBMISSION_PORT     FALSE   //  Change to TRUE if you want the default port for GSSAPI to be Submission (587)
                                        //  Keep it FALSE if you want the default port for GSSAPI to be SMTP (25)
    #define MECHTYPE            __T("%MECHTYPE%")
    #define MECHTYPE_SIZE       1024
    #define SERVICENAME_SIZE    1024
#endif
#define USAGE_LINE_SIZE         1024

#define MAXOUTLINE              2048

#define SERVER_SIZE             256     // #defines (bleah!) from Beverly Brown "beverly@datacube.com"
#define SENDER_SIZE             256
#define TRY_SIZE                256
#define SUBJECT_SIZE            1024
#define DEST_SIZE               1024
#define ORG_SIZE                512
#define TEXTMODE_SIZE           16      // added 15 June 1999 by James Greene "greene@gucc.org"

#define EMBED_ATTACHMENT        1
#define BINARY_ATTACHMENT       2
#define TEXT_ATTACHMENT         3
#define INLINE_ATTACHMENT       4
#define EMBED_TEXT_ATTACHMENT       (0x40 | EMBED_ATTACHMENT )
#define BINARY_TEXT_ATTACHMENT      (0x40 | BINARY_ATTACHMENT)
#define TEXT_TEXT_ATTACHMENT        (0x40 | TEXT_ATTACHMENT  )
#define INLINE_TEXT_ATTACHMENT      (0x40 | INLINE_ATTACHMENT)
#define EMBED_MESSAGE_ATTACHMENT    (0x80 | EMBED_ATTACHMENT )
#define BINARY_MESSAGE_ATTACHMENT   (0x80 | BINARY_ATTACHMENT)
#define TEXT_MESSAGE_ATTACHMENT     (0x80 | TEXT_ATTACHMENT  )
#define INLINE_MESSAGE_ATTACHMENT   (0x80 | INLINE_ATTACHMENT)

#define NATIVE_32BIT_UTF        0x04
#define NON_NATIVE_32BIT_UTF    0x84
#define NATIVE_16BIT_UTF        0x02
#define NON_NATIVE_16BIT_UTF    0x82
#define UTF_REQUESTED           1

#define DSN_NEVER               (1 << 0)
#define DSN_SUCCESS             (1 << 1)
#define DSN_FAILURE             (1 << 2)
#define DSN_DELAYED             (1 << 3)

#ifdef WIN32
    #define __far far
    #define huge far
    #define __near near
#endif

#ifndef _MAX_PATH
    #define _MAX_PATH 260
#endif

#ifdef _WIN64
    #define WIN_32_STR              __T("Win64 (AMD64)")
#else
    #define WIN_32_STR              __T("Win32")
#endif

#define BOUNDARY_MARKER             __T("=_BlatBoundary-")

#if defined(_UNICODE) || defined(UNICODE)
    #define _TCHAR_PRINTF_FORMAT    __T("h")
#else
    #define _TCHAR_PRINTF_FORMAT    __T("hh")
#endif

#pragma pack(1)

typedef struct _BLDHDRS {
    Buf  * messageBuffer;
    Buf  * header;
    Buf  * varHeaders;
    Buf  * multipartHdrs;
    int    buildSMTP;
    Buf  * lpszFirstReceivedData;
    Buf  * lpszOtherHeader;
    LPTSTR attachment_boundary;
    LPTSTR multipartID;
    LPTSTR wanted_hostname;
    LPTSTR server_name;
    int    part;
    int    totalparts;  // if multipart, this is part x of y, else is 0.
    int    attachNbr;
    int    nbrOfAttachments;
    LPTSTR attachName;
    DWORD  attachSize;
    int    addBccHeader;
} BLDHDRS;


typedef struct BLATOPTIONS {
    LPTSTR optionString;    // What string defines this option
    LPTSTR szCgiEntry;      // This is the CGI option name
    int    preprocess;      // Does this option need to be preprocessed?
    int    additionArgC;    // Minimum number of arguments for this option
    int (* initFunction)( int argc, LPTSTR* argv, int this_arg, int startargv );
    LPTSTR usageText;
} _BLATOPTIONS;

#pragma pack()

#if !defined(_tstol)
 #define _tstol      _ttol      // fix for older VC
#endif

#if !defined(_tstoi)
 #define _tstoi      _ttoi      // fix for older VC
#endif

#ifdef BLATDLL_EXPORTS // this is blat.dll, not blat.exe
// this is to be used with blat.wcx in Total Commander (www.ghisler.com)
// additional typedef for SetProcessDataProc
    /* Notify that data is processed - used for progress dialog */
    typedef int (__stdcall *tProcessDataProc)(char *FileName, int Size);
    typedef int (__stdcall *tProcessDataProcW)(wchar_t *FileName, int Size);
#endif

#endif
