/*
    blat.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <io.h>
//#include <vld.h>

#include "blat.h"
#include "winfile.h"
#include "common_data.h"
#include "blatext.hpp"
#include "macros.h"
#include "blatcgi.hpp"
#include "attach.hpp"
#include "sendsmtp.hpp"
#include "sendnntp.hpp"
#include "reg.hpp"
#include "options.hpp"
#include "makeargv.hpp"
#include "unicode.hpp"

#define VERSION_SUFFIX  __T("")

/* generic socket DLL support */
#include "gensock.h"
#if SUPPORT_GSSAPI
//#undef  VERSION_SUFFIX
//#define VERSION_SUFFIX  __T(" w/GSS encryption")
#else
#endif


#define BLAT_VERSION    __T("3.2.23")
// Major revision level      *      Update this when a major change occurs, such as a complete rewrite.
// Minor revision level        *    Update this when the user experience changes, such as when new options/features are added.
// Bug   revision level          *  Update this when bugs are fixed, but no other user experience changes.

_TCHAR  blatVersion[]    = BLAT_VERSION;
_TCHAR  blatVersionSuf[] = VERSION_SUFFIX;
_TCHAR  blatBuildDate[64] = { __T('\0') };
_TCHAR  blatBuildTime[64] = { __T('\0') };

char    blatBuildDateA[] = __DATE__;
char    blatBuildTimeA[] = __TIME__;

LPTSTR days[] = { __T("Sun"),
                  __T("Mon"),
                  __T("Tue"),
                  __T("Wed"),
                  __T("Thu"),
                  __T("Fri"),
                  __T("Sat") };


