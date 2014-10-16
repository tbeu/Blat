/*
    blat.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "blat.h"
#include "winfile.h"

/* generic socket DLL support */
#include "gensock.h"
#if SUPPORT_GSSAPI
#include "gssfuncs.h" // Please read the comments here for information about how to use GssSession
#define VERSION_SUFFIX  " w/GSS encryption"
#else
#define VERSION_SUFFIX  ""
#endif


#define BLAT_VERSION    "2.7.6"
// Major revision level  *      Update this when a major change occurs, such as a complete rewrite.
// Minor revision level    *    Update this when the user experience changes, such as when new options/features are added.
// Bug   revision level      *  Update this when bugs are fixed, but no other user experience changes.

#ifdef __cplusplus
extern "C" int main( int argc, char **argv, char **envp );
#endif

extern BOOL DoCgiWork(int &argc, char** &argv,    Buf &lpszMessage,
                      Buf &lpszCgiSuccessUrl,     Buf &lpszCgiFailureUrl,
                      Buf &lpszFirstReceivedData, Buf &lpszOtherHeader);
extern int  collectAttachmentInfo ( DWORD & totalsize, int msgBodySize );
extern void releaseAttachmentInfo ( void );

extern void searchReplaceEmailKeyword (Buf & email_addresses);
extern int  send_email( int msgBodySize, Buf &lpszFirstReceivedData, Buf &lpszOtherHeader,
                        char * attachment_boundary, char * multipartID,
                        int nbrOfAttachments, DWORD totalsize );

#if INCLUDE_NNTP
extern int  send_news( int msgBodySize, Buf &lpszFirstReceivedData, Buf &lpszOtherHeader,
                       char * attachment_boundary, char * multipartID,
                       int nbrOfAttachments, DWORD totalsize );
#endif
extern int  GetRegEntry( char *pstrProfile );

extern void printTitleLine( int quiet );
extern int  printUsage( int optionPtr );
extern int  processOptions( int argc, char ** argv, int startargv, int preprocessing );

extern size_t make_argv( char * arglist,                /* argument list                     */
                         char **static_argv,            /* pointer to argv to use            */
                         size_t max_static_entries,     /* maximum number of entries allowed */
                         size_t starting_entry,         /* entry in argv to begin placing    */
                         int    from_dll );             /* blat called as .dll               */

void printMsg(const char *p, ... );              // Added 23 Aug 2000 Craig Morrison

#if BLAT_LITE
#else
extern void convertUnicode( Buf &sourceText, int * utf, char * charset, int utfRequested );
#endif

#if INCLUDE_SUPERDEBUG
extern char          superDebug;
#endif
extern char          my_hostname[];
extern char          Profile[TRY_SIZE+1];
extern const char  * defaultSMTPPort;
#if INCLUDE_NNTP
extern const char  * defaultNNTPPort;
#endif
extern char          priority[2];

extern char          impersonating;
extern char          ssubject;
extern int           maxNames;

extern char          boundaryPosted;
extern char          commentChar;
extern char          disposition;
extern HANDLE        dll_module_handle;
extern char          includeUserAgent;
extern char          sendUndisclosed;
#if SUPPORT_MULTIPART
extern unsigned long multipartSize;
#endif
extern char          needBoundary;
extern char          returnreceipt;
#ifdef BLAT_LITE
#else
extern char          forcedHeaderEncoding;
extern char          noheader;
extern unsigned int  uuencodeBytesLine;
extern unsigned long maxMessageSize;
extern char          loginAuthSupported;
extern char          plainAuthSupported;
extern char          cramMd5AuthSupported;
extern char          ByPassCRAM_MD5;
#endif
#if SUPPORT_GSSAPI
extern char          gssapiAuthSupported;
#endif

int     delayBetweenMsgs;

char    blatVersion[]    = BLAT_VERSION;
char    blatVersionSuf[] = VERSION_SUFFIX;
char    blatBuildDate[]  = __DATE__;
char    blatBuildTime[]  = __TIME__;

