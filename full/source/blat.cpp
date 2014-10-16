/*
    blat.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
//#include <vld.h>

#include "blat.h"
#include "winfile.h"

#define VERSION_SUFFIX  __T("")

/* generic socket DLL support */
#include "gensock.h"
#if SUPPORT_GSSAPI
#include "gssfuncs.h" // Please read the comments here for information about how to use GssSession
//#undef  VERSION_SUFFIX
//#define VERSION_SUFFIX  __T(" w/GSS encryption")
#else
#endif


#define BLAT_VERSION    __T("3.0.6")
// Major revision level      *      Update this when a major change occurs, such as a complete rewrite.
// Minor revision level        *    Update this when the user experience changes, such as when new options/features are added.
// Bug   revision level          *  Update this when bugs are fixed, but no other user experience changes.

#ifdef __cplusplus
extern "C" int _tmain( int argc, LPTSTR *argv, LPTSTR *envp );
#endif

extern BOOL DoCgiWork(int &argc, LPTSTR* &argv,   Buf &lpszMessage,
                      Buf &lpszCgiSuccessUrl,     Buf &lpszCgiFailureUrl,
                      Buf &lpszFirstReceivedData, Buf &lpszOtherHeader);
extern int  collectAttachmentInfo ( DWORD & totalsize, size_t msgBodySize );
extern void releaseAttachmentInfo ( void );

extern void searchReplaceEmailKeyword (Buf & email_addresses);
extern int  send_email( size_t msgBodySize, Buf &lpszFirstReceivedData, Buf &lpszOtherHeader,
                        LPTSTR attachment_boundary, LPTSTR multipartID,
                        int nbrOfAttachments, DWORD totalsize );

#if INCLUDE_NNTP
extern int  send_news( size_t msgBodySize, Buf &lpszFirstReceivedData, Buf &lpszOtherHeader,
                       LPTSTR attachment_boundary, LPTSTR multipartID,
                       int nbrOfAttachments, DWORD totalsize );
#endif
extern int  GetRegEntry( LPTSTR pstrProfile );

extern void printTitleLine( int quiet );
extern int  printUsage( int optionPtr );
extern int  processOptions( int argc, LPTSTR * argv, int startargv, int preprocessing );

extern size_t make_argv( LPTSTR arglist,                /* argument list                     */
                         LPTSTR*static_argv,            /* pointer to argv to use            */
                         size_t max_static_entries,     /* maximum number of entries allowed */
                         size_t starting_entry,         /* entry in argv to begin placing    */
                         int    from_dll );             /* blat called as .dll               */

void printMsg(LPTSTR p, ... );              // Added 23 Aug 2000 Craig Morrison

extern void convertPackedUnicodeToUTF( Buf & sourceText, Buf & outputText, int * utf, LPTSTR charset, int utfRequested );
extern void convertUnicode( Buf &sourceText, int * utf, LPTSTR charset, int utfRequested );
#if defined(_UNICODE) || defined(UNICODE)
void checkInputForUnicode ( Buf & stringToCheck );
#endif

#if INCLUDE_SUPERDEBUG
extern _TCHAR        superDebug;
#endif
extern _TCHAR        my_hostname[];
extern _TCHAR        Profile[TRY_SIZE+1];
extern LPTSTR        defaultSMTPPort;
#if INCLUDE_NNTP
extern LPTSTR        defaultNNTPPort;
#endif
extern _TCHAR        priority[2];

extern _TCHAR        impersonating;
extern _TCHAR        ssubject;
extern int           maxNames;

extern _TCHAR        boundaryPosted;
extern _TCHAR        commentChar;
extern _TCHAR        disposition;
extern HANDLE        dll_module_handle;
extern _TCHAR        includeUserAgent;
extern _TCHAR        sendUndisclosed;
#if SUPPORT_MULTIPART
extern unsigned long multipartSize;
#endif
extern _TCHAR        needBoundary;
extern _TCHAR        returnreceipt;
#ifdef BLAT_LITE
#else
extern _TCHAR        forcedHeaderEncoding;
extern _TCHAR        noheader;
extern unsigned int  uuencodeBytesLine;
extern unsigned long maxMessageSize;
extern _TCHAR        loginAuthSupported;
extern _TCHAR        plainAuthSupported;
extern _TCHAR        cramMd5AuthSupported;
extern _TCHAR        ByPassCRAM_MD5;
#endif
#if SUPPORT_GSSAPI
extern _TCHAR        gssapiAuthSupported;
#endif

int     delayBetweenMsgs;

_TCHAR  blatVersion[]    = BLAT_VERSION;
_TCHAR  blatVersionSuf[] = VERSION_SUFFIX;
_TCHAR  blatBuildDate[64];
_TCHAR  blatBuildTime[64];

char    blatBuildDateA[] = __DATE__;
char    blatBuildTimeA[] = __TIME__;

/*
How to check the Microsoft compiler version, from stackoverflow.com

http://stackoverflow.com/questions/70013/how-to-detect-if-im-compiling-code-under-visual-studio-8

Some values for the more recent versions of the compiler are:

MSVC++ 11.0 _MSC_VER = 1700 (Visual Studio 2011)
MSVC++ 10.0 _MSC_VER = 1600 (Visual Studio 2010)
MSVC++ 9.0  _MSC_VER = 1500 (Visual Studio 2008)
MSVC++ 8.0  _MSC_VER = 1400 (Visual Studio 2005)
MSVC++ 7.1  _MSC_VER = 1310 (Visual Studio 2003)
MSVC++ 7.0  _MSC_VER = 1300
MSVC++ 6.0  _MSC_VER = 1200
MSVC++ 5.0  _MSC_VER = 1100
 */
#if (defined(_UNICODE) || defined(UNICODE)) && defined(_MSC_VER) && (_MSC_VER >= 1400)
_TCHAR  fileCreateAttribute[] = __T("w, ccs=UTF-8");
_TCHAR  fileAppendAttribute[] = __T("a, ccs=UTF-8");
#else
_TCHAR  fileCreateAttribute[] = __T("w");
_TCHAR  fileAppendAttribute[] = __T("a");
#endif

#if INCLUDE_POP3
_TCHAR  POP3Host[SERVER_SIZE+1];
_TCHAR  POP3Port[SERVER_SIZE+1];
Buf     POP3Login;
Buf     POP3Password;
_TCHAR  xtnd_xmit_wanted;
_TCHAR  xtnd_xmit_supported;
#endif
#if INCLUDE_IMAP
_TCHAR  IMAPHost[SERVER_SIZE+1];
_TCHAR  IMAPPort[SERVER_SIZE+1];
Buf     IMAPLogin;
Buf     IMAPPassword;
#endif

_TCHAR  SMTPHost[SERVER_SIZE+1];
_TCHAR  SMTPPort[SERVER_SIZE+1];

#if INCLUDE_NNTP
_TCHAR  NNTPHost[SERVER_SIZE+1];
_TCHAR  NNTPPort[SERVER_SIZE+1];
Buf     groups;
#endif