static void cleanUpBuffers( COMMON_DATA & CommonData, LPTSTR * savedArguments, int argumentCount )
{
    //FUNCTION_ENTRY();
    CommonData.Profile.Free();
    CommonData.priority.Free();
    CommonData.sensitivity.Free();
#if INCLUDE_POP3
    CommonData.POP3Host.Free();
    CommonData.POP3Port.Free();
#endif
#if INCLUDE_IMAP
    CommonData.IMAPHost.Free();
    CommonData.IMAPPort.Free();
#endif

    CommonData.SMTPHost.Free();
    CommonData.SMTPPort.Free();

#if INCLUDE_NNTP
    CommonData.NNTPHost.Free();
    CommonData.NNTPPort.Free();
#endif
    CommonData.Try.Free();
    CommonData.loginname.Free();
    CommonData.senderid.Free();
    CommonData.sendername.Free();
    CommonData.fromid.Free();
    CommonData.replytoid.Free();
    CommonData.returnpathid.Free();
    CommonData.logFile.Free();
#if SUPPORT_GSSAPI  //Added 2003-11-07 Joseph Calzaretta
    CommonData.servicename.Free();
    CommonData.mechtype.Free();
#endif

#if BLAT_LITE
#else
    CommonData.optionsFile.Free();
#endif

    CommonData.bodyFilename.Free();
    CommonData.my_hostname_wanted.Free();
    CommonData.charset.Free();
    CommonData.my_hostname.Free();

    CommonData.TempConsole.Free();
    CommonData.alternateText.Free();
    CommonData.bodyparameter.Free();
    CommonData.bcc_list.Free();
    CommonData.cc_list.Free();
    CommonData.destination.Free();
    CommonData.Recipients.Free();
    CommonData.Sender.Free();
    CommonData.subject.Free();
    CommonData.AUTHLogin.Free();
    CommonData.AUTHPassword.Free();
#if INCLUDE_IMAP
    CommonData.IMAPLogin.Free();
    CommonData.IMAPPassword.Free();
#endif
#if INCLUDE_POP3
    CommonData.POP3Login.Free();
    CommonData.POP3Password.Free();
#endif
#if INCLUDE_NNTP
    CommonData.groups.Free();
#endif
#if SUPPORT_SALUTATIONS
    CommonData.salutation.Free();
#endif
#if SUPPORT_SIGNATURES
    CommonData.signature.Free();
#endif
#if SUPPORT_TAGLINES
    CommonData.tagline.Free();
#endif
#if SUPPORT_POSTSCRIPTS
    CommonData.postscript.Free();
#endif
#if BLAT_LITE
#else
    CommonData.organization.Free();
    CommonData.xheaders.Free();
    CommonData.aheaders1.Free();
    CommonData.aheaders2.Free();
    CommonData.messageId.Free();
#endif

    CommonData.mdnHeader.Free();
    CommonData.mimeHeader.Free();

    if ( savedArguments ) {
        int x;

        for ( x = 1; x < argumentCount; x++ ) {
            free( savedArguments[x] );
        }

        free( savedArguments );
    }
    //FUNCTION_EXIT();
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


static void InitializeCommonData( COMMON_DATA & CommonData )
{
    // Initialize global variables.
    cleanUpBuffers( CommonData, NULL, 0 );

    CommonData.Profile.Clear();
    CommonData.priority.Clear();
    CommonData.sensitivity.Clear();

    CommonData.impersonating                = 0;
    CommonData.ssubject                     = 0;
    CommonData.maxNames                     = 0;

    CommonData.boundaryPosted               = 0;
    CommonData.commentChar                  = __T(';');
    CommonData.disposition                  = 0;
    CommonData.includeUserAgent             = 0;
    CommonData.sendUndisclosed              = 0;
    CommonData.disableMPS                   = 0;
#if SUPPORT_MULTIPART
    CommonData.multipartSize                = 0;
#endif
    CommonData.needBoundary                 = 0;
    CommonData.returnreceipt                = 0;
    CommonData.maxMessageSize               = 0;
    CommonData.loginAuthSupported           = 0;
    CommonData.plainAuthSupported           = 0;
    CommonData.cramMd5AuthSupported         = 0;
    CommonData.forcedHeaderEncoding         = 0;
    CommonData.noheader                     = 0;
    CommonData.ByPassCRAM_MD5               = 0;
    CommonData.commandPipelining            = 0;
    CommonData.enhancedCodeSupport          = 0;

    CommonData.delayBetweenMsgs             = 0;

#if INCLUDE_POP3
    CommonData.POP3Host.Clear();
    CommonData.POP3Port.Clear();
    CommonData.xtnd_xmit_wanted             = 0;
    CommonData.xtnd_xmit_supported          = 0;
#endif
#if INCLUDE_IMAP
    CommonData.IMAPHost.Clear();
    CommonData.IMAPPort.Clear();
#endif

    CommonData.SMTPHost.Clear();
    CommonData.SMTPPort.Clear();

#if INCLUDE_NNTP
    CommonData.NNTPHost.Clear();
    CommonData.NNTPPort.Clear();
#endif

    CommonData.Try.Clear();
    CommonData.loginname.Clear();
    CommonData.senderid.Clear();
    CommonData.sendername.Clear();
    CommonData.fromid.Clear();
    CommonData.replytoid.Clear();
    CommonData.returnpathid.Clear();
    CommonData.clearLogFirst                = 0;
    CommonData.logFile.Clear();
    CommonData.attachFoundFault             = 0;
    CommonData.globaltimeout                = 30;

    CommonData.attachList                   = 0;

    CommonData.network_initialized          = 0;

#if SUPPORT_GSSAPI  //Added 2003-11-07 Joseph Calzaretta
    CommonData.gssapiAuthSupported          = 0;
    CommonData.authgssapi                   = 0;
    CommonData.mutualauth                   = 0;
    CommonData.bSuppressGssOptionsAtRuntime = 0;
    CommonData.servicename.Clear();
    CommonData.pGss                         = 0;
    CommonData.have_mechtype                = 0;
    CommonData.mechtype.Clear();
#endif

#if BLAT_LITE
#else
    CommonData.uuencode                     = 0;

    CommonData.base64                       = 0;

    CommonData.yEnc                         = 0;
    CommonData.deliveryStatusRequested      = 0;
    CommonData.deliveryStatusSupported      = 0;

    CommonData.uuencodeBytesLine            = 45;
    CommonData.eightBitMimeSupported        = 0;
    CommonData.eightBitMimeRequested        = 0;
    CommonData.binaryMimeSupported          = 0;
    //CommonData.binaryMimeRequested          = 0;

    CommonData.optionsFile.Clear();
    CommonData.optsFile                     = 0;
#endif

    CommonData.chunkingSupported            = 0;
    CommonData.utf                          = 0;

    CommonData.bodyFilename.Clear();
    CommonData.ConsoleDone                  = 0;
    CommonData.formattedContent             = 0;
    CommonData.mime                         = 0;
    CommonData.quiet                        = 0;
    CommonData.haveEmbedded                 = 0;
    CommonData.haveAttachments              = 0;
    CommonData.attach                       = 0;
    CommonData.regerr                       = 0;
    CommonData.bodyconvert                  = 0;

    CommonData.exitRequired                 = 0;

    ZeroMemory( CommonData.attachfile, sizeof(CommonData.attachfile) );
    CommonData.my_hostname_wanted.Clear();
    CommonData.logOut                       = 0;
    CommonData.fCgiWork                     = 0;
    CommonData.charset.Clear();

    CommonData.attachtype[0]                = 0;
    CommonData.timestamp                    = 0;

    CommonData.delimiterPrinted             = 0;
    CommonData.lastByteSent                 = 0;

    CommonData.dll_module_handle            = 0;
    CommonData.gensock_lib                  = 0;

    CommonData.my_hostname.Clear();

    CommonData.titleLinePrinted             = 0;

#if INCLUDE_NNTP
    CommonData.NNTPPort = defaultNNTPPort;
#endif
    CommonData.SMTPPort = defaultSMTPPort;
    CommonData.Try      =__T("1");                              // Was ONCE

#if SUPPORT_GSSAPI  //Added 2003-11-07 Joseph Calzaretta
    CommonData.mechtype = __T("UNKNOWN");
    CommonData.gss_protection_level = GSSAUTH_P_PRIVACY;
#endif

    CommonData.bodyconvert      = TRUE;
    CommonData.formattedContent = TRUE;


#if INCLUDE_POP3
    CommonData.POP3Login.Clear();
    CommonData.POP3Password.Clear();
#endif
#if INCLUDE_IMAP
    CommonData.IMAPLogin.Clear();
    CommonData.IMAPPassword.Clear();
#endif
    CommonData.Sender.Clear();
    CommonData.subject.Clear();
    CommonData.AUTHLogin.Clear();
    CommonData.AUTHPassword.Clear();

    CommonData.textmode = __T("plain");
}


int _tmain( int argc,             /* Number of strings in array argv          */
            LPTSTR *argv,         /* Array of command-line argument strings   */
            LPTSTR *envp )        /* Array of environment variables           */
{
    LPTSTR    * savedArguments;
    COMMON_DATA CommonData;
    int         i, j;
    int         retcode;
    WinFile     fileh;
    _TCHAR      boundary1[24];
    Buf         lpszMessageCgi;
    Buf         lpszCgiFailureUrl;
    Buf         lpszCgiSuccessUrl;
    Buf         lpszFirstReceivedData;
    Buf         lpszOtherHeader;
#if BLAT_LITE
#else
    LPTSTR    * secondArgV;
    int         secondArgC;
#endif
    Buf         temp;
    DWORD       filesize;
    DWORD       totalsize; // total size of all attachments and the message body.
    int         nbrOfAttachments;
    _TCHAR      multipartID[1200];
    int         x;
#if defined(_UNICODE) || defined(UNICODE)
  #if BLAT_LITE
    _TCHAR      savedMime;
  #else
    _TCHAR      savedEightBitMimeRequested;
  #endif
    int         savedUTF;
    Buf         sourceText;
    DWORD       dwVersion;
#endif

    ZeroMemory( &CommonData, sizeof(CommonData) );

#if 01
    for ( x = 1; x < argc; x++ ) {
        if ( 0 == _tcsicmp( argv[x], __T("-showCommandLineArguments") ) ) {
            _tprintf( __T("\nBlat saw the following command line options:\n") );
            for ( x = 0; x < argc; x++ ) {
                int y;
                _tprintf( __T("argv[%3i] = %s"), x, argv[x] );
                for ( y = 0; ; y++ ) {
                    if ( !(y % 16) )
                        _tprintf(__T("\n           "));

#if defined(_UNICODE) || defined(UNICODE)
                    _tprintf( __T(" %04X"), (_TUCHAR)argv[x][y] );
#else
                    _tprintf( __T(" %02X"), (_TUCHAR)argv[x][y] );
#endif
                    if ( argv[x][y] == 0 )
                        break;
                }
                _tprintf( __T("\n") );
            }
            _tprintf( __T("\n") );
            break;
        }
    }
#endif

#if INCLUDE_SUPERDEBUG
    for ( x = 1; x < argc; x++ ) {
        if ( 0 == _tcsicmp( argv[x], __T("-superDuperDebug") ) ) {
            CommonData.superDuperDebug = TRUE;
            CommonData.superDebug = 1;
            CommonData.debug = TRUE;
        }
    }
#endif

#if defined(_UNICODE) || defined(UNICODE)
    dwVersion = GetVersion();
    if ( dwVersion & (0x80ul << ((sizeof(DWORD)-1) * 8)) ) {
        //fprintf( stderr, "dwVersion = %08lX\n", dwVersion );
        fprintf( stderr, "This Unicode version of Blat cannot run in Windows earlier than Windows 2000.\n" \
                         "Please download and use Blat from the Win98 download folder on Yahoo! groups\n" \
                         "at http://tech.groups.yahoo.com/group/blat/files/Official/32%%20bit%%20versions/\n" );
        exit(14);
    }

    if ( blatBuildDate[0] == __T('\0') ) {
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
    }
#else
    _tcscpy( blatBuildDate, blatBuildDateA );
    _tcscpy( blatBuildTime, blatBuildTimeA );
#endif

#if INCLUDE_SUPERDEBUG
    if ( CommonData.superDuperDebug == TRUE ) {
        _TCHAR savedQuiet = CommonData.quiet;
        CommonData.quiet = FALSE;
        printMsg( CommonData, __T("superDuperDebug: calling InitializeCommonData() from line %i\n"), __LINE__ + 4 );
        CommonData.quiet = savedQuiet;
    }
#endif
    InitializeCommonData( CommonData );

#if INCLUDE_SUPERDEBUG
    if ( CommonData.superDuperDebug == TRUE ) {
        _TCHAR savedQuiet = CommonData.quiet;
        CommonData.quiet = FALSE;
        printMsg( CommonData, __T("superDuperDebug: back from InitializeCommonData()\n") );
        CommonData.quiet = savedQuiet;
    }
#endif

    if ( argc >= 2 ) {
#ifdef __WACTOMC__
        //
        // Attempt to determine if the second argument is the same as the tail end of the first argument, which might indicate
        // that the user has quotation marks between the path and executable file name.  For example:
        //      "C:\Program Files\NoInstallReqd\Blat"\blat.exe
        //
        // For Open Watcom version 1.9, the first argument (argv[0]) would be the whole path\filename, while the second
        // argument is just the characters following the closing quotation mark.
        //
        // The above command line is given to Blat as:
        //
        // argv[0] = C:\Program Files\NoInstallReqd\Blat\blat.exe
        // argv[1] = \blat.exe
        //
        size_t len = _tcslen( argv[1] );
        if ( 0 == memcmp( &argv[0][_tcslen(argv[0]) - len], argv[1], len * sizeof(_TCHAR) ) ) {
            for ( x = 1; x < argc; x++ )
                argv[x] = argv[x+1];

            argc--;
        }
#else
 #ifndef WIN64
        //
        // Attempt to determine if the second argument should be part of the first argument.  Using the example in the above
        // comments, Visual Studio 2010 (and newer) will give just the portion between quotation marks as the first agument,
        // and give the remainder as the second argument.  This is for 32-bit compiles, not for 64-bit compiles.
        //
        // The command line: "C:\Program Files\NoInstallReqd\Blat"\blat.exe
        // is given to Blat as:
        //
        // argv[0] = C:\Program Files\NoInstallReqd\Blat
        // argv[1] = \blat.exe
        //
        if ( (0 != _tcsicmp( &argv[0][_tcslen( argv[0] )-4], __T(".exe") )) && (0 != _tcsicmp( argv[0], __T("blat.dll") )) ) {
            if ( 0 == _tcsicmp( &argv[1][_tcslen( argv[1] )-4], __T(".exe") ) ) {
                Buf newArg0;

                newArg0 = argv[0];
                newArg0.Add( argv[1] );
                if ( 0 == _taccess( newArg0.Get(), 0 ) ) {

                    //printf( "old argv list\n" );
                    //for ( x = 0; x < (0x50 * sizeof(_TCHAR)); x++ ) {
                    //    if ( x && !(x % 16) )
                    //        _tprintf( __T("\n") );
                    //
                    //    _tprintf( __T(" %02X"), *(BYTE *)((BYTE *)argv + x) );
                    //}
                    //_tprintf( __T("\n") );

                    _tcscpy( argv[0], newArg0.Get() );

                    for ( x = 1; x < argc; x++ )
                        argv[x] = argv[x+1];

                    argc--;

                    //printf( "new argv list\n" );
                    //for ( x = 0; x < (0x50 * sizeof(_TCHAR)); x++ ) {
                    //    if ( x && !(x % 16) )
                    //        _tprintf( __T("\n") );
                    //
                    //    _tprintf( __T(" %02X"), *(BYTE *)((BYTE *)argv + x) );
                    //}
                    //_tprintf( __T("\n") );
                }
                newArg0.Free();
            }
        }
 #endif
#endif
    }
    savedArguments = (LPTSTR *) malloc( argc * sizeof(LPTSTR) );

    for ( i = 1; i < argc; i++ ) {
        savedArguments[i] = (LPTSTR) malloc( (_tcslen(argv[i]) + 1) * sizeof(_TCHAR) );
        _tcscpy( savedArguments[i], argv[i] );

#if defined(_UNICODE) || defined(UNICODE)
        sourceText = argv[i];
        CommonData.utf = 0;
        CommonData.charset.Clear();
        checkInputForUnicode( CommonData, sourceText );
        if ( CommonData.utf ) {
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
        sourceText.Free();
#endif
    }

    (void)envp;     // To remove compiler warnings.

#if INCLUDE_SUPERDEBUG
    if ( CommonData.superDuperDebug == TRUE ) {
        _TCHAR savedQuiet = CommonData.quiet;
        CommonData.quiet = FALSE;
        printMsg( CommonData, __T("superDuperDebug: calling InitializeCommonData() from line %i\n"), __LINE__ + 4 );
        CommonData.quiet = savedQuiet;
    }
#endif
    InitializeCommonData( CommonData );

#if INCLUDE_SUPERDEBUG
    if ( CommonData.superDuperDebug == TRUE ) {
        _TCHAR savedQuiet = CommonData.quiet;
        CommonData.quiet = FALSE;
        printMsg( CommonData, __T("superDuperDebug: back from InitializeCommonData()\n") );
        CommonData.quiet = savedQuiet;
    }
#endif
#if SUPPORT_GSSAPI
    // Check to see if the GSSAPI library is present by trying to initialize the global GssSession object.
    try {
        CommonData.pGss = &CommonData.TheSession;
    }
    catch (GssSession::GssNoLibrary&) {
        // If no library present, disallow the AUTH GSSAPI options
        CommonData.bSuppressGssOptionsAtRuntime = TRUE;
        CommonData.pGss = NULL;
    }
    catch (GssSession::GssException&) {
        // Silently fail if any other GssException shows up.  Only complain later
        // if someone actually tries to use AUTH GSSAPI.
        CommonData.pGss = NULL;
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
                CommonData.quiet    = TRUE;
                CommonData.fCgiWork = TRUE;
            }
        }
    }
/*
    else {
        _tprintf( __T("\nBlat saw the following command line:\n") );
        for ( x = 0; x < argc; x++ ) {
            if ( x )
                _tprintf( __T(" ") );

            if ( _tcschr(argv[x], __T(' ')) || _tcschr(argv[x], __T('"')) )
                _tprintf( __T("\"%s\""), argv[x] );
            else
                _tprintf( __T("%s"), argv[x] );
        }
        _tprintf( __T("\n\n") );
    }
 */

    if ( argc < 2 ) {
//        Must have at least a file name to send.
        printUsage( CommonData, NULL );
        free( savedArguments );
        return(1);
    }

#if BLAT_LITE
#else

    secondArgV = NULL;
    secondArgC = 0;
#endif
#if defined(_UNICODE) || defined(UNICODE)
    if ( argv[1][0] == 0x2013 )
        argv[1][0] = __T('-');
#endif
#if INCLUDE_SUPERDEBUG
    if ( CommonData.superDuperDebug == TRUE ) {
        _TCHAR savedQuiet = CommonData.quiet;
        CommonData.quiet = FALSE;
        printMsg( CommonData, __T("superDuperDebug: calling processOptions() near line %i\n"), __LINE__ + 4 );
        CommonData.quiet = savedQuiet;
    }
#endif
    if ( ((argv[1][0] == __T('-')) || (argv[1][0] == __T('/'))) && argv[1][1] )
        retcode = processOptions( CommonData, argc, argv, 1, TRUE );        // Preprocess the options
    else
        retcode = processOptions( CommonData, argc, argv, 2, TRUE );        // Preprocess the options

    // If the -install or -profile option was used,
    // then CommonData.exitRequired should be TRUE while retcode is 0.
    if ( !CommonData.exitRequired && !CommonData.fCgiWork )
        printTitleLine( CommonData, CommonData.quiet );

#if INCLUDE_SUPERDEBUG
    if ( CommonData.superDuperDebug == TRUE ) {
        _TCHAR savedQuiet = CommonData.quiet;
        CommonData.quiet = FALSE;
        printMsg( CommonData, __T("superDuperDebug: line %i, exitRequired = %i, retcode %i\n"), __LINE__, CommonData.exitRequired, retcode );
        CommonData.quiet = savedQuiet;
    }
#endif
    if ( CommonData.exitRequired || retcode ) {
        printMsg( CommonData, NULL );
        if ( CommonData.logOut )
            fclose(CommonData.logOut);

        cleanUpBuffers( CommonData, savedArguments, argc );
        return(retcode);
    }

#if BLAT_LITE
#else
    if ( CommonData.optionsFile.Get()[0] ) {

        size_t maxEntries = 2048;

        secondArgV = (LPTSTR*)malloc( (maxEntries + 1) * sizeof(LPTSTR) );
        if ( secondArgV ) {
            DWORD   dummy;
            LPTSTR  tmpstr;
            size_t  nextEntry = 0;

            memset( secondArgV, 0, (maxEntries + 1) * sizeof(LPTSTR) );
            if ( !fileh.OpenThisFile(CommonData.optionsFile.Get()) ) {
                free( secondArgV );
                printMsg( CommonData, __T("Options file \"%s\" not found or could not be opened.\n"), CommonData.optionsFile.Get() );

                printMsg( CommonData, NULL );
                if ( CommonData.logOut )
                    fclose(CommonData.logOut);

                cleanUpBuffers( CommonData, savedArguments, argc );
                return(2);
            }
            filesize = fileh.GetSize();
            tmpstr = (LPTSTR)malloc( (filesize + 1)*sizeof(_TCHAR) );
            if ( !tmpstr ) {
                fileh.Close();
                printMsg( CommonData, __T("error allocating memory for reading %s, aborting\n"), CommonData.optionsFile.Get() );
                if ( CommonData.logOut )
                    fclose(CommonData.logOut);

                cleanUpBuffers( CommonData, savedArguments, argc );
                return(2);
            }

            if ( !fileh.ReadThisFile(tmpstr, filesize, &dummy, NULL) ) {
                fileh.Close();
                free(tmpstr);
                printMsg( CommonData, __T("error reading %s, aborting\n"), CommonData.optionsFile.Get() );
                if ( CommonData.logOut )
                    fclose(CommonData.logOut);

                cleanUpBuffers( CommonData, savedArguments, argc );
                return(2);
            }
            fileh.Close();

            tmpstr[filesize] = __T('\0');

  #if defined(_UNICODE) || defined(UNICODE)
            savedEightBitMimeRequested = CommonData.eightBitMimeRequested;
            CommonData.eightBitMimeRequested = 0;

            savedUTF = CommonData.utf;
            CommonData.utf = 0;
            sourceText.Clear();
            sourceText.Add( tmpstr, filesize );
            checkInputForUnicode( CommonData, sourceText );
            free( tmpstr );

            tmpstr = (LPTSTR)malloc( (sourceText.Length() + 1)*sizeof(_TCHAR) );
            if ( !tmpstr ) {
                sourceText.Free();
                printMsg( CommonData, __T("error allocating memory for reading %s, aborting\n"), CommonData.optionsFile.Get() );
                if ( CommonData.logOut )
                    fclose(CommonData.logOut);

                cleanUpBuffers( CommonData, savedArguments, argc );
                return(2);
            }
            if (sourceText.Get()[0] == 0xFEFF)
                sourceText.Remove(0);
            memcpy( tmpstr, sourceText.Get(), sourceText.Length()*sizeof(_TCHAR) );
            tmpstr[sourceText.Length()] = __T('\0');

            CommonData.utf = savedUTF;
            CommonData.eightBitMimeRequested = savedEightBitMimeRequested;
  #endif
            nextEntry = make_argv( CommonData.commentChar,
                                   tmpstr,      /* argument list                     */
                                   secondArgV,  /* pointer to argv to use            */
                                   maxEntries,  /* maximum number of entries allowed */
                                   nextEntry,
                                   FALSE );
            secondArgC = (int)nextEntry;
        }
    }

    if ( secondArgC ) {
#if INCLUDE_SUPERDEBUG
        if ( CommonData.superDuperDebug == TRUE ) {
            _TCHAR savedQuiet = CommonData.quiet;
            CommonData.quiet = FALSE;
            printMsg( CommonData, __T("superDuperDebug: calling processOptions() at line %i\n"), __LINE__ + 4 );
            CommonData.quiet = savedQuiet;
        }
#endif
        retcode = processOptions( CommonData, secondArgC, secondArgV, 0, TRUE );
        if ( CommonData.exitRequired || retcode ) {
            for ( i = 0; secondArgV[ i ]; i++) {
                free( secondArgV[ i ] );
            }

            free( secondArgV );
            if ( retcode ) {
                printMsg( CommonData, NULL );
                if ( CommonData.logOut )
                    fclose(CommonData.logOut);
            }
            cleanUpBuffers( CommonData, savedArguments, argc );
            return(retcode);
        }
    }
#endif

    // get file name from argv[1]
    if ( (argv[1][0] != __T('-')) && (argv[1][0] != __T('/')) ) {
        CommonData.bodyFilename = argv[1];
    }
#if INCLUDE_SUPERDEBUG
    if ( CommonData.superDuperDebug == TRUE ) {
        _TCHAR savedQuiet = CommonData.quiet;
        CommonData.quiet = FALSE;
        printMsg( CommonData, __T("superDuperDebug: line %i, bodyFileName = >>%s<<\n"), __LINE__, CommonData.bodyFilename.Get() );
        CommonData.quiet = savedQuiet;
    }
#endif

    CommonData.regerr = GetRegEntry( CommonData, CommonData.Profile );

    CommonData.senderid  = CommonData.Sender;
    CommonData.loginname = CommonData.Sender;

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
#if INCLUDE_SUPERDEBUG
        if ( CommonData.superDuperDebug == TRUE ) {
            _TCHAR savedQuiet = CommonData.quiet;
            CommonData.quiet = FALSE;
            printMsg( CommonData, __T("superDuperDebug: calling processOptions() at line %i\n"), __LINE__ + 4 );
            CommonData.quiet = savedQuiet;
        }
#endif
        retcode = processOptions( CommonData, secondArgC, secondArgV, 0, FALSE );

        for ( i = 0; secondArgV[ i ]; i++) {
            free( secondArgV[ i ] );
        }

        free( secondArgV );
#if INCLUDE_SUPERDEBUG
        if ( CommonData.superDuperDebug == TRUE ) {
            _TCHAR savedQuiet = CommonData.quiet;
            CommonData.quiet = FALSE;
            printMsg( CommonData, __T("superDuperDebug: line %i, exitRequired = %i, retcode %i\n"), __LINE__, CommonData.exitRequired, retcode );
            CommonData.quiet = savedQuiet;
        }
#endif
        if ( CommonData.exitRequired || retcode ) {
            printMsg( CommonData, NULL );
            if ( CommonData.logOut )
                fclose(CommonData.logOut);

            cleanUpBuffers( CommonData, savedArguments, argc );
            return(retcode);
        }
    }