#if INCLUDE_POP3
char    POP3Host[SERVER_SIZE+1];
char    POP3Port[SERVER_SIZE+1];
char    POP3Login[SERVER_SIZE+1];
char    POP3Password[SERVER_SIZE+1];
char    xtnd_xmit_wanted;
char    xtnd_xmit_supported;
#endif
#if INCLUDE_IMAP
char    IMAPHost[SERVER_SIZE+1];
char    IMAPPort[SERVER_SIZE+1];
char    IMAPLogin[SERVER_SIZE+1];
char    IMAPPassword[SERVER_SIZE+1];
#endif

char    SMTPHost[SERVER_SIZE+1];
char    SMTPPort[SERVER_SIZE+1];

#if INCLUDE_NNTP
char    NNTPHost[SERVER_SIZE+1];
char    NNTPPort[SERVER_SIZE+1];
Buf     groups;
#endif

char    AUTHLogin[SERVER_SIZE+1];
char    AUTHPassword[SERVER_SIZE+1];
char    Try[TRY_SIZE+1];
char    Sender[SENDER_SIZE+1];
Buf     TempConsole;
Buf     Recipients;
Buf     destination;
Buf     cc_list;
Buf     bcc_list;
char    loginname[SENDER_SIZE+1];       // RFC 821 MAIL From. <loginname>. There are some inconsistencies in usage
char    senderid[SENDER_SIZE+1];        // Inconsistent use in Blat for some RFC 822 Field definitions
char    sendername[SENDER_SIZE+1];      // RFC 822 Sender: <sendername>
char    fromid[SENDER_SIZE+1];          // RFC 822 From: <fromid>
char    replytoid[SENDER_SIZE+1];       // RFC 822 Reply-To: <replytoid>
char    returnpathid[SENDER_SIZE+1];    // RFC 822 Return-Path: <returnpath>
char    subject[SUBJECT_SIZE+1];
Buf     alternateText;
char    clearLogFirst;

#if SUPPORT_GSSAPI  //Added 2003-11-07 Joseph Calzaretta
BOOL    authgssapi;
BOOL    mutualauth;
BOOL    bSuppressGssOptionsAtRuntime;
char    servicename[SERVICENAME_SIZE];
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
char    organization[ORG_SIZE+1];
char    xheaders[DEST_SIZE+1];
char    aheaders1[DEST_SIZE+1];
char    aheaders2[DEST_SIZE+1];
char    uuencode;                       // by default Blat does not use UUEncode // Added by Gilles Vollant

    // by default Blat does not use base64 Quoted-Printable Content-Transfer-Encoding!
    //  If you're looking for something to do, then it would be nice if this thing
    //  detected any non-printable characters in the input, and use base64 whenever
    //  quoted-printable wasn't chosen by the user.
char    base64;
int     utf;

char    yEnc;
char    deliveryStatusRequested;
char    deliveryStatusSupported;

char    eightBitMimeSupported;
char    eightBitMimeRequested;
char    binaryMimeSupported;
//char    binaryMimeRequested;

char    optionsFile[_MAX_PATH];
FILE *  optsFile;
Buf     userContentType;
#endif

char    textmode[TEXTMODE_SIZE+1];      // added 15 June 1999 by James Greene "greene@gucc.org"
char    bodyFilename[_MAX_PATH];
Buf     bodyparameter;
char    ConsoleDone;
char    formattedContent;
char    mime;                           // by default Blat does not use mime Quoted-Printable Content-Transfer-Encoding!
char    quiet;
char    debug;
char    haveEmbedded;
char    haveAttachments;
int     attach;
int     regerr;
char    bodyconvert;

int     exitRequired;

char    attachfile[64][MAX_PATH+1];
char    my_hostname_wanted[1024];
FILE *  logOut;
int     fCgiWork;
char    charset[40];                    // Added 25 Apr 2001 Tim Charron (default ISO-8859-1)

char    attachtype[64];
char    timestamp;

const char * stdinFileName     = "stdin.txt";
const char * defaultCharset    = "iso-8859-1";
const char * days[]            = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat"};


LPCSTR GetNameWithoutPath(LPCSTR lpFn)
{
    LPCSTR lpRet = lpFn ;

    while ( *lpFn ) {
        if ( ((*lpFn) == ':') || ((*lpFn) == '\\') )
            lpRet = lpFn + 1;

        lpFn = CharNext(lpFn);
    }
    return(lpRet);
}