Buf     AUTHLogin;
Buf     AUTHPassword;
_TCHAR  Try[TRY_SIZE+1];
Buf     Sender;
Buf     TempConsole;
Buf     Recipients;
Buf     destination;
Buf     cc_list;
Buf     bcc_list;
_TCHAR  loginname[SENDER_SIZE+1];       // RFC 821 MAIL From. <loginname>. There are some inconsistencies in usage
_TCHAR  senderid[SENDER_SIZE+1];        // Inconsistent use in Blat for some RFC 822 Field definitions
_TCHAR  sendername[SENDER_SIZE+1];      // RFC 822 Sender: <sendername>
_TCHAR  fromid[SENDER_SIZE+1];          // RFC 822 From: <fromid>
_TCHAR  replytoid[SENDER_SIZE+1];       // RFC 822 Reply-To: <replytoid>
_TCHAR  returnpathid[SENDER_SIZE+1];    // RFC 822 Return-Path: <returnpath>
Buf     subject;
Buf     alternateText;
_TCHAR  clearLogFirst;
_TCHAR  logFile[_MAX_PATH];
int     attachFoundFault;

#if SUPPORT_GSSAPI  //Added 2003-11-07 Joseph Calzaretta
BOOL    authgssapi;
BOOL    mutualauth;
BOOL    bSuppressGssOptionsAtRuntime;
_TCHAR  servicename[SERVICENAME_SIZE];
protLev gss_protection_level;
GssSession* pGss;
#endif

#if SUPPORT_POSTSCRIPTS
Buf     postscript;
#endif
#if SUPPORT_TAGLINES
Buf     tagline;
#endif
#if SUPPORT_SALUTATIONS
Buf     salutation;
#endif
#if SUPPORT_SIGNATURES
Buf     signature;
#endif
#if BLAT_LITE
#else
_TCHAR  organization[ORG_SIZE+1];
_TCHAR  xheaders[DEST_SIZE+1];
_TCHAR  aheaders1[DEST_SIZE+1];
_TCHAR  aheaders2[DEST_SIZE+1];
_TCHAR  uuencode;                       // by default Blat does not use UUEncode // Added by Gilles Vollant

    // by default Blat does not use base64 Quoted-Printable Content-Transfer-Encoding!
    //  If you're looking for something to do, then it would be nice if this thing
    //  detected any non-printable characters in the input, and use base64 whenever
    //  quoted-printable wasn't chosen by the user.
_TCHAR  base64;

_TCHAR  yEnc;
_TCHAR  deliveryStatusRequested;
_TCHAR  deliveryStatusSupported;

_TCHAR  eightBitMimeSupported;
_TCHAR  eightBitMimeRequested;
_TCHAR  binaryMimeSupported;
//_TCHAR  binaryMimeRequested;

_TCHAR  optionsFile[_MAX_PATH];
FILE *  optsFile;
Buf     userContentType;
#endif

_TCHAR  chunkingSupported;
int     utf;

_TCHAR  textmode[TEXTMODE_SIZE+1];      // added 15 June 1999 by James Greene "greene@gucc.org"
_TCHAR  bodyFilename[_MAX_PATH+1];
Buf     bodyparameter;
_TCHAR  ConsoleDone;
_TCHAR  formattedContent;
_TCHAR  mime;                           // by default Blat does not use mime Quoted-Printable Content-Transfer-Encoding!
_TCHAR  quiet;
_TCHAR  debug;
_TCHAR  haveEmbedded;
_TCHAR  haveAttachments;
int     attach;
int     regerr;
_TCHAR  bodyconvert;

int     exitRequired;

_TCHAR  attachfile[64][MAX_PATH+1];
_TCHAR  my_hostname_wanted[1024];
FILE *  logOut;
int     fCgiWork;
_TCHAR  charset[40];                    // Added 25 Apr 2001 Tim Charron (default ISO-8859-1)

_TCHAR  attachtype[64];
_TCHAR  timestamp;

LPTSTR stdinFileName     = __T("stdin.txt");
LPTSTR defaultCharset    = __T("ISO-8859-1");
LPTSTR days[]            = { __T("Sun"),
                             __T("Mon"),
                             __T("Tue"),
                             __T("Wed"),
                             __T("Thu"),
                             __T("Fri"),
                             __T("Sat") };


static void cleanUpBuffers( void )
{
    TempConsole.Free();
    alternateText.Free();
    bodyparameter.Free();
    bcc_list.Free();
    cc_list.Free();
    destination.Free();
    Recipients.Free();
    Sender.Free();
    subject.Free();
    AUTHLogin.Free();
    AUTHPassword.Free();
#if INCLUDE_IMAP
    IMAPLogin.Free();
    IMAPPassword.Free();
#endif
#if INCLUDE_POP3
    POP3Login.Free();
    POP3Password.Free();
#endif
#if INCLUDE_NNTP
    groups.Free();
#endif
#if SUPPORT_SALUTATIONS
    salutation.Free();
#endif
#if SUPPORT_SIGNATURES
    signature.Free();
#endif
#if SUPPORT_TAGLINES
    tagline.Free();
#endif
#if SUPPORT_POSTSCRIPTS
    postscript.Free();
#endif
}


static void shrinkWhiteSpace( Buf & buffer )
{
    LPTSTR pp;
    _TCHAR prevChar;
    Buf    newBuffer;

    pp = buffer.Get();
    if ( pp ) {
        prevChar = 0;
        for ( ; *pp; pp++ ) {
            if ( *pp == __T(' ') )
                if ( !prevChar || (prevChar == __T(' ')) )
                    continue;

            newBuffer.Add( prevChar = *pp );
        }

        if ( prevChar == __T(' ') )
            newBuffer.Remove();

        buffer = newBuffer;
    }
    newBuffer.Free();
}