#endif

#if INCLUDE_SUPERDEBUG
        if ( CommonData.superDuperDebug == TRUE ) {
            _TCHAR savedQuiet = CommonData.quiet;
            CommonData.quiet = FALSE;
            printMsg( CommonData, __T("superDuperDebug: calling processOptions() near line %i\n"), __LINE__ + 4 );
            CommonData.quiet = savedQuiet;
        }
#endif
    if ( ((argv[1][0] == __T('-')) || (argv[1][0] == __T('/'))) && argv[1][1] )
        retcode = processOptions( CommonData, argc, argv, 1, FALSE );
    else
        retcode = processOptions( CommonData, argc, argv, 2, FALSE );

    if ( CommonData.logOut && CommonData.logCommands && !CommonData.usagePrinted ) {
        printMsg( CommonData, __T("%i command line argument%s received ...\n"), argc - 1, (argc != 2) ? __T("s") : __T("") );
        for ( x = 1; x < argc; x++ ) {
            if ( savedArguments[x] )
                printMsg( CommonData, __T("\t%s\n"), savedArguments[x] );
        }
    }

#if INCLUDE_SUPERDEBUG
    if ( CommonData.superDuperDebug == TRUE ) {
        _TCHAR savedQuiet = CommonData.quiet;
        CommonData.quiet = FALSE;
        printMsg( CommonData, __T("superDuperDebug: line %i, exitRequired = %i, retcode %i\n"), __LINE__, CommonData.exitRequired, retcode );
        CommonData.quiet = savedQuiet;
    }