static void cleanUpBuffers( void )
{
    TempConsole.Free();
    alternateText.Free();
    bodyparameter.Free();
    bcc_list.Free();
    cc_list.Free();
    destination.Free();
    Recipients.Free();
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
    char * pp;
    char   prevChar;
    Buf    newBuffer;

    pp = buffer.Get();
    if ( pp ) {
        prevChar = 0;
        for ( ; *pp; pp++ ) {
            if ( *pp == ' ' )
                if ( !prevChar || (prevChar == ' ') )
                    continue;

            newBuffer.Add( prevChar = *pp );
        }

        if ( prevChar == ' ' )
            newBuffer.Remove();

        buffer.Clear();
        buffer.Add( newBuffer );
    }
}


int main( int argc,             /* Number of strings in array argv          */
          char **argv,          /* Array of command-line argument strings   */
          char **envp )         /* Array of environment variables           */
{
    int      i, j;
    int      retcode;
    WinFile  fileh;
    char     boundary1[24];
    Buf      lpszMessageCgi;
    Buf      lpszCgiFailureUrl;
    Buf      lpszCgiSuccessUrl;
    Buf      lpszFirstReceivedData;
    Buf      lpszOtherHeader;
#if BLAT_LITE
#else
    char **  secondArgV;
    int      secondArgC;
#endif
    Buf      temp;
    DWORD    filesize;
    DWORD    totalsize; // total size of all attachments and the message body.
    int      nbrOfAttachments;
    char     multipartID[1200];

    envp = envp;    // To remove compiler warnings.

    // Initialize global variables.
    cleanUpBuffers();

    commentChar = ';';
    Profile[0] = 0;
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
    my_hostname_wanted[0] = 0;
    needBoundary = 0;
    priority[0] = 0;
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
    strcpy(NNTPPort, defaultNNTPPort);
#endif
    strcpy(SMTPPort, defaultSMTPPort);
    strcpy(Try,      "1");                      // Was ONCE

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
    utf                     = 0;
    deliveryStatusRequested = 0;
    deliveryStatusSupported =
    uuencode                =         // by default Blat does not use UUEncode // Added by Gilles Vollant
    base64                  =
    yEnc                    =
    eightBitMimeSupported   =
    eightBitMimeRequested   =
//    binaryMimeRequested     =
#endif
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
    bodyconvert             = TRUE;
    formattedContent        = TRUE;
    logOut                  = (FILE *)NULL;


#if SUPPORT_GSSAPI  //Added 2003-11-07 Joseph Calzaretta
    servicename[0]          =
#endif
#if INCLUDE_POP3
    POP3Host[0]             =
    POP3Port[0]             =
    POP3Login[0]            =
    POP3Password[0]         =
#endif
#if INCLUDE_IMAP
    IMAPHost[0]             =
    IMAPPort[0]             =
    IMAPLogin[0]            =
    IMAPPassword[0]         =
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
    SMTPHost[0]             =
    Sender[0]               =
    my_hostname_wanted[0]   =
    my_hostname[0]          =
    loginname[0]            =           // RFC 821 MAIL From: <loginname>
    senderid[0]             =           // Inconsistent use in Blat for some RFC 822 Field definitions
    sendername[0]           =           // RFC 822 Sender: <sendername>
    fromid[0]               =           // RFC 822 From: <fromid>
    replytoid[0]            =           // RFC 822 Reply-To: <replytoid>
    returnpathid[0]         =           // RFC 822 Return-Path: <returnpath>subject[0] = '\0';
    subject[0]              =
    AUTHLogin[0]            =
    AUTHPassword[0]         =
    bodyFilename[0]         = '\0';

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
        char c;
        if ( (GetEnvironmentVariable("REQUEST_METHOD",   &c,1)>0) &&
             (GetEnvironmentVariable("GATEWAY_INTERFACE",&c,1)>0) )
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

        printf( "\nBlat saw the following command line:\n" );
        for ( x = 0; x < argc; x++ ) {
            if ( x )
                printf( " " );

            if ( strchr(argv[x], ' ') || strchr(argv[x], '"') )
                printf( "\"%s\"", argv[x] );
            else
                printf( "%s", argv[x] );
        }
        printf( "\n\n" );
    }
 */

    charset[0] = '\0';

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

    strcpy(textmode, "plain");

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
    if ( ((argv[1][0] == '-') || (argv[1][0] == '/')) && argv[1][1] )
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
        char   buffer[2048];
        char * bufPtr;
        size_t maxEntries = 256;

        secondArgV = (char **)malloc( (maxEntries + 1) * sizeof(char *) );
        if ( secondArgV ) {
            size_t nextEntry = 0;

            memset( secondArgV, 0, (maxEntries + 1) * sizeof(char *) );
            optsFile = fopen( optionsFile, "r" );
            if ( !optsFile ) {
                free( secondArgV );
                printMsg( "Options file \"%s\" not found or could not be opened.\n", optionsFile );

                printMsg( NULL );
                if ( logOut )
                    fclose(logOut);

                cleanUpBuffers();
                return(2);
            }

            for ( ; nextEntry < maxEntries; ) {
                if ( feof( optsFile ) )
                    break;

                bufPtr = fgets( buffer, sizeof( buffer ), optsFile );
                if ( bufPtr ) {
                    for ( ;; ) {
                        i = (int)strlen(buffer) - 1;
                        if ( buffer[ i ] == '\n' ) {
                            buffer[ i ] = '\0';
                            continue;
                        }

                        if ( buffer[ i ] == '\r' ) {
                            buffer[ i ] = '\0';
                            continue;
                        }

                        break;
                    }

                    nextEntry = make_argv( buffer,      /* argument list                     */
                                           secondArgV,  /* pointer to argv to use            */
                                           maxEntries,  /* maximum number of entries allowed */
                                           nextEntry,
                                           FALSE );
                }
            }

            fclose( optsFile );
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
    if ( (argv[1][0] != '-') && (argv[1][0] != '/') ) {
        strncpy( bodyFilename, argv[1], _MAX_PATH-1 );
        bodyFilename[_MAX_PATH-1] = '\0';
    }

    bodyFilename[_MAX_PATH-1] = 0;
    regerr = GetRegEntry( Profile );

    strcpy(senderid,  Sender);
    strcpy(loginname, Sender);

    srand( (unsigned int) time( NULL ) + (unsigned int) clock() );

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

    if ( ((argv[1][0] == '-') || (argv[1][0] == '/')) && argv[1][1] )
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
            printMsg( "Failed to open registry key for Blat\n" );
#else
    if ( regerr == 12 )
        if ( !loginname[0] || !SMTPHost[0] )
            printMsg( "Failed to open registry key for Blat\n" );
#endif

    // if we are not impersonating loginname is the same as the sender
    if ( ! impersonating )
        strcpy(senderid, loginname);

    // fixing the argument parsing
    // Ends here

#if INCLUDE_NNTP
    if ( !loginname[0] || (!SMTPHost[0] && !NNTPHost[0]) ) {
        printMsg( "To set the SMTP server's name/address and your username/email address for that\n" \
                  "server machine do:\n" \
                  "blat -install  server_name  your_email_address\n" \
                  "or use '-server <server_name>' and '-f <your_email_address>'\n");
        printMsg( "aborting, nothing sent\n" );

        printMsg( NULL );
        if ( logOut )
            fclose(logOut);

        cleanUpBuffers();
        return(12);
    }
#else
    if ( !loginname[0] || !SMTPHost[0] ) {
        printMsg( "To set the SMTP server's name/address and your username/email address for that\n" \
                  "server machine do:\n" \
                  "blat -install  server_name  your_email_address\n" \
                  "or use '-server <server_name>' and '-f <your_email_address>'\n");
        printMsg( "aborting, nothing sent\n" );

        printMsg( NULL );
        if ( logOut )
            fclose(logOut);

        cleanUpBuffers();
        return(12);
    }
#endif

    // make sure filename exists, get full pathname
    if ( bodyFilename[0] && (strcmp(bodyFilename, "-") != 0) ) {
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
                int lastError = GetLastError();
                if ( lastError == 0 )
                    printMsg("%s does not exist\n",bodyFilename);
                else
                    printMsg("unknown error code %d when trying to open %s\n", lastError, bodyFilename);

                printMsg( NULL );
                if ( logOut )
                    fclose(logOut);

                cleanUpBuffers();
                return(2);
            }
            CloseHandle(fh);
        } else {                        // Windows 95 through NT 4.0
            OFSTRUCT of;
            HFILE fh;

            fh = OpenFile(bodyFilename,&of,OF_EXIST);
            if ( fh == HFILE_ERROR ) {
                printMsg("%s does not exist\n",bodyFilename);

                printMsg( NULL );
                if ( logOut )
                    fclose(logOut);

                cleanUpBuffers();
                return(2);
            }
            _lclose(fh);
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
        // NOT! while ( destination[i]==' ' )   i++;
        // look for comments in brackets, and omit
        if ( destination.Get()[i]=='(' ) {
            while ( destination.Get()[i]!=')' )   i++;
            i++;
        }
        temp.Get()[j++] = destination.Get()[i];
    }
    temp.Get()[j] = '\0';                              // End of list added!
    Recipients.Add(temp.Get());

    // Parse the "Cc:" line
    for ( i = j = 0; (unsigned)i < cc_list.Length(); i++ ) {
        // strip white space
        // NOT! while ( cc_list[i]==' ' ) i++;
        // look for comments in brackets, and omit
        if ( cc_list.Get()[i]=='(' ) {
            while ( cc_list.Get()[i]!=')' ) i++;
            i++;
        }
        temp.Get()[j++] = cc_list.Get()[i];
    }
    temp.Get()[j] = '\0';                              // End of list added!
    if ( cc_list.Length() ) {
        if ( Recipients.Length() )
            Recipients.Add(',');

        Recipients.Add(temp.Get());
    }

    // Parse the "Bcc:" line
    for (i = j = 0; (unsigned)i < bcc_list.Length(); i++) {
        // strip white space
        // NOT! while ( bcc_list[i]==' ' )  i++;
        // look for comments in brackets, and omit
        if (bcc_list.Get()[i] == '(') {
            while (bcc_list.Get()[i] != ')') i++;
            i++;
        }
        temp.Get()[j++] = bcc_list.Get()[i];
    }
    temp.Get()[j] = '\0';                              // End of list added!
    if ( bcc_list.Length() > 0 ) {
        if ( Recipients.Length() )
            Recipients.Add(',');

        Recipients.Add(temp.Get());
    }