int _tmain( int argc,             /* Number of strings in array argv          */
            LPTSTR *argv,         /* Array of command-line argument strings   */
            LPTSTR *envp )        /* Array of environment variables           */
{
    int      i, j;
    int      retcode;
    WinFile  fileh;
    _TCHAR   boundary1[24];
    Buf      lpszMessageCgi;
    Buf      lpszCgiFailureUrl;
    Buf      lpszCgiSuccessUrl;
    Buf      lpszFirstReceivedData;
    Buf      lpszOtherHeader;
#if BLAT_LITE
#else
    LPTSTR * secondArgV;
    int      secondArgC;
#endif
    Buf      temp;
    DWORD    filesize;
    DWORD    totalsize; // total size of all attachments and the message body.
    int      nbrOfAttachments;
    _TCHAR   multipartID[1200];
#if defined(_UNICODE) || defined(UNICODE)
  #if BLAT_LITE
    _TCHAR   savedMime;
  #else
    _TCHAR   savedEightBitMimeRequested;
  #endif
    int      savedUTF;
    Buf      sourceText;

    DWORD dwVersion = GetVersion();
    if ( dwVersion & (0x80ul << ((sizeof(DWORD)-1) * 8)) ) {
        fprintf( stderr, "This Unicode version of Blat cannot run in Windows earlier than Windows 2000.\n" \
                         "Please download and use Blat from the Win98 download folder on Yahoo! groups\n" \
                         "at http://tech.groups.yahoo.com/group/blat/files/Official/32%20bit%20versions/\n" );
        exit(14);
    }

    for ( i = 0; ; ) {
        blatBuildDate[i] = (_TCHAR)blatBuildDateA[i];
        if ( !blatBuildDateA[i++] )
            break;
    }
    for ( i = 0; ; ) {
        blatBuildTime[i] = (_TCHAR)blatBuildTimeA[i];
        if ( !blatBuildTimeA[i++] )
            break;
    }

 #if 1
    for ( i = 1; i < argc; i++ ) {
        sourceText = argv[i];
        utf = 0;
        charset[0] = __T('\0');
        checkInputForUnicode( sourceText );
        if ( utf ) {
            LPTSTR pString;
            size_t oldLength;
            size_t newLength;

            pString = sourceText.Get();
            if ( *pString == 0xFEFF )
               pString++;

            oldLength = _tcslen( argv[i] );
            newLength = _tcslen( pString );

            /* if the new Unicode string is shorter or equal length to input string */
            if ( newLength <= oldLength )
                memcpy( argv[i], pString, (newLength + 1) * sizeof(_TCHAR) );
            else
            /* oldLength < newLength, which should not happen. */
            if ( _tcscmp( argv[i], pString ) != 0 ) {
                LPTSTR pArgv;

                pArgv = (LPTSTR) malloc( (newLength + 1) * sizeof(_TCHAR) );
                if ( pArgv ) {
                    memcpy( pArgv, pString, (newLength + 1) * sizeof(_TCHAR) );
                    free( argv[i] );
                    argv[i] = pArgv;
                }
            }
        }
    #if 01
        _tprintf( __T("argv[%2i] = '%s'\n"), i, argv[i] );
    #endif
        sourceText.Free();
    }
  #endif
#else
    _tcscpy( blatBuildDate, blatBuildDateA );
    _tcscpy( blatBuildTime, blatBuildTimeA );
#endif

    envp = envp;    // To remove compiler warnings.

    // Initialize global variables.
    cleanUpBuffers();

    commentChar = __T(';');
    Profile[0] = __T('\0');
    boundaryPosted = 0;
    delayBetweenMsgs = 0;
    disposition = 0;
    dll_module_handle = 0;
    exitRequired = FALSE;
#if SUPPORT_GSSAPI
    gssapiAuthSupported = FALSE;
#endif
    impersonating = 0;
    includeUserAgent = 0;
    sendUndisclosed = 0;
    maxNames = 0;
#if SUPPORT_MULTIPART
    multipartSize = 0;
#endif
    my_hostname_wanted[0] = __T('\0');
    needBoundary = 0;
    priority[0] = __T('\0');
    returnreceipt = 0;
#ifdef BLAT_LITE
#else
    forcedHeaderEncoding = 0;
    noheader = 0;
    uuencodeBytesLine = 45;
    maxMessageSize = 0;
    loginAuthSupported = 0;
    plainAuthSupported = 0;
    cramMd5AuthSupported = 0;
    ByPassCRAM_MD5 = FALSE;
#endif

    fCgiWork = FALSE;

    memset( attachtype, 0, sizeof(attachtype) );
    memset( attachfile, 0, sizeof(attachfile) );

#if INCLUDE_NNTP
    _tcscpy(NNTPPort, defaultNNTPPort);
#endif
    _tcscpy(SMTPPort, defaultSMTPPort);
    _tcscpy(Try,      __T("1"));                      // Was ONCE

    exitRequired            = FALSE;

#if SUPPORT_GSSAPI  //Added 2003-11-07 Joseph Calzaretta
    authgssapi              =
    mutualauth              =
    bSuppressGssOptionsAtRuntime = FALSE;
    gss_protection_level = GSSAUTH_P_PRIVACY;
#endif

#if BLAT_LITE
#else
    userContentType.Free();

    binaryMimeSupported     = 0;
    deliveryStatusRequested = 0;
    deliveryStatusSupported =
    uuencode                =         // by default Blat does not use UUEncode // Added by Gilles Vollant
    base64                  =
    yEnc                    =
    eightBitMimeSupported   =
    eightBitMimeRequested   =
//    binaryMimeRequested     =
#endif
    chunkingSupported       =
    ConsoleDone             =
    mime                    =           // by default Blat does not use mime Quoted-Printable Content-Transfer-Encoding!
    quiet                   =           // by default Blat is very noisy!
    debug                   =           // by default Blat is very noisy!
#if INCLUDE_SUPERDEBUG
    superDebug              =
#endif
#if INCLUDE_POP3
    xtnd_xmit_wanted        =
    xtnd_xmit_supported     =
#endif
    timestamp               =
    haveEmbedded            = FALSE;
    haveAttachments         = FALSE;
    attach                  = 0;
    utf                     = 0;
    bodyconvert             = TRUE;
    formattedContent        = TRUE;
    logOut                  = (FILE *)NULL;


#if SUPPORT_GSSAPI  //Added 2003-11-07 Joseph Calzaretta
    servicename[0]          =
#endif
#if INCLUDE_POP3
    POP3Host[0]             =
    POP3Port[0]             = __T('\0');
    POP3Login.Clear();
    POP3Password.Clear();
#endif
#if INCLUDE_IMAP
    IMAPHost[0]             =
    IMAPPort[0]             = __T('\0');
    IMAPLogin.Clear();
    IMAPPassword.Clear();
#endif
#if INCLUDE_NNTP
    NNTPHost[0]             =
#endif
#if BLAT_LITE
#else
    organization[0]         =
    xheaders[0]             =
    aheaders1[0]            =
    aheaders2[0]            =
#endif
    SMTPHost[0]             = __T('\0');
    Sender.Clear();
    my_hostname_wanted[0]   =
    my_hostname[0]          =
    loginname[0]            =           // RFC 821 MAIL From: <loginname>
    senderid[0]             =           // Inconsistent use in Blat for some RFC 822 Field definitions
    sendername[0]           =           // RFC 822 Sender: <sendername>
    fromid[0]               =           // RFC 822 From: <fromid>
    replytoid[0]            =           // RFC 822 Reply-To: <replytoid>
    returnpathid[0]         =           // RFC 822 Return-Path: <returnpath>subject[0] = '\0';
    bodyFilename[0]         =
    logFile[0]              = __T('\0');
    subject.Clear();
    AUTHLogin.Clear();
    AUTHPassword.Clear();

#if SUPPORT_GSSAPI
    // Check to see if the GSSAPI library is present by trying to initialize the global GssSession object.
    try
    {
        static GssSession TheSession;
        pGss = &TheSession;
    }
    catch (GssSession::GssNoLibrary&) // If no library present, disallow the AUTH GSSAPI options
    {
        bSuppressGssOptionsAtRuntime = TRUE;
        pGss = NULL;
    }
    catch (GssSession::GssException&) // Silently fail if any other GssException shows up.  Only complain later
        // if someone actually tries to use AUTH GSSAPI.
    {
        pGss = NULL;
    }
#endif

    if ( argc <= 2 ) {
#ifndef DEBUGCGI
        _TCHAR c;
        if ( (GetEnvironmentVariable(__T("REQUEST_METHOD"),   &c,1)>0) &&
             (GetEnvironmentVariable(__T("GATEWAY_INTERFACE"),&c,1)>0) )
#endif
        {
            if ( DoCgiWork(argc,argv,lpszMessageCgi,lpszCgiSuccessUrl,
                           lpszCgiFailureUrl,lpszFirstReceivedData,
                           lpszOtherHeader) ) {
                quiet    = TRUE;
                fCgiWork = TRUE;
            }
        }
    }
/*
    else {
        int x;

        tprintf( __T("\nBlat saw the following command line:\n") );
        for ( x = 0; x < argc; x++ ) {
            if ( x )
                tprintf( __T(" ") );

            if ( _tcschr(argv[x], __T(' ')) || _tcschr(argv[x], __T('"')) )
                tprintf( __T("\"%s\""), argv[x] );
            else
                tprintf( __T("%s"), argv[x] );
        }
        tprintf( __T("\n\n") );
    }
 */

    charset[0] = __T('\0');

    // attach -- Added in by Tim Charron (tcharron@interlog.com)
    // If "-attach filename" is on the command line at least once,
    // then those files will be attached as base64 encoded files.
    // This variable is the count of how many of these files there are.
    attach = 0;

    // -ss  -- Added in by Arthur Donchey (adonchey@vpga.com)
    // if no subject is defined and -ss is stated, then no subject line is added to header
    ssubject = FALSE;                                                            /*$$ASD*/

    // -enriched -- Added 10. June 1999 by James Greene (greene@gucc.org)
    // -html     -- Added 15. June 1999 by James Greene (greene@gucc.org)
    // if "-enriched" is on the command line, assume the text is written
    // with enriched text markers, e.g. <bold><center><flushright>
    // <FontFamily> etc.
    // if "-html" is on the command line, assume the text is well-formed HTML
    // otherwise, assume plain text

    _tcscpy(textmode, __T("plain"));

    if ( argc < 2 ) {
//        Must have at least a file name to send.
        printUsage( NULL );
        return(1);
    }

#if BLAT_LITE
#else

    secondArgV = NULL;
    secondArgC = 0;
    memset( optionsFile, 0, sizeof(optionsFile) );
#endif
    if ( ((argv[1][0] == __T('-')) || (argv[1][0] == __T('/'))) && argv[1][1] )
        retcode = processOptions( argc, argv, 1, TRUE );        // Preprocess the options
    else
        retcode = processOptions( argc, argv, 2, TRUE );        // Preprocess the options

    // If the -install or -profile option was used,
    // then exitRequired should be TRUE while retcode is 0.
    if ( !exitRequired && !fCgiWork )
        printTitleLine( quiet );

    if ( exitRequired || retcode ) {
        printMsg( NULL );
        if ( logOut )
            fclose(logOut);

        cleanUpBuffers();
        return(retcode);
    }

#if BLAT_LITE
#else
    if ( optionsFile[0] ) {

        size_t maxEntries = 256;

        secondArgV = (LPTSTR*)malloc( (maxEntries + 1) * sizeof(LPTSTR) );
        if ( secondArgV ) {
            WinFile fileh;
            DWORD   filesize;
            DWORD   dummy;
            LPTSTR  tmpstr;
            size_t  nextEntry = 0;
            LPTSTR  pChar;

            memset( secondArgV, 0, (maxEntries + 1) * sizeof(LPTSTR) );
            if ( !fileh.OpenThisFile(optionsFile) ) {
                free( secondArgV );
                printMsg( __T("Options file \"%s\" not found or could not be opened.\n"), optionsFile );

                printMsg( NULL );
                if ( logOut )
                    fclose(logOut);

                cleanUpBuffers();
                return(2);
            }
            filesize = fileh.GetSize();
            tmpstr = (LPTSTR)malloc( (filesize + 1)*sizeof(_TCHAR) );
            if ( !tmpstr ) {
                fileh.Close();
                printMsg( __T("error allocating memory for reading %s, aborting\n"), optionsFile );
                if ( logOut )
                    fclose(logOut);

                cleanUpBuffers();
                return(2);
            }

            if ( !fileh.ReadThisFile(tmpstr, filesize, &dummy, NULL) ) {
                fileh.Close();
                free(tmpstr);
                printMsg( __T("error reading %s, aborting\n"), optionsFile );
                if ( logOut )
                    fclose(logOut);

                cleanUpBuffers();
                return(2);
            }
            fileh.Close();

            tmpstr[filesize] = __T('\0');

  #if defined(_UNICODE) || defined(UNICODE)
            savedEightBitMimeRequested = eightBitMimeRequested;
            eightBitMimeRequested = 0;

            savedUTF = utf;
            utf = 0;
            sourceText.Clear();
            sourceText.Add( tmpstr, filesize );
            checkInputForUnicode( sourceText );
            free( tmpstr );

            tmpstr = (LPTSTR)malloc( (sourceText.Length() + 1)*sizeof(_TCHAR) );
            if ( !tmpstr ) {
                sourceText.Free();
                printMsg( __T("error allocating memory for reading %s, aborting\n"), optionsFile );
                if ( logOut )
                    fclose(logOut);

                cleanUpBuffers();
                return(2);
            }
            memcpy( tmpstr, sourceText.Get(), sourceText.Length()*sizeof(_TCHAR) );
            tmpstr[sourceText.Length()] = __T('\0');
            if ( tmpstr[0] == 0xFEFF )
                _tcscpy( &tmpstr[0], &tmpstr[1] );

            utf = savedUTF;
            eightBitMimeRequested = savedEightBitMimeRequested;
  #endif
            nextEntry = make_argv( tmpstr,      /* argument list                     */
                                   secondArgV,  /* pointer to argv to use            */
                                   maxEntries,  /* maximum number of entries allowed */
                                   nextEntry,
                                   FALSE );
            secondArgC = (int)nextEntry;
        }
    }

    if ( secondArgC ) {
        retcode = processOptions( secondArgC, secondArgV, 0, TRUE );
        if ( exitRequired || retcode ) {
            for ( i = 0; secondArgV[ i ]; i++) {
                free( secondArgV[ i ] );
            }

            free( secondArgV );
            if ( retcode ) {
                printMsg( NULL );
                if ( logOut )
                    fclose(logOut);
            }
            cleanUpBuffers();
            return(retcode);
        }
    }
#endif

    // get file name from argv[1]
    if ( (argv[1][0] != __T('-')) && (argv[1][0] != __T('/')) ) {
        _tcsncpy( bodyFilename, argv[1], _MAX_PATH );
        bodyFilename[_MAX_PATH] = __T('\0');
    }

    regerr = GetRegEntry( Profile );

    _tcscpy(senderid,  Sender.Get());
    _tcscpy(loginname, Sender.Get());

#if 01
    LARGE_INTEGER ticksPerSecond;
    LARGE_INTEGER tick;   // A point in time
    //LARGE_INTEGER time;   // For converting tick into real time

    // get the high resolution counter's accuracy
    QueryPerformanceFrequency(&ticksPerSecond);
    if ( ticksPerSecond.QuadPart )
        QueryPerformanceCounter(&tick);
    else
        tick.u.LowPart = GetTickCount();

    srand( (unsigned int)tick.QuadPart );
#else
    srand( (unsigned int) time( NULL ) + (unsigned int) clock() );
#endif

#if BLAT_LITE
#else
    if ( secondArgC ) {
        retcode = processOptions( secondArgC, secondArgV, 0, FALSE );

        for ( i = 0; secondArgV[ i ]; i++) {
            free( secondArgV[ i ] );
        }

        free( secondArgV );
        if ( exitRequired || retcode ) {
            printMsg( NULL );
            if ( logOut )
                fclose(logOut);

            cleanUpBuffers();
            return(retcode);
        }
    }
#endif

    if ( ((argv[1][0] == __T('-')) || (argv[1][0] == __T('/'))) && argv[1][1] )
        retcode = processOptions( argc, argv, 1, FALSE );
    else
        retcode = processOptions( argc, argv, 2, FALSE );

    // If the -install or -profile option was used,
    // then exitRequired should be TRUE while retcode is 0.
    if ( exitRequired || retcode ) {
        printMsg( NULL );
        if ( logOut )
            fclose(logOut);

        cleanUpBuffers();
        return(retcode);
    }

#if INCLUDE_NNTP
    if ( regerr == 12 )
        if ( !loginname[0] || (!SMTPHost[0] && !NNTPHost[0]) )
            printMsg( __T("Failed to open registry key for Blat\n") );
#else
    if ( regerr == 12 )
        if ( !loginname[0] || !SMTPHost[0] )
            printMsg( __T("Failed to open registry key for Blat\n") );
#endif

    // if we are not impersonating loginname is the same as the sender
    if ( ! impersonating )
        _tcscpy(senderid, loginname);

    // fixing the argument parsing
    // Ends here

#if INCLUDE_NNTP
    if ( !loginname[0] || (!SMTPHost[0] && !NNTPHost[0]) ) {
        printMsg( __T("To set the SMTP server's name/address and your username/email address for that\n") \
                  __T("server machine do:\n") \
                  __T("blat -install  server_name  your_email_address\n") \
                  __T("or use '-server <server_name>' and '-f <your_email_address>'\n") );
        printMsg( __T("aborting, nothing sent\n") );

        printMsg( NULL );
        if ( logOut )
            fclose(logOut);

        cleanUpBuffers();
        return(12);
    }
#else
    if ( !loginname[0] || !SMTPHost[0] ) {
        printMsg( __T("To set the SMTP server's name/address and your username/email address for that\n") \
                  __T("server machine do:\n") \
                  __T("blat -install  server_name  your_email_address\n") \
                  __T("or use '-server <server_name>' and '-f <your_email_address>'\n") );
        printMsg( __T("aborting, nothing sent\n") );

        printMsg( NULL );
        if ( logOut )
            fclose(logOut);

        cleanUpBuffers();
        return(12);
    }
#endif

    // make sure filename exists, get full pathname
    if ( bodyFilename[0] && (_tcscmp(bodyFilename, __T("-")) != 0) ) {
        int useCreateFile;
        OSVERSIONINFOEX osvi;

        memset(&osvi, 0, sizeof(OSVERSIONINFOEX));

        // Try calling GetVersionEx using the OSVERSIONINFOEX structure.
        // If that fails, try using the OSVERSIONINFO structure.

        useCreateFile = FALSE;
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

        if ( !GetVersionEx ((OSVERSIONINFO *) &osvi) ) {
            osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
            GetVersionEx ( (OSVERSIONINFO *) &osvi);
        }

        if ( osvi.dwPlatformId == VER_PLATFORM_WIN32_NT ) {
            if ( osvi.dwMajorVersion >= 5 )
                useCreateFile = TRUE;
        }

        if ( useCreateFile ) {  // For Windows 2000 and newer.
            HANDLE fh;

            fh = CreateFile( bodyFilename, FILE_READ_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
            if ( fh == INVALID_HANDLE_VALUE ) {
                DWORD lastError = GetLastError();
                if ( lastError )
                    printMsg(__T("unknown error code %lu when trying to open %s\n"), lastError, bodyFilename);
                else
                    printMsg(__T("%s does not exist\n"), bodyFilename);

                printMsg( NULL );
                if ( logOut )
                    fclose(logOut);

                cleanUpBuffers();
                return(2);
            }
            CloseHandle(fh);
        } else {                        // Windows 95 through NT 4.0
            FILE * fh;

            fh = _tfopen(bodyFilename, __T("r"));
            if ( fh == NULL ) {
                printMsg(__T("%s does not exist\n"),bodyFilename);

                printMsg( NULL );
                if ( logOut )
                    fclose(logOut);

                cleanUpBuffers();
                return(2);
            }
            fclose(fh);
        }
    }

    shrinkWhiteSpace( bcc_list );
    shrinkWhiteSpace( cc_list );
    shrinkWhiteSpace( destination );

    searchReplaceEmailKeyword( bcc_list );
    searchReplaceEmailKeyword( cc_list );
    searchReplaceEmailKeyword( destination );

    // build temporary recipients list for parsing the "To:" line
    // build the recipients list
    temp.Alloc(       destination.Length() + cc_list.Length() + bcc_list.Length() + 4 );
    Recipients.Alloc( destination.Length() + cc_list.Length() + bcc_list.Length() + 4 );

    // Parse the "To:" line
    for ( i = j = 0; (unsigned)i < destination.Length(); i++ ) {
        // strip white space
        // NOT! while ( destination[i] == __T(' ') )   i++;
        // look for comments in brackets, and omit
        if ( destination.Get()[i] == __T('(') ) {
            while ( destination.Get()[i] != __T(')') )   i++;
            i++;
        }
        temp.Get()[j++] = destination.Get()[i];
    }
    temp.Get()[j] = __T('\0');                              // End of list added!
    Recipients.Add(temp.Get());

    // Parse the "Cc:" line
    for ( i = j = 0; (unsigned)i < cc_list.Length(); i++ ) {
        // strip white space
        // NOT! while ( cc_list[i] == __T(' ') ) i++;
        // look for comments in brackets, and omit
        if ( cc_list.Get()[i] == __T('(') ) {
            while ( cc_list.Get()[i] != __T(')') ) i++;
            i++;
        }
        temp.Get()[j++] = cc_list.Get()[i];
    }
    temp.Get()[j] = __T('\0');                              // End of list added!
    if ( cc_list.Length() ) {
        if ( Recipients.Length() )
            Recipients.Add( __T(',') );

        Recipients.Add(temp.Get());
    }

    // Parse the "Bcc:" line
    for (i = j = 0; (unsigned)i < bcc_list.Length(); i++) {
        // strip white space
        // NOT! while ( bcc_list[i] == __T(' ') )  i++;
        // look for comments in brackets, and omit
        if (bcc_list.Get()[i] == __T('(')) {
            while (bcc_list.Get()[i] != __T(')')) i++;
            i++;
        }
        temp.Get()[j++] = bcc_list.Get()[i];
    }
    temp.Get()[j] = __T('\0');                              // End of list added!
    if ( bcc_list.Length() > 0 ) {
        if ( Recipients.Length() )
            Recipients.Add( __T(',') );

        Recipients.Add(temp.Get());
    }

#if INCLUDE_NNTP
    if ( !Recipients.Length() && !groups.Length() ) {
        printMsg( __T("No target email address or newsgroup was specified.  You must give an email\n") \
                  __T("address or usenet newsgroup to send messages to.  Use -to, -cc, or -bcc option\n") \
                  __T("for email, or -groups for usenet.\n") \
                  __T("Aborting, nobody to send messages to.\n") );

        printMsg( NULL );
        if ( logOut )
            fclose(logOut);

        cleanUpBuffers();
        return(12);
    }
#else
    if ( !Recipients.Length() ) {
        printMsg( __T("No target email address was specified.  You must give an email address\n") \
                  __T("to send messages to.  Use -to, -cc, or -bcc option.\n") \
                  __T("Aborting, nobody to send messages to.\n") );

        printMsg( NULL );
        if ( logOut )
            fclose(logOut);

        cleanUpBuffers();
        return(12);
    }
#endif

    // if reading from the console, read everything into a temporary file first
    ConsoleDone = FALSE;
    if ( (bodyFilename[0] == __T('\0')) || (_tcscmp(bodyFilename, __T("-")) == 0) ) {

        if ( lpszMessageCgi.Length() ) {
            ConsoleDone = TRUE;
            TempConsole.Add(lpszMessageCgi);
        } else
        if ( bodyparameter.Length() ) {
            LPTSTR p = bodyparameter.Get();
            ConsoleDone = TRUE;
            if (bodyconvert) {
                i = 0;
                while ( p[i] ) {
                    if ( p[i] == __T('|') )      // CRLF signified by the pipe character
                        TempConsole.Add( __T("\r\n") );
                    else
                        TempConsole.Add( p[i] );
                    i++;
                }
            } else
                TempConsole.Add( p );
        } else {
            ConsoleDone = TRUE;
            for ( ; ; ) {
                DWORD dwNbRead = 0;
                i=0;

                if ( !ReadFile(GetStdHandle(STD_INPUT_HANDLE),&i,1,&dwNbRead,NULL) ||
                     !dwNbRead || (i == __T('\x1A')) )
                    break;

                TempConsole.Add((char)i);
            }
        }
        _tcscpy(bodyFilename, stdinFileName);
    }

    if (!ConsoleDone) {
        DWORD dummy;

        //get the text of the file into a string buffer
        if ( !fileh.OpenThisFile(bodyFilename) ) {
            printMsg(__T("error reading %s, aborting\n"),bodyFilename);

            printMsg( NULL );
            if ( logOut )
                fclose(logOut);

            cleanUpBuffers();
            return(3);
        }
        if ( !fileh.IsDiskFile() ) {
            fileh.Close();
            printMsg(__T("Sorry, I can only mail messages from disk files...\n"));

            printMsg( NULL );
            if ( logOut )
                fclose(logOut);

            cleanUpBuffers();
            return(4);
        }
        filesize = fileh.GetSize();
        TempConsole.Clear();
        if ( filesize ) {
            TempConsole.Alloc( filesize + 1 );
            retcode = fileh.ReadThisFile(TempConsole.Get(), filesize, &dummy, NULL);
            TempConsole.SetLength( filesize );
            *TempConsole.GetTail() = __T('\0');
        } else {
            filesize = 1;
            TempConsole = __T("\n");
            retcode = true;
        }
        fileh.Close();

        if ( !retcode ) {
            printMsg(__T("error reading %s, aborting\n"), bodyFilename);
            cleanUpBuffers();
            return(5);
        }
    }

#if defined(_UNICODE) || defined(UNICODE)
  #if BLAT_LITE
    savedMime = mime;
  #else
    savedEightBitMimeRequested = eightBitMimeRequested;
    eightBitMimeRequested = 0;
  #endif

    savedUTF = utf;
    utf = 0;
    checkInputForUnicode ( TempConsole );
    if ( utf == UTF_REQUESTED ) {
        if ( charset[0] == __T('\0') )
            _tcscpy( charset, __T("utf-8") );   // Set to lowercase to distinguish between our determination and user specified.
    } else {
  #if BLAT_LITE
        mime = savedMime;
  #else
        eightBitMimeRequested = savedEightBitMimeRequested;
  #endif
        utf = savedUTF;
    }
#endif
    filesize = (DWORD)TempConsole.Length();

    attachFoundFault = FALSE;
    nbrOfAttachments = collectAttachmentInfo( totalsize, filesize );
    if ( attachFoundFault ) {
        printMsg( __T("One or more attachments had not been found.\n") \
                  __T("Aborting, so you find / fix the missing attachment.\n") );

        printMsg( NULL );
        if ( logOut )
            fclose(logOut);

        cleanUpBuffers();
        return(12);
    }

    if ( nbrOfAttachments && !totalsize ) {
        printMsg( __T("Sum total size of all attachments exceeds 4G bytes.  This is too much to be\n") \
                  __T("sending with SMTP or NNTP.  Please try sending through FTP instead.\n") \
                  __T("Aborting, too much data to send.\n") );

        printMsg( NULL );
        if ( logOut )
            fclose(logOut);

        cleanUpBuffers();
        return(12);
    }

#if BLAT_LITE
#else
    if ( base64 )
        formattedContent = TRUE;
#endif

    // supply the message body size, in case of sending multipart messages.
    retcode = send_email( filesize, lpszFirstReceivedData, lpszOtherHeader,
                          boundary1, multipartID, nbrOfAttachments, totalsize );
#if INCLUDE_NNTP
    int ret;

    // supply the message body size, in case of sending multipart messages.
    ret = send_news ( filesize, lpszFirstReceivedData, lpszOtherHeader,
                      boundary1, multipartID, nbrOfAttachments, totalsize );
    if ( !retcode )
        retcode = ret;
#endif
    releaseAttachmentInfo();

    cleanUpBuffers();

    if ( fCgiWork ) {
        Buf    lpszCgiText;
        LPTSTR lpszUrl;
        DWORD  dwLenUrl;
        DWORD  dwSize;
        int    i;                                 //lint !e578 hiding i

        for ( i=0;i<argc;i++ ) free(argv[i]);
        free(argv);

        lpszUrl  = (retcode == 0) ? lpszCgiSuccessUrl.Get() : lpszCgiFailureUrl.Get();
        dwLenUrl = 0;
        if ( lpszUrl )
            dwLenUrl = (DWORD)_tcslen(lpszUrl);

        lpszCgiText.Alloc( 1024+(dwLenUrl*4) );

        if ( dwLenUrl ) {
            _stprintf(lpszCgiText.Get(),
                      __T("Expires: Thu, 01 Dec 1994 16:00:00 GMT\r\n") \
                      __T("Pragma: no-cache\r\n") \
                      __T("Location: %s\r\n") \
                      __T("\r\n") \
                      __T("<html><body>\r\n") \
                      __T("<a href=\"%s\">Click here to go to %s</a>\r\n") \
                      __T("<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"0; URL=%s\">\r\n") \
                      __T("</body></html>\r\n"),
                      lpszUrl,lpszUrl,lpszUrl,lpszUrl);
        } else {
            _stprintf(lpszCgiText.Get(),
                      __T("Expires: Thu, 01 Dec 1994 16:00:00 GMT\r\n") \
                      __T("Pragma: no-cache\r\n") \
                      __T("Content-type: text/html\r\n") \
                      __T("\r\n") \
                      __T("<html><body>\r\n") \
                      __T("Blat sending message result = %d : %s\r\n") \
                      __T("</body></html>\r\n"),
                      retcode,(retcode==0) ? __T("Success") : __T("Failure") );
        }

        lpszCgiText.SetLength();
        dwSize = (DWORD)lpszCgiText.Length();
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),lpszCgiText.Get(),dwSize,&dwSize,NULL);
    }

    printMsg( NULL );
    if ( logOut )                                // Added 23 Aug 2000 Craig Morrison
        fclose(logOut);

    logOut = NULL;

    return(abs(retcode));
}