#endif
    // If the -install or -profile option was used,
    // then CommonData.exitRequired should be TRUE while retcode is 0.
    if ( CommonData.exitRequired || retcode ) {
        printMsg( CommonData, NULL );
        if ( CommonData.logOut )
            fclose(CommonData.logOut);

        cleanUpBuffers( CommonData, savedArguments, argc );
        return(retcode);
    }

#if INCLUDE_NNTP
    if ( CommonData.regerr == 12 )
        if ( !CommonData.loginname.Get()[0] || (!CommonData.SMTPHost.Get()[0] && !CommonData.NNTPHost.Get()[0]) )
            printMsg( CommonData, __T("Failed to open registry key for Blat\n") );
#else
    if ( CommonData.regerr == 12 )
        if ( !CommonData.loginname.Get()[0] || !CommonData.SMTPHost.Get()[0] )
            printMsg( CommonData, __T("Failed to open registry key for Blat\n") );
#endif

    // if we are not impersonating loginname is the same as the sender
    if ( ! CommonData.impersonating )
        CommonData.senderid = CommonData.loginname;

    // fixing the argument parsing
    // Ends here

#if INCLUDE_NNTP
    if ( !CommonData.loginname.Get()[0] || (!CommonData.SMTPHost.Get()[0] && !CommonData.NNTPHost.Get()[0]) ) {
        printMsg( CommonData, __T("To set the SMTP server's name/address and your username/email address for that\n") \
                              __T("server machine do:\n") \
                              __T("blat -install  server_name  your_email_address\n") \
                              __T("or use '-server <server_name>' and '-f <your_email_address>'\n") );
        printMsg( CommonData, __T("aborting, nothing sent\n") );

        printMsg( CommonData, NULL );
        if ( CommonData.logOut )
            fclose(CommonData.logOut);

        cleanUpBuffers( CommonData, savedArguments, argc );
        return(12);
    }
