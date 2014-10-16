#ifndef BLAT_H
#define BLAT_H

#include "buf.h"

#ifndef true
    #define true                (1==1)
    #define false               (1==0)
#endif
#ifndef TRUE
    #define TRUE                true
    #define FALSE               false
#endif

#define BLAT_LITE               FALSE
#define BASE_SMTP_ONLY          FALSE

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

#if SUPPORT_GSSAPI  //Added 2003-11-07 Joseph Calzaretta
    #define SUBMISSION_PORT     FALSE   //  Change to TRUE if you want the default port for GSSAPI to be Submission (587)
                                        //  Keep it FALSE if you want the default port for GSSAPI to be SMTP (25)
    #define MECHTYPE            "%MECHTYPE%"
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
    #define WIN_32_STR "Win64"
#else
    #define WIN_32_STR "Win32"
#endif

#define BOUNDARY_MARKER     "=_BlatBoundary-"

typedef struct _BLDHDRS {
    Buf  * messageBuffer;
    Buf  * header;
    Buf  * varHeaders;
    Buf  * multipartHdrs;
    int    buildSMTP;
    Buf  * lpszFirstReceivedData;
    Buf  * lpszOtherHeader;
    char * attachment_boundary;
    char * multipartID;
    char * wanted_hostname;
    char * server_name;
    int    part;
    int    totalparts;  // if multipart, this is part x of y, else is 0.
    int    attachNbr;
    int    nbrOfAttachments;
    char * attachName;
    DWORD  attachSize;
    int    addBccHeader;
} BLDHDRS;


typedef struct BLATOPTIONS {
    char * optionString;    // What string defines this option
    char * szCgiEntry;      // This is the CGI option name
    int    preprocess;      // Does this option need to be preprocessed?
    int    additionArgC;    // Minimum number of arguments for this option
    int (* initFunction)( int argc, char ** argv, int this_arg, int startargv );
    char * usageText;
} _BLATOPTIONS;

#endif