#ifdef BLATDLL_EXPORTS // this is blat.dll, not blat.exe

#define BLATDLL_API __declspec(dllexport)

#if defined(__cplusplus)
extern "C" {
#endif

static int localSend (LPCTSTR sCmd)
{
    LPTSTR* argv;
    LPTSTR  sIn;
    size_t  iCount;
    int     iResult;
    size_t  maxEntries = 256;


    iResult = 0;
    sIn = (LPTSTR) malloc ( (_tcslen ((LPTSTR)sCmd) + 2) * sizeof(_TCHAR) );
    if (sIn) {
        _tcscpy (sIn, (LPTSTR) sCmd);

        argv = (LPTSTR*)malloc( (maxEntries + 1) * sizeof(LPTSTR) );
        if ( argv ) {
            memset( argv, 0, (maxEntries + 1) * sizeof(LPTSTR) );
            for ( iCount = _tcslen(sIn); iCount; ) {
                iCount--;
                if ( (sIn[ iCount ] == __T('\n')) ||
                     (sIn[ iCount ] == __T('\r')) ||
                     (sIn[ iCount ] == __T('\t')) ||
                     (sIn[ iCount ] == __T(' ') ) ) {
                    sIn[ iCount ] = __T('\0');
                    continue;
                }

                break;
            }

            argv[0] = __T("blat.dll");
            iCount = make_argv( sIn,        /* argument list                     */
                                argv,       /* pointer to argv to use            */
                                maxEntries, /* maximum number of entries allowed */
                                1, TRUE );

            iResult = _tmain ((int)iCount, argv, NULL);

            for ( ; iCount > 1; ) {
                free (argv[--iCount]);
            }

            free (argv);
        } else
            iResult = -1;
    } else
        iResult = -1;

    if (sIn)
        free (sIn);

    return iResult;
}

BLATDLL_API int APIENTRY SendW (LPCWSTR sCmd)
{
    int     iResult;

#if defined(_UNICODE) || defined(UNICODE)
    iResult = localSend( sCmd );
#else
    iResult = 0;

    int byteCount = WideCharToMultiByte( CP_UTF8, 0, sCmd, -1, NULL, 0, NULL, NULL );
    if ( byteCount > 1 ) {
        char * pCharCmd = (char *) new char[byteCount+1];
        if ( pCharCmd ) {
            WideCharToMultiByte( CP_UTF8, 0, sCmd, -1, pCharCmd, byteCount, NULL, NULL );

            iResult = localSend( (LPCSTR)pCharCmd );
        }
        delete [] pCharCmd;
    }
#endif

    return iResult;
}

BLATDLL_API int APIENTRY SendA (LPCSTR sCmd)
{
    int     iResult;

#if defined(_UNICODE) || defined(UNICODE)
    iResult = 0;

    int byteCount = MultiByteToWideChar( CP_OEMCP, 0, sCmd, -1, NULL, 0 );
    if ( byteCount > 1 ) {
        wchar_t * pWCharCmd = (wchar_t *) new wchar_t[(size_t)byteCount+1];
        if ( pWCharCmd ) {
            MultiByteToWideChar( CP_OEMCP, 0, sCmd, -1, pWCharCmd, byteCount );

            iResult = localSend( (LPCWSTR)pWCharCmd );
        }
        delete [] pWCharCmd;
    }
#else
    iResult = localSend( sCmd );
#endif
    return iResult;
}

BLATDLL_API int __cdecl cSendW (LPCWSTR sCmd)
{
    return SendW (sCmd);
}

BLATDLL_API int __cdecl cSendA (LPCSTR sCmd)
{
    return SendA (sCmd);
}

BOOL APIENTRY DllMain( HANDLE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    hModule            = hModule;               // Remove compiler warnings
    lpReserved         = lpReserved;            // Remove compiler warnings
    ul_reason_for_call = ul_reason_for_call;    // Remove compiler warnings

//    switch (ul_reason_for_call)
//    {
//        case DLL_PROCESS_ATTACH:
//        case DLL_THREAD_ATTACH:
//        case DLL_THREAD_DETACH:
//        case DLL_PROCESS_DETACH:
//            break;
//    }

    return TRUE;
}

BLATDLL_API int _stdcall BlatW(int argc, LPWSTR argv[]) {

#if defined(_UNICODE) || defined(UNICODE)
    return _tmain(argc, argv, NULL);
#else
    int     iResult;
    char ** newArgv;
    int     x;
    int     byteCount;


    iResult = 0;
    newArgv = (char **) new char[sizeof(char *) * (argc+1)];
    if ( newArgv ) {
        ZeroMemory( newArgv, sizeof(char *) * (argc+1) );
        for ( x = 0; x < argc; x++ ) {
            byteCount = WideCharToMultiByte( CP_UTF8, 0, argv[x], -1, NULL, 0, NULL, NULL );
            if ( byteCount > 1 ) {
                newArgv[x] = (char *) new char[byteCount+1];
                if ( newArgv[x] ) {
                    WideCharToMultiByte( CP_UTF8, 0, argv[x], -1, newArgv[x], byteCount, NULL, NULL );
                }
            }
            if ( !newArgv[x] )
                break;
        }
        if ( x == argc )
            iResult = _tmain(argc, newArgv, NULL);
        x = argc;
        while ( x ) {
            --x;
            if ( newArgv[x] )
                delete [] newArgv[x];
        }
        delete [] newArgv;
    }
    return iResult;
#endif
}

BLATDLL_API int _stdcall BlatA(int argc, LPSTR argv[]) {

#if defined(_UNICODE) || defined(UNICODE)
    int        iResult;
    wchar_t ** newArgv;
    int        x;
    int        byteCount;


    iResult = 0;
    newArgv = (wchar_t **) new char[sizeof(wchar_t *) * (argc+1)];
    if ( newArgv ) {
        ZeroMemory( newArgv, sizeof(wchar_t *) * (argc+1) );
        for ( x = 0; x < argc; x++ ) {
            byteCount = MultiByteToWideChar( CP_OEMCP, 0, argv[x], -1, NULL, 0 );
            if ( byteCount > 1 ) {
                newArgv[x] = (wchar_t *) new wchar_t[(size_t)byteCount+1];
                if ( newArgv[x] ) {
                    MultiByteToWideChar( CP_OEMCP, 0, argv[x], -1, newArgv[x], byteCount );
                }
            }
            if ( !newArgv[x] )
                break;
        }
        if ( x == argc )
            iResult = _tmain(argc, newArgv, NULL);
        x = argc;
        while ( x ) {
            --x;
            if ( newArgv[x] )
                delete [] newArgv[x];
        }
        delete [] newArgv;
    }
    return iResult;
#else
    return _tmain(argc, argv, NULL);
#endif
}

#if defined(_UNICODE) || defined(UNICODE)
#  define printDLL printDLLW
#else
#  define printDLL printDLLA
#endif

static void (__stdcall *pPrintDLL)(LPTSTR) = 0;

/*
 * Built for Unicode   Set PrintDLL   String Type   Convert?
 *       Yes             printDLLW     WideChar       No
 *       Yes             printDLLA     WideChar       Yes
 *       No              printDLLW     MultiByte      Yes
 *       No              printDLLA     MultiByte      No
 */

static void printDLLW(LPTSTR pString) {

    if (pPrintDLL) {
#if defined(_UNICODE) || defined(UNICODE)
        pPrintDLL( (LPTSTR)pString );
#else
        int byteCount = MultiByteToWideChar( CP_OEMCP, 0, (LPSTR)pString, -1, NULL, 0 );
        if ( byteCount > 1 ) {
            LPWSTR pWCharString = (LPWSTR) new wchar_t[byteCount+1];
            if ( pWCharString ) {
                MultiByteToWideChar( CP_OEMCP, 0, (LPSTR)pString, -1, pWCharString, byteCount );

                pPrintDLL( (LPTSTR)pWCharString );
            }
            delete [] pWCharString;
        }
#endif
    }
    else
        _fputts( pString, stdout );
}

static void printDLLA(LPTSTR pString) {

    if (pPrintDLL) {
#if defined(_UNICODE) || defined(UNICODE)
        int byteCount = WideCharToMultiByte( CP_UTF8, 0, (LPWSTR)pString, -1, NULL, 0, NULL, NULL );
        if ( byteCount > 1 ) {
            LPSTR pCharString = (LPSTR) new char[(size_t)byteCount+1];
            if ( pCharString ) {
                WideCharToMultiByte( CP_UTF8, 0, (LPWSTR)pString, -1, pCharString, byteCount, NULL, NULL );

                pPrintDLL( (LPTSTR)pCharString );
            }
            delete [] pCharString;
        }
#else
        pPrintDLL( (LPTSTR)pString );
#endif
    }
    else
        _fputts( pString, stdout );
}

void (*pMyPrintDLL)(LPTSTR) = printDLL;


BLATDLL_API void _stdcall SetPrintFuncW(void (__stdcall *func)(LPTSTR)) {
    pPrintDLL = func;
    pMyPrintDLL = printDLLW;
}

BLATDLL_API void _stdcall SetPrintFuncA(void (__stdcall *func)(LPTSTR)) {
    pPrintDLL = func;
    pMyPrintDLL = printDLLA;
}


BLATDLL_API int APIENTRY Send (LPCSTR sCmd)
{
    return SendA (sCmd);
}

BLATDLL_API void _stdcall SetPrintFunc(void (__stdcall *func)(LPTSTR)) {    /* For compatibility with Blat.dll 2.x */
    SetPrintFuncA( func );
}

BLATDLL_API int _stdcall Blat(int argc, LPSTR argv[])   /* For compatibility with Blat.dll 2.x */
{
    return BlatA( argc, argv );
}

BLATDLL_API int __cdecl cSend (LPCSTR sCmd) /* For compatibility with Blat.dll 2.x */
{
    return cSendA (sCmd);
}


#if defined(__cplusplus)
}
#endif