#else
    if ( !CommonData.loginname.Get()[0] || !CommonData.SMTPHost.Get()[0] ) {
        printMsg( CommonData, __T("To set the SMTP server's name/address and your username/email address for that\n") \
                              __T("server machine do:\n") \
                              __T("blat -install  server_name  your_email_address\n") \
                              __T("or use '-server <server_name>' and '-f <your_email_address>'\n") );
        printMsg( CommonData, __T("aborting, nothing sent\n") );

        printMsg( CommonData, NULL );
        if ( CommonData.logOut )
            fclose(CommonData.logOut);

        cleanUpBuffers( CommonData, savedArguments, argc );
        return(12);
    }
#endif

    // make sure filename exists, get full pathname
    if ( CommonData.bodyFilename.Get()[0] && (_tcscmp(CommonData.bodyFilename.Get(), __T("-")) != 0) ) {
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

#if INCLUDE_SUPERDEBUG
            if ( CommonData.superDuperDebug == TRUE ) {
                _TCHAR savedQuiet = CommonData.quiet;
                CommonData.quiet = FALSE;
                printMsg( CommonData, __T("superDuperDebug: line %i, bodyFileName = >>%s<<\n"), __LINE__, CommonData.bodyFilename.Get() );
                CommonData.quiet = savedQuiet;
            }
#endif
            fh = CreateFile( CommonData.bodyFilename.Get(), FILE_READ_DATA, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
#if INCLUDE_SUPERDEBUG
            if ( CommonData.superDuperDebug == TRUE ) {
                _TCHAR savedQuiet = CommonData.quiet;
                CommonData.quiet = FALSE;
                printMsg( CommonData, __T("superDuperDebug: line %i, file handle = %i\n"), __LINE__, fh );
                CommonData.quiet = savedQuiet;
            }
#endif
            if ( fh == INVALID_HANDLE_VALUE ) {
                DWORD lastError = GetLastError();
                if ( lastError == 2 )
                    printMsg(CommonData, __T("%s does not exist\n"), CommonData.bodyFilename.Get());
                else
                    printMsg(CommonData, __T("unknown error code %lu when trying to open %s\n"), lastError, CommonData.bodyFilename.Get());

                printMsg( CommonData, NULL );
                if ( CommonData.logOut )
                    fclose(CommonData.logOut);

                cleanUpBuffers( CommonData, savedArguments, argc );
                return(2);
            }
            CloseHandle(fh);
        } else {                        // Windows 95 through NT 4.0
            FILE * fh;

            fh = _tfopen(CommonData.bodyFilename.Get(), __T("r"));
            if ( fh == NULL ) {
                printMsg(CommonData, __T("%s does not exist\n"),CommonData.bodyFilename.Get());

                printMsg( CommonData, NULL );
                if ( CommonData.logOut )
                    fclose(CommonData.logOut);

                cleanUpBuffers( CommonData, savedArguments, argc );
                return(2);
            }
            fclose(fh);
        }
    }

    shrinkWhiteSpace( CommonData.bcc_list );
    shrinkWhiteSpace( CommonData.cc_list );
    shrinkWhiteSpace( CommonData.destination );

    searchReplaceEmailKeyword( CommonData, CommonData.bcc_list );
    searchReplaceEmailKeyword( CommonData, CommonData.cc_list );
    searchReplaceEmailKeyword( CommonData, CommonData.destination );

    // build temporary recipients list for parsing the "To:" line
    // build the recipients list
    temp.Alloc(                  CommonData.destination.Length() + CommonData.cc_list.Length() + CommonData.bcc_list.Length() + 4 );
    CommonData.Recipients.Alloc( CommonData.destination.Length() + CommonData.cc_list.Length() + CommonData.bcc_list.Length() + 4 );

    // Parse the "To:" line
    for ( i = j = 0; (unsigned)i < CommonData.destination.Length(); i++ ) {
        // strip white space
        // NOT! while ( CommonData.destination[i] == __T(' ') )   i++;
        // look for comments in brackets, and omit
        if ( CommonData.destination.Get()[i] == __T('(') ) {
            while ( CommonData.destination.Get()[i] != __T(')') )   i++;
            i++;
        }
        temp.Get()[j++] = CommonData.destination.Get()[i];
    }
    temp.Get()[j] = __T('\0');                              // End of list added!
    CommonData.Recipients.Add(temp.Get());

    // Parse the "Cc:" line
    for ( i = j = 0; (unsigned)i < CommonData.cc_list.Length(); i++ ) {
        // strip white space
        // NOT! while ( CommonData.cc_list[i] == __T(' ') ) i++;
        // look for comments in brackets, and omit
        if ( CommonData.cc_list.Get()[i] == __T('(') ) {
            while ( CommonData.cc_list.Get()[i] != __T(')') ) i++;
            i++;
        }
        temp.Get()[j++] = CommonData.cc_list.Get()[i];
    }
    temp.Get()[j] = __T('\0');                              // End of list added!
    if ( CommonData.cc_list.Length() ) {
        if ( CommonData.Recipients.Length() )
            CommonData.Recipients.Add( __T(',') );

        CommonData.Recipients.Add(temp.Get());
    }

    // Parse the "Bcc:" line
    for (i = j = 0; (unsigned)i < CommonData.bcc_list.Length(); i++) {
        // strip white space
        // NOT! while ( CommonData.bcc_list[i] == __T(' ') )  i++;
        // look for comments in brackets, and omit
        if (CommonData.bcc_list.Get()[i] == __T('(')) {
            while (CommonData.bcc_list.Get()[i] != __T(')')) i++;
            i++;
        }
        temp.Get()[j++] = CommonData.bcc_list.Get()[i];
    }
    temp.Get()[j] = __T('\0');                              // End of list added!
    if ( CommonData.bcc_list.Length() > 0 ) {
        if ( CommonData.Recipients.Length() )
            CommonData.Recipients.Add( __T(',') );

        CommonData.Recipients.Add(temp.Get());
    }