#if INCLUDE_NNTP
    if ( !Recipients.Length() && !groups.Length() ) {
        printMsg( "No target email address or newsgroup was specified.  You must give an email\n" \
                  "address or usenet newsgroup to send messages to.  Use -to, -cc, or -bcc option\n" \
                  "for email, or -groups for usenet.\n" \
                  "Aborting, nobody to send messages to.\n" );

        printMsg( NULL );
        if ( logOut )
            fclose(logOut);

        cleanUpBuffers();
        return(12);
    }
#else
    if ( !Recipients.Length() ) {
        printMsg( "No target email address was specified.  You must give an email address\n" \
                  "to send messages to.  Use -to, -cc, or -bcc option.\n" \
                  "Aborting, nobody to send messages to.\n" );

        printMsg( NULL );
        if ( logOut )
            fclose(logOut);

        cleanUpBuffers();
        return(12);
    }
#endif

    // if reading from the console, read everything into a temporary file first
    ConsoleDone = FALSE;
    if ( !bodyFilename[0] || (strcmp(bodyFilename, "-") == 0) ) {

        if ( lpszMessageCgi.Length() ) {
            ConsoleDone = TRUE;
            TempConsole.Add(lpszMessageCgi);
        } else if ( bodyparameter.Length() ) {
            char * p = bodyparameter.Get();
            ConsoleDone = TRUE;
            if (bodyconvert) {
                i = 0;
                while ( p[i] ) {
                    if ( p[i] == '|' )      // CRLF signified by the pipe character
                        TempConsole.Add( "\r\n" );
                    else
                        TempConsole.Add( p[i] );
                    i++;
                }
            }
            else
                TempConsole.Add( p );
        } else {
            ConsoleDone = TRUE;
            for ( ; ; ) {
                DWORD dwNbRead = 0;
                i=0;

                if ( !ReadFile(GetStdHandle(STD_INPUT_HANDLE),&i,1,&dwNbRead,NULL) ||
                     !dwNbRead || (i == '\x1A') )
                    break;

                TempConsole.Add((char)i);
            }
        }
        strcpy(bodyFilename, stdinFileName);
    }

    if (!ConsoleDone) {
        DWORD dummy;

        //get the text of the file into a string buffer
        if ( !fileh.OpenThisFile(bodyFilename) ) {
            printMsg("error reading %s, aborting\n",bodyFilename);

            printMsg( NULL );
            if ( logOut )
                fclose(logOut);

            cleanUpBuffers();
            return(3);
        }
        if ( !fileh.IsDiskFile() ) {
            fileh.Close();
            printMsg("Sorry, I can only mail messages from disk files...\n");

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
            *TempConsole.GetTail() = 0;
        } else {
            filesize = 1;
            TempConsole = "\n";
            retcode = true;
        }
        fileh.Close();

        if ( !retcode ) {
            printMsg("error reading %s, aborting\n", bodyFilename);
            cleanUpBuffers();
            return(5);
        }
    }

