#if !defined __COMMON_DATA_H__
#define __COMMON_DATA_H__ 1

#include "gensock.h"
#include "connections.h"
#if SUPPORT_GSSAPI
#include "gssfuncs.h" // Please read the comments here for information about how to use GssSession
#endif

typedef struct NODES {
    NODES * next;
    LPTSTR  attachmentName;
    int     fileType;
    DWORD   fileSize;
} _NODES;

typedef struct _COMMON_DATA
{
#if INCLUDE_SUPERDEBUG
    _TCHAR        superDebug;
#endif
    _TCHAR        Profile[TRY_SIZE+1];
    _TCHAR        priority[2];
    _TCHAR        sensitivity[2];

    _TCHAR        impersonating;
    _TCHAR        ssubject;
    int           maxNames;

    _TCHAR        boundaryPosted;
    _TCHAR        commentChar;
    _TCHAR        disposition;
    _TCHAR        includeUserAgent;
    _TCHAR        sendUndisclosed;
    _TCHAR        disableMPS;
#if SUPPORT_MULTIPART
    DWORD         multipartSize;
#endif
    _TCHAR        needBoundary;
    _TCHAR        returnreceipt;
    DWORD         maxMessageSize;
    _TCHAR        loginAuthSupported;
    _TCHAR        plainAuthSupported;
    _TCHAR        cramMd5AuthSupported;
    unsigned int  uuencodeBytesLine;
    _TCHAR        forcedHeaderEncoding;
    _TCHAR        noheader;
    _TCHAR        ByPassCRAM_MD5;
    _TCHAR        commandPipelining;
    _TCHAR        enhancedCodeSupport;

    int           delayBetweenMsgs;

#if INCLUDE_POP3
    _TCHAR        POP3Host[SERVER_SIZE+1];
    _TCHAR        POP3Port[SERVER_SIZE+1];
    Buf           POP3Login;
    Buf           POP3Password;
    _TCHAR        xtnd_xmit_wanted;
    _TCHAR        xtnd_xmit_supported;
#endif
#if INCLUDE_IMAP
    _TCHAR        IMAPHost[SERVER_SIZE+1];
    _TCHAR        IMAPPort[SERVER_SIZE+1];
    Buf           IMAPLogin;
    Buf           IMAPPassword;
#endif

    _TCHAR        SMTPHost[SERVER_SIZE+1];
    _TCHAR        SMTPPort[SERVER_SIZE+1];

#if INCLUDE_NNTP
    _TCHAR        NNTPHost[SERVER_SIZE+1];
    _TCHAR        NNTPPort[SERVER_SIZE+1];
    Buf           groups;
#endif

    Buf           AUTHLogin;
    Buf           AUTHPassword;
    _TCHAR        Try[TRY_SIZE+1];
    Buf           Sender;
    Buf           TempConsole;
    Buf           Recipients;
    Buf           destination;
    Buf           cc_list;
    Buf           bcc_list;
    _TCHAR        loginname   [SENDER_SIZE+1];   // RFC 821 MAIL From. <loginname>. There are some inconsistencies in usage
    _TCHAR        senderid    [SENDER_SIZE+1];   // Inconsistent use in Blat for some RFC 822 Field definitions
    _TCHAR        sendername  [SENDER_SIZE+1];   // RFC 822 Sender: <sendername>
    _TCHAR        fromid      [SENDER_SIZE+1];   // RFC 822 From: <fromid>
    _TCHAR        replytoid   [SENDER_SIZE+1];   // RFC 822 Reply-To: <replytoid>
    _TCHAR        returnpathid[SENDER_SIZE+1];   // RFC 822 Return-Path: <returnpath>
    Buf           subject;
    Buf           alternateText;
    _TCHAR        clearLogFirst;
    _TCHAR        logFile[_MAX_PATH];
    int           attachFoundFault;
    int           globaltimeout;
    Buf           processedOptions;

    NODES       * attachList;

//
// ---------------------------------------------------------------------------
// global variables (shared by all DLL users)

    connection_list global_socket_list;
    int           network_initialized;

#if SUPPORT_GSSAPI  //Added 2003-11-07 Joseph Calzaretta
    _TCHAR        gssapiAuthSupported;
    BOOL          authgssapi;
    BOOL          mutualauth;
    BOOL          bSuppressGssOptionsAtRuntime;
    _TCHAR        servicename[SERVICENAME_SIZE];
    protLev       gss_protection_level;
    GssSession *  pGss;
    BOOL          have_mechtype;
    _TCHAR        mechtype[MECHTYPE_SIZE];
    GssSession    TheSession;
#endif

#if SUPPORT_POSTSCRIPTS
    Buf           postscript;
#endif
#if SUPPORT_TAGLINES
    Buf           tagline;
#endif
#if SUPPORT_SALUTATIONS
    Buf           salutation;
#endif
#if SUPPORT_SIGNATURES
    Buf           signature;
#endif
#if BLAT_LITE
#else
    Buf           organization;
    Buf           xheaders;
    Buf           aheaders1;
    Buf           aheaders2;
    _TCHAR        uuencode;                      // by default Blat does not use UUEncode // Added by Gilles Vollant

    _TCHAR        base64;                        // by default Blat does not use base64 Quoted-Printable Content-Transfer-Encoding!
                                                    //  If you're looking for something to do, then it would be nice if this thing
                                                    //  detected any non-printable characters in the input, and use base64 whenever
                                                    //  quoted-printable wasn't chosen by the user.

    _TCHAR        yEnc;
    _TCHAR        deliveryStatusRequested;
    _TCHAR        deliveryStatusSupported;

    _TCHAR        eightBitMimeSupported;
    _TCHAR        eightBitMimeRequested;
    _TCHAR        binaryMimeSupported;
    //_TCHAR  binaryMimeRequested;

    _TCHAR        optionsFile[_MAX_PATH];
    FILE *        optsFile;
    Buf           userContentType;
#endif

    _TCHAR        chunkingSupported;
    int           utf;

    _TCHAR        textmode[TEXTMODE_SIZE+1];     // added 15 June 1999 by James Greene "greene@gucc.org"
    _TCHAR        bodyFilename[_MAX_PATH+1];
    Buf           bodyparameter;
    _TCHAR        ConsoleDone;
    _TCHAR        formattedContent;
    _TCHAR        mime;                          // by default Blat does not use mime Quoted-Printable Content-Transfer-Encoding!
    _TCHAR        quiet;
    _TCHAR        debug;
    _TCHAR        haveEmbedded;
    _TCHAR        haveAttachments;
    int           attach;
    int           regerr;
    _TCHAR        bodyconvert;

    int           exitRequired;

    _TCHAR        attachfile[64][MAX_PATH+1];
    _TCHAR        my_hostname_wanted[1024];
    FILE *        logOut;
    int           fCgiWork;
    _TCHAR        charset[40];                   // Added 25 Apr 2001 Tim Charron (default ISO-8859-1)

    _TCHAR        attachtype[64];
    _TCHAR        timestamp;

    int           delimiterPrinted;
    _TCHAR        lastByteSent;

#if defined(WIN32) || defined(WIN64)        /* This is for NT */
    HANDLE        dll_module_handle;
#else                                       /* This is for WIN16 */
    HINSTANCE     dll_module_handle;
#endif
    HMODULE       gensock_lib;

    socktag       ServerSocket;

#define MY_HOSTNAME_SIZE    1024
    _TCHAR        my_hostname[MY_HOSTNAME_SIZE];

    int           titleLinePrinted;
    int           usagePrinted;
    int           logCommands;

} COMMON_DATA;

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
    int (* initFunction)( COMMON_DATA & CommonData, int argc, LPTSTR* argv, int this_arg, int startargv );
    LPTSTR usageText;
} _BLATOPTIONS;

#pragma pack()

#endif      // #if !defined __COMMON_DATA_H__