#if INCLUDE_NNTP
    if ( !CommonData.Recipients.Length() && !CommonData.groups.Length() ) {
        printMsg( CommonData, __T("No target email address or newsgroup was specified.  You must give an email\n") \
                              __T("address or usenet newsgroup to send messages to.  Use -to, -cc, or -bcc option\n") \
                              __T("for email, or -groups for usenet.\n") \
                              __T("Aborting, nobody to send messages to.\n") );

        printMsg( CommonData, NULL );
        if ( CommonData.logOut )
            fclose(CommonData.logOut);

        cleanUpBuffers( CommonData, savedArguments, argc );
        return(12);
    }
#else
    if ( !CommonData.Recipients.Length() ) {
        printMsg( CommonData, __T("No target email address was specified.  You must give an email address\n") \
                              __T("to send messages to.  Use -to, -cc, or -bcc option.\n") \
                              __T("Aborting, nobody to send messages to.\n") );

        printMsg( CommonData, NULL );
        if ( CommonData.logOut )
            fclose(CommonData.logOut);

        cleanUpBuffers( CommonData, savedArguments, argc );
        return(12);
    }
#endif

    // if reading from the console, read everything into a temporary file first
    CommonData.ConsoleDone = FALSE;
    if ( (CommonData.bodyFilename.Get()[0] == __T('\0')) || (_tcscmp(CommonData.bodyFilename.Get(), __T("-")) == 0) ) {

        if ( lpszMessageCgi.Length() ) {
            CommonData.ConsoleDone = TRUE;
            CommonData.TempConsole.Add(lpszMessageCgi);
        } else
        if ( CommonData.bodyparameter.Length() ) {
            LPTSTR p = CommonData.bodyparameter.Get();
            CommonData.ConsoleDone = TRUE;
            if (CommonData.bodyconvert) {
                i = 0;
                while ( p[i] ) {
                    if ( p[i] == __T('|') )      // CRLF signified by the pipe character
                        CommonData.TempConsole.Add( __T("\r\n") );
                    else
                        CommonData.TempConsole.Add( p[i] );
                    i++;
                }
            } else
                CommonData.TempConsole.Add( p );
        } else {
            CommonData.ConsoleDone = TRUE;
            for ( ; ; ) {
                DWORD dwNbRead = 0;
                i=0;

                if ( !ReadFile(GetStdHandle(STD_INPUT_HANDLE),&i,1,&dwNbRead,NULL) ||
                     !dwNbRead || (i == __T('\x1A')) )
                    break;

                CommonData.TempConsole.Add((char)i);
            }
            if (CommonData.TempConsole.Length() == 0)
                CommonData.TempConsole.Add( __T("\r\n") );
        }
        CommonData.bodyFilename = stdinFileName;
#if INCLUDE_SUPERDEBUG
        if ( CommonData.superDuperDebug == TRUE ) {
            _TCHAR savedQuiet = CommonData.quiet;
            CommonData.quiet = FALSE;
            printMsg( CommonData, __T("superDuperDebug: line %i, bodyFileName = >>%s<<\n"), __LINE__, CommonData.bodyFilename.Get() );
            CommonData.quiet = savedQuiet;
        }
#endif
    }

    if (!CommonData.ConsoleDone) {
        DWORD dummy;

#if INCLUDE_SUPERDEBUG
        if ( CommonData.superDuperDebug == TRUE ) {
            _TCHAR savedQuiet = CommonData.quiet;
            CommonData.quiet = FALSE;
            printMsg( CommonData, __T("superDuperDebug: line %i, attempting to open bodyFileName = >>%s<<\n"), __LINE__, CommonData.bodyFilename.Get() );
            CommonData.quiet = savedQuiet;
        }
#endif
        //get the text of the file into a string buffer
        if ( !fileh.OpenThisFile(CommonData.bodyFilename.Get()) ) {
            printMsg(CommonData, __T("error reading %s, aborting\n"),CommonData.bodyFilename.Get());

            printMsg( CommonData, NULL );
            if ( CommonData.logOut )
                fclose(CommonData.logOut);

            cleanUpBuffers( CommonData, savedArguments, argc );
            return(3);
        }
        if ( !fileh.IsDiskFile() ) {
            fileh.Close();
            printMsg(CommonData, __T("Sorry, I can only mail messages from disk files...\n"));

            printMsg( CommonData, NULL );
            if ( CommonData.logOut )
                fclose(CommonData.logOut);

            cleanUpBuffers( CommonData, savedArguments, argc );
            return(4);
        }
        filesize = fileh.GetSize();
#if INCLUDE_SUPERDEBUG
        if ( CommonData.superDuperDebug == TRUE ) {
            _TCHAR savedQuiet = CommonData.quiet;
            CommonData.quiet = FALSE;
            printMsg( CommonData, __T("superDuperDebug: line %i, filesize = %lu\n"), __LINE__, filesize );
            CommonData.quiet = savedQuiet;
        }
#endif
        CommonData.TempConsole.Clear();
        if ( filesize ) {
            CommonData.TempConsole.Alloc( filesize + 1 );
#if INCLUDE_SUPERDEBUG
        if ( CommonData.superDuperDebug == TRUE ) {
            _TCHAR savedQuiet = CommonData.quiet;
            CommonData.quiet = FALSE;
            printMsg( CommonData, __T("superDuperDebug: line %i, calling ReadThisFile()\n"), __LINE__ );
            CommonData.quiet = savedQuiet;
        }
#endif
            retcode = fileh.ReadThisFile(CommonData.TempConsole.Get(), filesize, &dummy, NULL);
#if INCLUDE_SUPERDEBUG
        if ( CommonData.superDuperDebug == TRUE ) {
            _TCHAR savedQuiet = CommonData.quiet;
            CommonData.quiet = FALSE;
            printMsg( CommonData, __T("superDuperDebug: line %i, ReadThisFile() returned %i\n"), __LINE__, retcode );
            CommonData.quiet = savedQuiet;
        }
#endif
            CommonData.TempConsole.SetLength( filesize );
            *CommonData.TempConsole.GetTail() = __T('\0');
        } else {
            filesize = 1;
            CommonData.TempConsole = __T("\n");
            retcode = true;
        }
        fileh.Close();

        if ( !retcode ) {
            printMsg(CommonData, __T("error reading %s, aborting\n"), CommonData.bodyFilename.Get());
            cleanUpBuffers( CommonData, savedArguments, argc );
            return(5);
        }
    }