#endif  // #ifdef BLATDLL_EXPORTS // this is blat.dll, not blat.exe

/*
     Added 23 Aug 2000, Craig Morrison

     Provides a central location to perform message output.

     Works just like printf but allows us to output to a file
     instead of just to stdout.. -q overrides this.

*/

void printMsg(LPTSTR p, ... )
{
    static int  delimiterPrinted = FALSE;
    time_t      nowtime;
    struct tm * localT;
    _TCHAR      buf[2048];
    va_list     args;
    int         x, y;
    _TCHAR      timeBuffer[32];
    static _TCHAR lastByteSent = 0;

    if ( quiet && !logOut )
        return;

    if ( fCgiWork && !logOut ) {
#if INCLUDE_SUPERDEBUG
        superDebug = FALSE;
#endif
        debug = FALSE;
        return;
    }

    va_start(args, p);

    time(&nowtime);
    localT = localtime(&nowtime);
    if ( !localT )
        _tcscpy( timeBuffer, __T("Date/Time not available") );
    else
        _stprintf( timeBuffer, __T("%04d.%02d.%02d %02d:%02d:%02d (%3s)"),
                   localT->tm_year+1900,
                   localT->tm_mon +1,
                   localT->tm_mday,
                   localT->tm_hour,
                   localT->tm_min,
                   localT->tm_sec,
                   days[localT->tm_wday] );

    if ( !p ) {
        if ( logOut ) {
            if ( lastByteSent != __T('\n') )
                _ftprintf( logOut, __T("\n") );

            _ftprintf( logOut, __T("%s-------------End of Session------------------\n"), timeBuffer );
            fflush( logOut );
            fclose( logOut );
            logOut = _tfopen(logFile, fileAppendAttribute);
            delimiterPrinted = FALSE;
        }
        return;
    }

    _vstprintf( buf, p, args );

#if defined(_UNICODE) || defined(UNICODE)
    Buf    tmpStr;
    _TCHAR c;
    DWORD  lValue;
    bool   converted;
    int    len;

    if ( _tcsicmp( charset, __T("UTF-8") ) == 0 ) {
        // Convert UTF-8 data to Unicode for correct string output
        converted = TRUE;
        y = (int)_tcslen(buf);
        for ( x = 0; x < y; x++ ) {
            c = buf[x];
            if ( c > 0xFF ) {
                converted = FALSE;
                break;          /* Exit from for() loop because of bad data. */
            }
            if ( c < 0x80 )
                tmpStr.Add( c );
            else
            {
                if ( (c & 0xE0) == 0xC0 ) {
                    lValue = (DWORD)c & 0x1F;
                    len = 1;
                } else
                if ( (c & 0xF0) == 0xE0 ) {
                    lValue = (DWORD)c & 0x0F;
                    len = 2;
                } else
                if ( (c & 0xF8) == 0xF0 ) {
                    lValue = (DWORD)c & 0x07;
                    len = 3;
                } else
                if ( (c & 0xFC) == 0xF8 ) {
                    lValue = (DWORD)c & 0x03;
                    len = 4;
                } else
                if ( (c & 0xFE) == 0xFC ) {
                    lValue = (DWORD)c & 0x01;
                    len = 5;
                } else {
                    converted = FALSE;
                    break;      /* Exit from for() loop because of bad data. */
                }
                while ( len ) {
                    if ( (buf[x+1] & ~0x3F) != 0x80 )
                        break;  /* Exit from while() loop because of bad data. */

                    lValue = (lValue << 6) + (buf[++x] & 0x3F);
                    len--;
                }
                if ( len || (lValue > 0x0010FFFFul) ) {
                    converted = FALSE;
                    break;      /* Exit from for() loop because of bad data. */
                }
                if ( lValue > 0x0000FFFFul ) {
                    c = (_TCHAR)(((lValue - 0x10000ul) >> 10) | 0xD800);
                    tmpStr.Add( c );
                    c = (_TCHAR)((lValue & 0x3FFul) | 0xDC00);
                } else
                    c = (_TCHAR)lValue;

                tmpStr.Add( c );
            }
        }
        if ( converted ) {
            _tcscpy( buf, tmpStr.Get() );
            y = (int)tmpStr.Length();
        }

        tmpStr.Free();
    }
#endif
    y = (int)_tcslen(buf);
    for ( x = 0; buf[x]; x++ ) {
        if ( buf[x] == __T('\r') ) {
            if ( (buf[x+1] == __T('\r')) || (buf[x+1] == __T('\n')) ) {
                memcpy( &buf[x], &buf[x+1], (y - x)*sizeof(_TCHAR) );
                x--;
            }
            continue;
        }

        if ( buf[x] == __T('\n') ) {
            if ( buf[x+1] == __T('\n') ) {
                memcpy( &buf[x], &buf[x+1], (y - x)*sizeof(_TCHAR) );
                x--;
                continue;
            }

            if ( buf[x+1] == __T('\r') ) {
                memcpy( &buf[x+1], &buf[x+2], (y - (x+1))*sizeof(_TCHAR) );
                x--;
            }
            continue;
        }
    }

    y = (int)_tcslen(buf);
    if ( y ) {
        lastByteSent = buf[y - 1];

        if ( logOut ) {
            if ( !delimiterPrinted ) {
                _ftprintf( logOut, __T("\n%s------------Start of Session-----------------\n"), timeBuffer );
                delimiterPrinted = TRUE;
            }

            if ( timestamp )
                _ftprintf( logOut, __T("%s: "), timeBuffer );

#if defined(_MSC_VER) && (_MSC_VER < 1400) && (defined(_UNICODE) || defined(UNICODE))
            Buf source;
            Buf output;
            int tempUTF;

            source.Add( (_TCHAR)0xFEFF );       /* Prepend a UTF-16 BOM */
            source.Add( buf );
            tempUTF = NATIVE_16BIT_UTF;
            convertPackedUnicodeToUTF( source, output, &tempUTF, NULL, 8 );
            if ( tempUTF )
                _tcscpy( buf, output.Get() );

            output.Free();
            source.Free();
#endif
            _ftprintf(logOut, __T("%s"), buf);
            fflush( logOut );
            fclose( logOut );
            logOut = _tfopen(logFile, fileAppendAttribute);
        } else {
#ifdef BLATDLL_EXPORTS
            if ( pMyPrintDLL )
                pMyPrintDLL(buf);
            else
#endif
            {
                _tprintf(__T("%s"), buf);
                fflush( stdout );
            }
        }
    }
    va_end(args);
}