#if BLAT_LITE
#else
    if ( eightBitMimeRequested )
        convertUnicode( TempConsole, &utf, charset, 8 );
    else
        convertUnicode( TempConsole, &utf, charset, 7 );
#endif
    filesize = TempConsole.Length();

    nbrOfAttachments = collectAttachmentInfo( totalsize, filesize );

    if ( nbrOfAttachments && !totalsize ) {
        printMsg( "Sum total size of all attachments exceeds 4G bytes.  This is too much to be\n" \
                  "sending with SMTP or NNTP.  Please try sending through FTP instead.\n" \
                  "Aborting, too much data to send.\n" );

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
        Buf   lpszCgiText;
        LPSTR lpszUrl;
        DWORD dwLenUrl;
        DWORD dwSize;
        int   i;                                 //lint !e578 hiding i

        for ( i=0;i<argc;i++ ) free(argv[i]);
        free(argv);

        lpszUrl  = (retcode == 0) ? lpszCgiSuccessUrl.Get() : lpszCgiFailureUrl.Get();
        dwLenUrl = 0;
        if ( lpszUrl )
            dwLenUrl = lstrlen(lpszUrl);

        lpszCgiText.Alloc( 1024+(dwLenUrl*4) );

        if ( dwLenUrl ) {
            wsprintf(lpszCgiText.Get(),
                     "Expires: Thu, 01 Dec 1994 16:00:00 GMT\r\n" \
                     "Pragma: no-cache\r\n" \
                     "Location: %s\r\n" \
                     "\r\n" \
                     "<html><body>\r\n" \
                     "<a href=\"%s\">Click here to go to %s</a>\r\n"\
                     "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"0; URL=%s\">\r\n"\
                     "</body></html>\r\n" ,
                     lpszUrl,lpszUrl,lpszUrl,lpszUrl);
        } else {
            wsprintf(lpszCgiText.Get(),
                     "Expires: Thu, 01 Dec 1994 16:00:00 GMT\r\n" \
                     "Pragma: no-cache\r\n" \
                     "Content-type: text/html\r\n" \
                     "\r\n" \
                     "<html><body>\r\n" \
                     "Blat sending message result = %d : %s\r\n"\
                     "</body></html>\r\n" ,
                     retcode,(retcode==0)?"Success":"Failure");
        }

        lpszCgiText.SetLength();
        dwSize = lpszCgiText.Length();
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

extern "C"
int APIENTRY Send (LPCSTR sCmd)
{
    char ** argv;
    char *  sIn;
    size_t  iCount;
    int     iResult;
    size_t  maxEntries = 256;


    iResult = 0;
    sIn = (char *) malloc (strlen ((char *)sCmd) + 2);
    if (sIn) {
        strcpy (sIn, (char *) sCmd);

        argv = (char **)malloc( (maxEntries + 1) * sizeof(char *) );
        if ( argv ) {
            memset( argv, 0, (maxEntries + 1) * sizeof(char *) );
            for ( iCount = strlen(sIn); iCount; ) {
                iCount--;
                if ( (sIn[ iCount ] == '\n') ||
                     (sIn[ iCount ] == '\r') ||
                     (sIn[ iCount ] == '\t') ||
                     (sIn[ iCount ] == ' ' ) ) {
                    sIn[ iCount ] = '\0';
                    continue;
                }

                break;
            }

            argv[0] = "blat.dll";
            iCount = make_argv( sIn,        /* argument list                     */
                                argv,       /* pointer to argv to use            */
                                maxEntries, /* maximum number of entries allowed */
                                1, TRUE );

            iResult = main ((int)iCount, argv, NULL);

            for ( ; iCount > 1; ) {
                free (argv[--iCount]);
            }

            free (argv);
        }
        else
            iResult = -1;
    }
    else
        iResult = -1;

    if (sIn)
        free (sIn);

    return iResult;
}


extern "C"
int __cdecl cSend (LPCSTR sCmd)
{
    return Send (sCmd);
}


extern "C"
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

extern "C"
BLATDLL_API int _stdcall Blat(int argc, char *argv[]) {
    return main(argc, argv, NULL);
}

void (__stdcall *pPrintDLL)(char *) = 0;

void printDLL(char *p) {
    if (pPrintDLL)
        pPrintDLL(p);
}

extern "C"
BLATDLL_API void _stdcall SetPrintFunc(void (__stdcall *func)(char *)) {
    pPrintDLL = func;
}

#endif

/*
     Added 23 Aug 2000, Craig Morrison

     Provides a central location to perform message output.

     Works just like printf but allows us to output to a file
     instead of just to stdout.. -q overrides this.

*/

void printMsg(const char *p, ... )
{
    static int  delimiterPrinted = FALSE;
    time_t      nowtime;
    struct tm * localT;
    char        buf[2048];
    va_list     args;
    int         x, y;
    char        timeBuffer[32];
    static char lastByteSent = 0;

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
        strcpy( timeBuffer, "Date/Time not available" );
    else
        sprintf( timeBuffer, "%04u.%02u.%02u %02u:%02u:%02u (%3s)",
                 localT->tm_year+1900,
                 localT->tm_mon +1,
                 localT->tm_mday,
                 localT->tm_hour,
                 localT->tm_min,
                 localT->tm_sec,
                 days[localT->tm_wday] );

    if ( !p ) {
        if ( logOut ) {
            if ( lastByteSent != '\n' )
                fprintf( logOut, "\n" );

            fprintf( logOut, "%s-------------End of Session------------------\n", timeBuffer );
            delimiterPrinted = FALSE;
        }
        return;
    }

    vsprintf( buf, p, args );
    y = (int)strlen(buf);
    for ( x = 0; buf[x]; x++ ) {
        if ( buf[x] == '\r' ) {
            if ( (buf[x+1] == '\r') || (buf[x+1] == '\n') ) {
                memcpy( &buf[x], &buf[x+1], y - x );
                x--;
            }
            continue;
        }

        if ( buf[x] == '\n' ) {
            if ( buf[x+1] == '\n' ) {
                memcpy( &buf[x], &buf[x+1], y - x );
                x--;
                continue;
            }

            if ( buf[x+1] == '\r' ) {
                memcpy( &buf[x+1], &buf[x+2], y - (x+1) );
                x--;
            }
            continue;
        }
    }

    lastByteSent = buf[strlen(buf) - 1];

    if ( logOut ) {
        if ( !delimiterPrinted ) {
            fprintf( logOut, "\n%s------------Start of Session-----------------\n", timeBuffer );
            delimiterPrinted = TRUE;
        }

        if ( timestamp )
            fprintf( logOut, "%s: ", timeBuffer );

        fprintf(logOut, "%s", buf);
    } else {
#ifdef BLATDLL_EXPORTS
        printDLL(buf);
#else
        printf("%s", buf);
#endif
    }
    va_end(args);
}