#if defined(_UNICODE) || defined(UNICODE)
  #if BLAT_LITE
    savedMime = CommonData.mime;
  #else
    savedEightBitMimeRequested = CommonData.eightBitMimeRequested;
    CommonData.eightBitMimeRequested = 0;
  #endif

    savedUTF = CommonData.utf;
    CommonData.utf = 0;
    checkInputForUnicode ( CommonData, CommonData.TempConsole );
    if ( CommonData.utf == UTF_REQUESTED ) {
        if ( CommonData.charset.Get()[0] == __T('\0') )
            CommonData.charset = __T("utf-8");              // Set to lowercase to distinguish between our determination and user specified.
    } else {
  #if BLAT_LITE
        CommonData.mime = savedMime;
  #else
        CommonData.eightBitMimeRequested = savedEightBitMimeRequested;
  #endif
        CommonData.utf = savedUTF;
    }
#endif
    filesize = (DWORD)CommonData.TempConsole.Length();

    CommonData.attachFoundFault = FALSE;
    nbrOfAttachments = collectAttachmentInfo( CommonData, totalsize, filesize );
    if ( CommonData.attachFoundFault ) {
        printMsg( CommonData, __T("One or more attachments had not been found.\n") \
                              __T("Aborting, so you find / fix the missing attachment.\n") );

        printMsg( CommonData, NULL );
        if ( CommonData.logOut )
            fclose(CommonData.logOut);

        cleanUpBuffers( CommonData, savedArguments, argc );
        return(12);
    }
    if ( nbrOfAttachments == 0 ) {
        CommonData.haveEmbedded    = FALSE;
        CommonData.haveAttachments = FALSE;
        CommonData.attach = 0;
        CommonData.attachfile[0].Clear();
        CommonData.attachtype[0] = 0;
    }

    if ( nbrOfAttachments && !totalsize ) {
        printMsg( CommonData, __T("Sum total size of all attachments exceeds 4G bytes.  This is too much to be\n") \
                              __T("sending with SMTP or NNTP.  Please try sending through FTP instead.\n") \
                              __T("Aborting, too much data to send.\n") );

        printMsg( CommonData, NULL );
        if ( CommonData.logOut )
            fclose(CommonData.logOut);

        cleanUpBuffers( CommonData, savedArguments, argc );
        return(12);
    }

#if BLAT_LITE
#else
    if ( CommonData.base64 )
        CommonData.formattedContent = TRUE;
#endif

    // supply the message body size, in case of sending multipart messages.
    retcode = send_email( CommonData, filesize, lpszFirstReceivedData, lpszOtherHeader,
                          boundary1, multipartID, nbrOfAttachments, totalsize );
#if INCLUDE_NNTP
    int ret;

    // supply the message body size, in case of sending multipart messages.
    ret = send_news ( CommonData, filesize, lpszFirstReceivedData, lpszOtherHeader,
                      boundary1, multipartID, nbrOfAttachments, totalsize );
    if ( !retcode )
        retcode = ret;
#endif
    releaseAttachmentInfo( CommonData );

    cleanUpBuffers( CommonData, savedArguments, argc );

    if (  CommonData.fCgiWork ) {
        Buf    lpszCgiText;
        LPTSTR lpszUrl;
        DWORD  dwLenUrl;
        DWORD  dwSize;

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

    printMsg( CommonData, NULL );
    if ( CommonData.logOut )                                // Added 23 Aug 2000 Craig Morrison
        fclose(CommonData.logOut);

    CommonData.logOut = NULL;

    if ( retcode < 0 )
        retcode = 0 - retcode;

    return( retcode );
}

#ifdef BLATDLL_EXPORTS // this is blat.dll, not blat.exe

#ifdef BLATDLL_TC_WCX
#define BLATDLL_API

extern tProcessDataProcW pProcessDataProcW = 0;
extern tProcessDataProc pProcessDataProc = 0;
#else
#define BLATDLL_API __declspec(dllexport)
#endif

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
            iCount = make_argv( __T(';'),
                                sIn,        /* argument list                     */
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

    int byteCount = WideCharToMultiByte( CP_ACP, 0, sCmd, -1, NULL, 0, NULL, NULL );
    if ( byteCount > 1 ) {
        char * pCharCmd = (char *) new char[byteCount+1];
        if ( pCharCmd ) {
            WideCharToMultiByte( CP_ACP, 0, sCmd, -1, pCharCmd, byteCount, NULL, NULL );

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

    int byteCount = MultiByteToWideChar( CP_ACP, 0, sCmd, -1, NULL, 0 );
    if ( byteCount > 1 ) {
        wchar_t * pWCharCmd = (wchar_t *) new wchar_t[(size_t)byteCount+1];
        if ( pWCharCmd ) {
            MultiByteToWideChar( CP_ACP, 0, sCmd, -1, pWCharCmd, byteCount );

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
    (void)hModule;              // Remove compiler warnings
    (void)lpReserved;           // Remove compiler warnings
    (void)ul_reason_for_call;   // Remove compiler warnings

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
            byteCount = WideCharToMultiByte( CP_ACP, 0, argv[x], -1, NULL, 0, NULL, NULL );
            if ( byteCount > 1 ) {
                newArgv[x] = (char *) new char[byteCount+1];
                if ( newArgv[x] ) {
                    WideCharToMultiByte( CP_ACP, 0, argv[x], -1, newArgv[x], byteCount, NULL, NULL );
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
            byteCount = MultiByteToWideChar( CP_ACP, 0, argv[x], -1, NULL, 0 );
            if ( byteCount > 1 ) {
                newArgv[x] = (wchar_t *) new wchar_t[(size_t)byteCount+1];
                if ( newArgv[x] ) {
                    MultiByteToWideChar( CP_ACP, 0, argv[x], -1, newArgv[x], byteCount );
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
        int byteCount = MultiByteToWideChar( CP_ACP, 0, (LPSTR)pString, -1, NULL, 0 );
        if ( byteCount > 1 ) {
            LPWSTR pWCharString = (LPWSTR) new wchar_t[byteCount+1];
            if ( pWCharString ) {
                MultiByteToWideChar( CP_ACP, 0, (LPSTR)pString, -1, pWCharString, byteCount );

                pPrintDLL( (LPTSTR)pWCharString );
            }
            delete [] pWCharString;
        }
#endif
    } else
        _fputts( pString, stdout );
}

static void printDLLA(LPTSTR pString) {

    if (pPrintDLL) {
#if defined(_UNICODE) || defined(UNICODE)
        int byteCount = WideCharToMultiByte( CP_ACP, 0, (LPWSTR)pString, -1, NULL, 0, NULL, NULL );
        if ( byteCount > 1 ) {
            LPSTR pCharString = (LPSTR) new char[(size_t)byteCount+1];
            if ( pCharString ) {
                WideCharToMultiByte( CP_ACP, 0, (LPWSTR)pString, -1, pCharString, byteCount, NULL, NULL );

                pPrintDLL( (LPTSTR)pCharString );
            }
            delete [] pCharString;
        }
#else
        pPrintDLL( (LPTSTR)pString );
#endif
    } else
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

// additional exported function SetProcessDataProcW
BLATDLL_API void _stdcall SetProcessDataProcW(tProcessDataProcW pProcessDataProcW1)
{
#ifdef BLATDLL_TC_WCX
    pProcessDataProcW = pProcessDataProcW1;
#else
    UNREFERENCED_PARAMETER( pProcessDataProcW1 );
#endif
}

BLATDLL_API void _stdcall SetProcessDataProc(tProcessDataProc pProcessDataProc1)
{
#ifdef BLATDLL_TC_WCX
    pProcessDataProc = pProcessDataProc1;
#else
    UNREFERENCED_PARAMETER( pProcessDataProc1 );
#endif
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

void printMsg(COMMON_DATA & CommonData, LPTSTR p, ... )
{
    time_t      nowtime;
    struct tm * localT;
    _TCHAR      buf[2048];
    va_list     args;
    int         x, y;
    _TCHAR      timeBuffer[32];

    if ( CommonData.quiet && !CommonData.logOut )
        return;

    if ( CommonData.fCgiWork && !CommonData.logOut ) {
#if INCLUDE_SUPERDEBUG
        CommonData.superDebug = FALSE;
#endif
        CommonData.debug = FALSE;
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
        if ( CommonData.logOut ) {
            if ( CommonData.lastByteSent != __T('\n') )
                _ftprintf( CommonData.logOut, __T("\n") );

            _ftprintf( CommonData.logOut, __T("%s-------------End of Session------------------\n"), timeBuffer );
            fflush( CommonData.logOut );
            if ( CommonData.logOut != stdout ) {
                fclose( CommonData.logOut );

                if (CommonData.logFile.Length())
                    CommonData.logOut = _tfopen(CommonData.logFile.Get(), fileAppendAttribute);
                else
                    CommonData.logOut = NULL;
            }
            CommonData.delimiterPrinted = FALSE;
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

    if ( (CommonData.charset.Get() != NULL) && (_tcsicmp( CommonData.charset.Get(), __T("UTF-8") ) == 0) ) {
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
            else {
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
        CommonData.lastByteSent = buf[y - 1];

        if ( CommonData.logOut ) {
            if ( !CommonData.delimiterPrinted ) {
                _ftprintf( CommonData.logOut, __T("\n%s------------Start of Session-----------------\n"), timeBuffer );
                CommonData.delimiterPrinted = TRUE;
            }

            if ( CommonData.timestamp )
                _ftprintf( CommonData.logOut, __T("%s: "), timeBuffer );

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
            _ftprintf(CommonData.logOut, __T("%s"), buf);
            fflush( CommonData.logOut );
            if ( CommonData.logOut != stdout ) {
                fclose( CommonData.logOut );
                CommonData.logOut = _tfopen(CommonData.logFile.Get(), fileAppendAttribute);
            }
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
