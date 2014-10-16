/*
    options.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "blat.h"
#include "winfile.h"
#include "gensock.h"

#if SUPPORT_GSSAPI
#include "gssfuncs.h" // Please read the comments here for information about how to use GssSession
  extern BOOL    authgssapi;
  extern BOOL    mutualauth;
  extern _TCHAR  servicename[SERVICENAME_SIZE];
  extern protLev gss_protection_level;
  extern BOOL    bSuppressGssOptionsAtRuntime;
#endif

extern _TCHAR    fileCreateAttribute[];
extern _TCHAR    fileAppendAttribute[];

#ifdef BLATDLL_EXPORTS // this is blat.dll, not blat.exe
#if defined(__cplusplus)
extern "C" {
#endif
extern void (*pMyPrintDLL)(LPTSTR);
#if defined(__cplusplus)
}
#endif
#else
#define pMyPrintDLL _tprintf
#endif

extern int  CreateRegEntry( int useHKCU );
extern int  DeleteRegEntry( LPTSTR pstrProfile, int useHKCU );
extern void ShowRegHelp( void );
extern void ListProfiles( LPTSTR pstrProfile );
extern void parseCommaDelimitString ( LPTSTR source, Buf & parsed_addys, int pathNames );
extern void convertUnicode( Buf &sourceText, int * utf, LPTSTR charset, int utfRequested );

extern void printMsg( LPTSTR p, ... );              // Added 23 Aug 2000 Craig Morrison

extern _TCHAR       blatVersion[];
extern _TCHAR       blatVersionSuf[];
extern _TCHAR       blatBuildDate[];
extern _TCHAR       blatBuildTime[];

extern _TCHAR       Profile[TRY_SIZE+1];
extern _TCHAR       priority[2];
extern _TCHAR       sensitivity[2];

extern _TCHAR       commentChar;
extern _TCHAR       impersonating;
extern _TCHAR       returnreceipt;
extern _TCHAR       disposition;
extern _TCHAR       ssubject;
extern _TCHAR       includeUserAgent;
extern _TCHAR       sendUndisclosed;
extern _TCHAR       clearLogFirst;
extern _TCHAR       logFile[_MAX_PATH];

extern int          utf;
#if BLAT_LITE
#else
extern _TCHAR       noheader;
extern unsigned int uuencodeBytesLine;
extern _TCHAR       uuencode;
extern _TCHAR       base64;
extern _TCHAR       yEnc;
extern _TCHAR       deliveryStatusRequested;
extern _TCHAR       eightBitMimeRequested;
extern _TCHAR       forcedHeaderEncoding;

extern _TCHAR       organization[ORG_SIZE+1];
extern _TCHAR       xheaders[DEST_SIZE+1];
extern _TCHAR       aheaders1[DEST_SIZE+1];
extern _TCHAR       aheaders2[DEST_SIZE+1];
extern _TCHAR       optionsFile[_MAX_PATH];
extern Buf          userContentType;
extern _TCHAR       ByPassCRAM_MD5;
#endif
#if SUPPORT_SIGNATURES
extern Buf          signature;
#endif
#if SUPPORT_TAGLINES
extern Buf          tagline;
#endif
#if SUPPORT_POSTSCRIPTS
extern Buf          postscript;
#endif

extern _TCHAR       SMTPHost[SERVER_SIZE+1];
extern _TCHAR       SMTPPort[SERVER_SIZE+1];
extern LPTSTR       defaultSMTPPort;
#if SUPPORT_GSSAPI
  #if SUBMISSION_PORT
extern LPTSTR       SubmissionPort;
  #endif
#endif

#if INCLUDE_NNTP
extern _TCHAR       NNTPHost[SERVER_SIZE+1];
extern _TCHAR       NNTPPort[SERVER_SIZE+1];
extern LPTSTR       defaultNNTPPort;
extern Buf          groups;
#endif

#if INCLUDE_POP3
extern _TCHAR       POP3Host[SERVER_SIZE+1];
extern _TCHAR       POP3Port[SERVER_SIZE+1];
extern LPTSTR       defaultPOP3Port;
extern Buf          POP3Login;
extern Buf          POP3Password;
extern _TCHAR       xtnd_xmit_wanted;
#endif
#if INCLUDE_IMAP
extern _TCHAR       IMAPHost[SERVER_SIZE+1];
extern _TCHAR       IMAPPort[SERVER_SIZE+1];
extern LPTSTR       defaultIMAPPort;
extern Buf          IMAPLogin;
extern Buf          IMAPPassword;
#endif

extern Buf          AUTHLogin;
extern Buf          AUTHPassword;
extern _TCHAR       Try[TRY_SIZE+1];
extern Buf          Sender;
extern Buf          destination;
extern Buf          cc_list;
extern Buf          bcc_list;
extern _TCHAR       loginname[SENDER_SIZE+1];       // RFC 821 MAIL From. <loginname>. There are some inconsistencies in usage
extern _TCHAR       senderid[SENDER_SIZE+1];        // Inconsistent use in Blat for some RFC 822 Field definitions
extern _TCHAR       sendername[SENDER_SIZE+1];      // RFC 822 Sender: <sendername>
extern _TCHAR       fromid[SENDER_SIZE+1];          // RFC 822 From: <fromid>
extern _TCHAR       replytoid[SENDER_SIZE+1];       // RFC 822 Reply-To: <replytoid>
extern _TCHAR       returnpathid[SENDER_SIZE+1];    // RFC 822 Return-Path: <returnpath>
extern Buf          subject;
extern _TCHAR       textmode[TEXTMODE_SIZE+1];      // added 15 June 1999 by James Greene "greene@gucc.org"
extern _TCHAR       bodyFilename[_MAX_PATH];
extern Buf          bodyparameter;
extern _TCHAR       formattedContent;
extern _TCHAR       mime;
extern _TCHAR       quiet;
extern _TCHAR       debug;
extern int          attach;
extern int          regerr;
extern _TCHAR       bodyconvert;
extern _TCHAR       haveEmbedded;
extern _TCHAR       haveAttachments;
extern int          maxNames;

static Buf          processedOptions;

extern _TCHAR       attachfile[64][_MAX_PATH+1];
extern _TCHAR       my_hostname_wanted[1024];
extern FILE *       logOut;
extern _TCHAR       charset[40];            // Added 25 Apr 2001 Tim Charron (default ISO-8859-1)

extern _TCHAR       attachtype[64];
extern _TCHAR       timestamp;
extern Buf          alternateText;
extern int          delayBetweenMsgs;

extern int          exitRequired;

_TCHAR strInstall[]       = __T("-install");
_TCHAR strSaveSettings[]  = __T("-SaveSettings");
#if BLAT_LITE
#else
_TCHAR strInstallSMTP[]   = __T("-installSMTP");
  #if INCLUDE_NNTP
_TCHAR strInstallNNTP[]   = __T("-installNNTP");
  #endif
  #if INCLUDE_POP3
_TCHAR strInstallPOP3[]   = __T("-installPOP3");
  #endif
  #if INCLUDE_IMAP
_TCHAR strInstallIMAP[]   = __T("-installIMAP");
  #endif
#endif

_TCHAR strP[]             = __T("-p");
_TCHAR strProfile[]       = __T("-profile");
_TCHAR strServer[]        = __T("-server");
#if BLAT_LITE
#else
_TCHAR strServerSMTP[]    = __T("-serverSMTP");
  #if INCLUDE_NNTP
_TCHAR strServerNNTP[]    = __T("-serverNNTP");
  #endif
  #if INCLUDE_POP3
_TCHAR strServerPOP3[]    = __T("-serverPOP3");
  #endif
  #if INCLUDE_IMAP
_TCHAR strServerIMAP[]    = __T("-serverIMAP");
  #endif
#endif
_TCHAR strF[]             = __T("-f");
_TCHAR strI[]             = __T("-i");
_TCHAR strPort[]          = __T("-port");
#if BLAT_LITE
#else
_TCHAR strPortSMTP[]      = __T("-portSMTP");
  #if INCLUDE_NNTP
_TCHAR strPortNNTP[]      = __T("-portNNTP");
  #endif
  #if INCLUDE_POP3
_TCHAR strPortPOP3[]      = __T("-portPOP3");
  #endif
  #if INCLUDE_IMAP
_TCHAR strPortIMAP[]      = __T("-portIMAP");
  #endif
#endif
_TCHAR strUsername[]      = __T("-username");
_TCHAR strU[]             = __T("-u");
_TCHAR strPassword[]      = __T("-password");
_TCHAR strPwd[]           = __T("-pwd");
_TCHAR strPw[]            = __T("-pw");
#if INCLUDE_POP3
_TCHAR strPop3Username[]  = __T("-pop3username");
_TCHAR strPop3U[]         = __T("-pu");
_TCHAR strPop3Password[]  = __T("-pop3password");
_TCHAR strPop3Pw[]        = __T("-ppw");
#endif
#if INCLUDE_IMAP
_TCHAR strImapUsername[]  = __T("-imapusername");
_TCHAR strImapU[]         = __T("-iu");
_TCHAR strImapPassword[]  = __T("-imappassword");
_TCHAR strImapPw[]        = __T("-ipw");
#endif
_TCHAR strTry[]           = __T("-try");
_TCHAR strMailFrom[]      = __T("-mailfrom");
_TCHAR strFrom[]          = __T("-from");
_TCHAR strSender[]        = __T("-sender");


#if INCLUDE_SUPERDEBUG
_TCHAR superDebug = FALSE;
#endif

int  printUsage( int optionPtr );

#if BLAT_LITE
#else

static int ReadNamesFromFile(LPTSTR type, LPTSTR namesfilename, Buf &listofnames) {
    WinFile fileh;
    int     found;
    DWORD   filesize;
    DWORD   dummy;
    Buf     p;
    LPTSTR  tmpstr;

    if ( !fileh.OpenThisFile(namesfilename)) {
        printMsg(__T("error opening %s, aborting\n"), namesfilename);
        return(3);
    }
    filesize = fileh.GetSize();
    tmpstr = (LPTSTR)malloc( (filesize + 1)*sizeof(_TCHAR) );
    if ( !tmpstr ) {
        fileh.Close();
        printMsg(__T("error allocating memory for reading %s, aborting\n"), namesfilename);
        return(5);
    }

    if ( !fileh.ReadThisFile(tmpstr, filesize, &dummy, NULL) ) {
        fileh.Close();
        free(tmpstr);
        printMsg(__T("error reading %s, aborting\n"), namesfilename);
        return(5);
    }
    fileh.Close();

    tmpstr[filesize] = __T('\0');
  #if defined(_UNICODE) || defined(UNICODE)
    Buf    sourceText;
    int    utf;

    utf = 0;
    sourceText.Add( tmpstr, filesize );
    convertUnicode( sourceText, &utf, NULL, 8 );
    if ( utf )
        _tcscpy( tmpstr, sourceText.Get() );

    sourceText.Free();
  #endif
    parseCommaDelimitString( tmpstr, p, FALSE );
    free(tmpstr);
    tmpstr = p.Get();

    if ( !tmpstr ) {
        printMsg(__T("error finding email addresses in %s, aborting\n"), namesfilename);
        return(5);
    }

    // Count and consolidate addresses.
    found = 0;
    for ( ; *tmpstr; tmpstr += _tcslen(tmpstr) + 1 ) {
        if ( found )
            listofnames.Add( __T(',') );

        listofnames.Add( tmpstr );
        found++;
    }

    printMsg(__T("Read %d %s address%s from %s\n"), found, type, (found == 1) ? __T("") : __T("es"), namesfilename);

    p.Free();
    return(0);                                   // indicates no error.
}
#endif


/*
 * In the following check* functions, their arguments have these meanings:
 *
 * argc      is the number of arguments that may be associated with the option.
 * argv      is the pointer to a list of pointers to arguments.
 * this_arg  is the argument index in argv[]
 * startargv is a zero or one, to indicate the argument comes from an options file or not.
 *
 */

typedef enum {
    INSTALL_STATE_SERVER = 0,
    INSTALL_STATE_SENDER,
    INSTALL_STATE_TRY,
    INSTALL_STATE_PORT,
    INSTALL_STATE_PROFILE,
    INSTALL_STATE_LOGIN_ID,
    INSTALL_STATE_PASSWORD,
    INSTALL_STATE_DONE
} _INSTALL_STATES;


static int checkInstallOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int argState;
    int useHKCU;
    int hostFound, senderFound, tryFound, portFound;
    int profileFound, loginFound, pwdFound;

    if ( !startargv )   // -installSMTP is not allowed in an option file.
        return (0);

    startargv    = 0;
    argState     = INSTALL_STATE_SERVER;
    useHKCU      = FALSE;
    hostFound    = FALSE;
    senderFound  = FALSE;
    tryFound     = FALSE;
    portFound    = FALSE;
    profileFound = FALSE;
    loginFound   = FALSE;
    pwdFound     = FALSE;

    SMTPHost[0]  =
    Profile[0]   = __T('\0');

    Sender.Clear();

    AUTHLogin.Clear();
    AUTHPassword.Clear();

    _tcscpy(SMTPPort, defaultSMTPPort);
    _tcscpy( Try, __T("1") );

    if ( !argc && !_tcsicmp( argv[this_arg], strSaveSettings ) )
        argc = (INSTALL_STATE_DONE * 2) + 1;

    for ( ; argc && (argState < INSTALL_STATE_DONE) && argv[this_arg+1]; ) {
        argc--;
        this_arg++;
        startargv++;

        if ( !_tcsicmp(argv[this_arg], __T("-hkcu")) ) {
            useHKCU = TRUE;
            continue;
        }

        if ( !_tcsicmp(argv[this_arg], strServer    )
#if BLAT_LITE
#else
          || !_tcsicmp(argv[this_arg], strServerSMTP)
#endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                _tcsncpy( SMTPHost, argv[this_arg], SERVER_SIZE );
                SMTPHost[SERVER_SIZE] = __T('\0');
                hostFound = TRUE;
                argState = INSTALL_STATE_SERVER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strF       )
          || !_tcsicmp(argv[this_arg], strI       )
          || !_tcsicmp(argv[this_arg], strSender  )
          || !_tcsicmp(argv[this_arg], strMailFrom)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                Sender = argv[this_arg];
                senderFound = TRUE;
                argState = INSTALL_STATE_SENDER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strTry) ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                _tcsncpy( Try, argv[this_arg], TRY_SIZE );
                Try[TRY_SIZE] = __T('\0');
                tryFound = TRUE;
                argState = INSTALL_STATE_TRY+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strPort    )
#if BLAT_LITE
#else
          || !_tcsicmp(argv[this_arg], strPortSMTP)
#endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                _tcsncpy( SMTPPort, argv[this_arg], SERVER_SIZE );
                SMTPPort[SERVER_SIZE] = __T('\0');
                portFound = TRUE;
                argState = INSTALL_STATE_PORT+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strP      )
          || !_tcsicmp(argv[this_arg], strProfile)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the profile.
                    _tcsncpy( Profile, argv[this_arg], TRY_SIZE );// Keep the profile if not "-"
                    Profile[TRY_SIZE] = __T('\0');
                }
                profileFound = TRUE;
                argState = INSTALL_STATE_PROFILE+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strU       )
          || !_tcsicmp(argv[this_arg], strUsername)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the login name.
                    AUTHLogin = argv[this_arg];                   // Keep the login name if not "-"
                }
                loginFound = TRUE;
                argState = INSTALL_STATE_LOGIN_ID+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strPw      )
          || !_tcsicmp(argv[this_arg], strPwd     )
          || !_tcsicmp(argv[this_arg], strPassword)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the password.
                    AUTHPassword = argv[this_arg];                // Keep the password if not "-"
                }
                pwdFound = TRUE;
                argState = INSTALL_STATE_PASSWORD+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        switch( argState ) {
        case INSTALL_STATE_SERVER:
            argState++;
            if ( !hostFound ) {
                hostFound = TRUE;
                _tcsncpy( SMTPHost, argv[this_arg], SERVER_SIZE );
                SMTPHost[SERVER_SIZE] = __T('\0');
                continue;
            }

        case INSTALL_STATE_SENDER:
            argState++;
            if ( !senderFound ) {
                senderFound = TRUE;
                Sender = argv[this_arg];
                continue;
            }

        case INSTALL_STATE_TRY:
            argState++;
            if ( !tryFound ) {
                tryFound = TRUE;
                _tcsncpy( Try, argv[this_arg], TRY_SIZE );
                Try[TRY_SIZE] = __T('\0');
                continue;
            }

        case INSTALL_STATE_PORT:
            argState++;
            if ( !portFound ) {
                portFound = TRUE;
                _tcsncpy( SMTPPort, argv[this_arg], SERVER_SIZE );
                SMTPPort[SERVER_SIZE] = __T('\0');
                continue;
            }

        case INSTALL_STATE_PROFILE:
            argState++;
            if ( !profileFound ) {
                profileFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the profile.
                    _tcsncpy( Profile, argv[this_arg], TRY_SIZE );// Keep the profile if not "-"
                    Profile[TRY_SIZE] = __T('\0');
                }
                continue;
            }

        case INSTALL_STATE_LOGIN_ID:
            argState++;
            if ( !loginFound ) {
                loginFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the login name.
                    AUTHLogin = argv[this_arg];                   // Keep the login name if not "-"
                }
                continue;
            }

        case INSTALL_STATE_PASSWORD:
            argState++;
            if ( !pwdFound ) {
                pwdFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then delete the password.
                    AUTHPassword = argv[this_arg];                // Keep the password if not "-"
                }
                continue;
            }
        }
    }

    if ( !argState ) {
        if ( hostFound && senderFound )
            argState = INSTALL_STATE_DONE;
    }

    if ( argc ) {
        if ( argv[this_arg+1] && !_tcsicmp(argv[this_arg+1], __T("-hkcu")) ) {
            useHKCU = TRUE;
            argc--;
            this_arg++;
            startargv++;
        }
    }

    if ( argState ) {
        if ( !_tcscmp(Try,__T("-")) ) _tcscpy(Try,__T("1"));
        if ( !_tcscmp(Try,__T("0")) ) _tcscpy(Try,__T("1"));

        if ( !_tcscmp(SMTPPort,__T("-")) ) _tcscpy(SMTPPort,defaultSMTPPort);
        if ( !_tcscmp(SMTPPort,__T("0")) ) _tcscpy(SMTPPort,defaultSMTPPort);

#if INCLUDE_NNTP
        NNTPHost[0] = __T('\0');
#endif
#if INCLUDE_POP3
        POP3Host[0] = __T('\0');
#endif
#if INCLUDE_IMAP
        IMAPHost[0] = __T('\0');
#endif
        if ( CreateRegEntry( useHKCU ) == 0 ) {
            printMsg(__T("SMTP server set to %s on port %s with user %s, retry %s time(s)\n"), SMTPHost, SMTPPort, Sender.Get(), Try );
            regerr = 0;
            printMsg( NULL );
            if ( logOut )
                fclose( logOut );

            exitRequired = TRUE;
            return(0);
        }
    } else {
        printMsg( __T("To set the SMTP server's name/address and your username/email address for that\n") \
                  __T("server machine do:\n") \
                  __T("blat -install  server_name  your_email_address\n") \
                  __T("or use '-server <server_name>' and '-f <your_email_address>'\n") );
        return(-6);
    }

    return(startargv);
}

#if INCLUDE_NNTP

static int checkNNTPInstall ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int argState;
    int useHKCU;
    int hostFound, senderFound, tryFound, portFound;
    int profileFound, loginFound, pwdFound;

    if ( !startargv )   // -installNNTP is not allowed in an option file.
        return (0);

    startargv    = 0;
    argState     = INSTALL_STATE_SERVER;
    useHKCU      = FALSE;
    hostFound    = FALSE;
    senderFound  = FALSE;
    tryFound     = FALSE;
    portFound    = FALSE;
    profileFound = FALSE;
    loginFound   = FALSE;
    pwdFound     = FALSE;

    NNTPHost[0]  =
    Profile[0]   = __T('\0');

    Sender.Clear();

    AUTHLogin.Clear();
    AUTHPassword.Clear();

    _tcscpy(NNTPPort, defaultNNTPPort);
    _tcscpy( Try, __T("1") );

    for ( ; argc && argState < INSTALL_STATE_DONE; ) {
        argc--;
        this_arg++;
        startargv++;

        if ( !_tcsicmp(argv[this_arg], __T("-hkcu")) ) {
            useHKCU = TRUE;
            continue;
        }

        if ( !_tcsicmp(argv[this_arg], strServer    )
          || !_tcsicmp(argv[this_arg], strServerNNTP)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                _tcsncpy( NNTPHost, argv[this_arg], SERVER_SIZE );
                NNTPHost[SERVER_SIZE] = __T('\0');
                hostFound = TRUE;
                argState = INSTALL_STATE_SERVER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strF       )
          || !_tcsicmp(argv[this_arg], strI       )
          || !_tcsicmp(argv[this_arg], strSender  )
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                Sender = argv[this_arg];
                senderFound = TRUE;
                argState = INSTALL_STATE_SENDER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strTry) ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                _tcsncpy( Try, argv[this_arg], TRY_SIZE );
                Try[TRY_SIZE] = __T('\0');
                tryFound = TRUE;
                argState = INSTALL_STATE_TRY+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strPort    )
#if BLAT_LITE
#else
          || !_tcsicmp(argv[this_arg], strPortNNTP)
#endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                _tcsncpy( NNTPPort, argv[this_arg], SERVER_SIZE );
                NNTPPort[SERVER_SIZE] = __T('\0');
                portFound = TRUE;
                argState = INSTALL_STATE_PORT+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strP      )
          || !_tcsicmp(argv[this_arg], strProfile)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the profile.
                    _tcsncpy( Profile, argv[this_arg], TRY_SIZE );// Keep the profile if not "-"
                    Profile[TRY_SIZE] = __T('\0');
                }
                profileFound = TRUE;
                argState = INSTALL_STATE_PROFILE+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strU       )
          || !_tcsicmp(argv[this_arg], strUsername)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the login name.
                    AUTHLogin = argv[this_arg];                   // Keep the login name if not "-"
                }
                loginFound = TRUE;
                argState = INSTALL_STATE_LOGIN_ID+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strPw      )
          || !_tcsicmp(argv[this_arg], strPwd     )
          || !_tcsicmp(argv[this_arg], strPassword)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the password.
                    AUTHPassword = argv[this_arg];                // Keep the password if not "-"
                }
                pwdFound = TRUE;
                argState = INSTALL_STATE_PASSWORD+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        switch( argState ) {
        case INSTALL_STATE_SERVER:
            argState++;
            if ( !hostFound ) {
                hostFound = TRUE;
                _tcsncpy( NNTPHost, argv[this_arg], SERVER_SIZE );
                NNTPHost[SERVER_SIZE] = __T('\0');
                continue;
            }

        case INSTALL_STATE_SENDER:
            argState++;
            if ( !senderFound ) {
                senderFound = TRUE;
                Sender = argv[this_arg];
                continue;
            }

        case INSTALL_STATE_TRY:
            argState++;
            if ( !tryFound ) {
                tryFound = TRUE;
                _tcsncpy( Try, argv[this_arg], TRY_SIZE );
                Try[TRY_SIZE] = __T('\0');
                continue;
            }

        case INSTALL_STATE_PORT:
            argState++;
            if ( !portFound ) {
                portFound = TRUE;
                _tcsncpy( NNTPPort, argv[this_arg], SERVER_SIZE );
                NNTPPort[SERVER_SIZE] = __T('\0');
                continue;
            }

        case INSTALL_STATE_PROFILE:
            argState++;
            if ( !profileFound ) {
                profileFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the profile.
                    _tcsncpy( Profile, argv[this_arg], TRY_SIZE );// Keep the profile if not "-"
                    Profile[TRY_SIZE] = __T('\0');
                }
                continue;
            }

        case INSTALL_STATE_LOGIN_ID:
            argState++;
            if ( !loginFound ) {
                loginFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the login name.
                    AUTHLogin = argv[this_arg];                   // Keep the login name if not "-"
                }
                continue;
            }

        case INSTALL_STATE_PASSWORD:
            argState++;
            if ( !pwdFound ) {
                pwdFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then delete the password.
                    AUTHPassword = argv[this_arg];                // Keep the password if not "-"
                }
                continue;
            }
        }
    }

    if ( !argState ) {
        if ( hostFound && senderFound )
            argState = INSTALL_STATE_DONE;
    }

    if ( argc ) {
        if ( !_tcsicmp(argv[++this_arg], __T("-hkcu")) ) {
            useHKCU = TRUE;
            startargv++;
        }
    }

    if ( argState ) {
        if ( !_tcscmp(Try,__T("-")) ) _tcscpy(Try,__T("1"));
        if ( !_tcscmp(Try,__T("0")) ) _tcscpy(Try,__T("1"));

        if ( !_tcscmp(NNTPPort,__T("-")) ) _tcscpy(NNTPPort,defaultNNTPPort);
        if ( !_tcscmp(NNTPPort,__T("0")) ) _tcscpy(NNTPPort,defaultNNTPPort);

#if INCLUDE_POP3
        POP3Host[0] = __T('\0');
#endif
#if INCLUDE_IMAP
        IMAPHost[0] = __T('\0');
#endif
        SMTPHost[0] = __T('\0');
        if ( CreateRegEntry( useHKCU ) == 0 ) {
            printMsg(__T("NNTP server set to %s on port %s with user %s, retry %s time(s)\n"), NNTPHost, NNTPPort, Sender.Get(), Try );
            regerr = 0;
            printMsg( NULL );
            if ( logOut )
                fclose( logOut );

            exitRequired = TRUE;
            return(0);
        }
    } else {
        printMsg( __T("To set the NNTP server's address and the user name at that address do:\nblat -installNNTP server username\n"));
        return(-6);
    }

    return(startargv);
}

static int checkNNTPSrvr ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( NNTPHost, argv[this_arg+1], SERVER_SIZE );
    NNTPHost[SERVER_SIZE] = __T('\0');

    return(1);
}

static int checkNNTPPort ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( NNTPPort, argv[this_arg+1], SERVER_SIZE );
    NNTPPort[SERVER_SIZE] = __T('\0');

    return(1);
}

static int checkNNTPGroups ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( groups.Length() ) {
        printMsg(__T("Only one -groups can be used at a time.  Aborting.\n"));
        return(-1);
    }
    groups = argv[this_arg+1];

    return(1);
}
#endif

#if INCLUDE_POP3

static int checkXtndXmit ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    xtnd_xmit_wanted = TRUE;

    return(0);
}

static int checkPOP3Install ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int argState;
    int useHKCU;
    int hostFound, senderFound, tryFound, portFound;
    int profileFound, loginFound, pwdFound;

    if ( !startargv )   // -installPOP3 is not allowed in an option file.
        return (0);

    startargv    = 0;
    argState     = INSTALL_STATE_SERVER;
    useHKCU      = FALSE;
    hostFound    = FALSE;
    senderFound  = FALSE;
    tryFound     = FALSE;
    portFound    = FALSE;
    profileFound = FALSE;
    loginFound   = FALSE;
    pwdFound     = FALSE;

    POP3Host[0]  =
    Profile[0]   = __T('\0');

    Sender.Clear();

    POP3Login.Clear();
    POP3Password.Clear();

    _tcscpy(POP3Port, defaultPOP3Port);

    for ( ; argc && argState < INSTALL_STATE_DONE; ) {
        argc--;
        this_arg++;
        startargv++;

        if ( !_tcsicmp(argv[this_arg], __T("-hkcu")) ) {
            useHKCU = TRUE;
            continue;
        }

        if ( !_tcsicmp(argv[this_arg], strServer    )
          || !_tcsicmp(argv[this_arg], strServerPOP3)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                _tcsncpy( POP3Host, argv[this_arg], SERVER_SIZE );
                POP3Host[SERVER_SIZE] = __T('\0');
                hostFound = TRUE;
                argState = INSTALL_STATE_SERVER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strF       )
          || !_tcsicmp(argv[this_arg], strI       )
          || !_tcsicmp(argv[this_arg], strSender  )
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                Sender = argv[this_arg];
                senderFound = TRUE;
                argState = INSTALL_STATE_SENDER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strTry) ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                _tcsncpy( Try, argv[this_arg], TRY_SIZE );
                Try[TRY_SIZE] = __T('\0');
                tryFound = TRUE;
                argState = INSTALL_STATE_TRY+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strPort    )
  #if BLAT_LITE
  #else
          || !_tcsicmp(argv[this_arg], strPortPOP3)
  #endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                _tcsncpy( POP3Port, argv[this_arg], SERVER_SIZE );
                POP3Port[SERVER_SIZE] = __T('\0');
                portFound = TRUE;
                argState = INSTALL_STATE_PORT+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strP      )
          || !_tcsicmp(argv[this_arg], strProfile)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the profile.
                    _tcsncpy( Profile, argv[this_arg], TRY_SIZE );// Keep the profile if not "-"
                    Profile[TRY_SIZE] = __T('\0');
                }
                profileFound = TRUE;
                argState = INSTALL_STATE_PROFILE+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strU           )
          || !_tcsicmp(argv[this_arg], strUsername    )
  #if BLAT_LITE
  #else
          || !_tcsicmp(argv[this_arg], strPop3U       )
          || !_tcsicmp(argv[this_arg], strPop3Username)
  #endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the login name.
                    POP3Login = argv[this_arg];                   // Keep the login name if not "-"
                 }
                loginFound = TRUE;
                argState = INSTALL_STATE_LOGIN_ID+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strPw          )
          || !_tcsicmp(argv[this_arg], strPwd     )
          || !_tcsicmp(argv[this_arg], strPassword    )
  #if BLAT_LITE
  #else
          || !_tcsicmp(argv[this_arg], strPop3Pw      )
          || !_tcsicmp(argv[this_arg], strPop3Password)
  #endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the password.
                    POP3Password = argv[this_arg];                // Keep the password if not "-"
                }
                pwdFound = TRUE;
                argState = INSTALL_STATE_PASSWORD+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        switch( argState ) {
        case INSTALL_STATE_SERVER:
            argState++;
            if ( !hostFound ) {
                hostFound = TRUE;
                _tcsncpy( POP3Host, argv[this_arg], SERVER_SIZE );
                POP3Host[SERVER_SIZE] = __T('\0');
                continue;
            }

        case INSTALL_STATE_SENDER:
            argState++;
            senderFound = TRUE;
            continue;

        case INSTALL_STATE_TRY:
            argState++;
            tryFound = TRUE;
            continue;

        case INSTALL_STATE_PORT:
            argState++;
            if ( !portFound ) {
                portFound = TRUE;
                _tcsncpy( POP3Port, argv[this_arg], SERVER_SIZE );
                POP3Port[SERVER_SIZE] = __T('\0');
                continue;
            }

        case INSTALL_STATE_PROFILE:
            argState++;
            if ( !profileFound ) {
                profileFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the profile.
                    _tcsncpy( Profile, argv[this_arg], TRY_SIZE );// Keep the profile if not "-"
                    Profile[TRY_SIZE] = __T('\0');
                }
                continue;
            }

        case INSTALL_STATE_LOGIN_ID:
            argState++;
            if ( !loginFound ) {
                loginFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the login name.
                    POP3Login = argv[this_arg];                   // Keep the login name if not "-"
                }
                continue;
            }

        case INSTALL_STATE_PASSWORD:
            argState++;
            if ( !pwdFound ) {
                pwdFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then delete the password.
                    POP3Password = argv[this_arg];                // Keep the password if not "-"
                }
                continue;
            }
        }
    }

    if ( !argState ) {
        if ( hostFound && senderFound )
            argState = INSTALL_STATE_DONE;
    }

    if ( argc ) {
        if ( !_tcsicmp(argv[++this_arg], __T("-hkcu")) ) {
            useHKCU = TRUE;
            startargv++;
        }
    }

    if ( argState ) {
        if ( !_tcscmp(POP3Port,__T("-")) ) _tcscpy(POP3Port,defaultPOP3Port);
        if ( !_tcscmp(POP3Port,__T("0")) ) _tcscpy(POP3Port,defaultPOP3Port);

  #if INCLUDE_NNTP
        NNTPHost[0] = __T('\0');
  #endif
        SMTPHost[0] = __T('\0');
        if ( CreateRegEntry( useHKCU ) == 0 ) {
            printMsg(__T("POP3 server set to %s on port %s\n"), POP3Host, POP3Port );
            regerr = 0;
            printMsg( NULL );
            if ( logOut )
                fclose( logOut );

            exitRequired = TRUE;
            return(0);
        }
    } else {
        printMsg( __T("To set the POP3 server's address and the login name at that address do:\nblat -installPOP3 server - - - - loginname loginpwd\n"));
        return(-6);
    }

    return(startargv);
}

static int checkPOP3Srvr ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( POP3Host, argv[this_arg+1], SERVER_SIZE );
    POP3Host[SERVER_SIZE] = __T('\0');

    return(1);
}

static int checkPOP3Port ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( POP3Port, argv[this_arg+1], SERVER_SIZE );
    POP3Port[SERVER_SIZE] = __T('\0');

    return(1);
}
#endif

#if INCLUDE_IMAP

static int checkIMAPInstall ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int argState;
    int useHKCU;
    int hostFound, senderFound, tryFound, portFound;
    int profileFound, loginFound, pwdFound;

    if ( !startargv )   // -installIMAP is not allowed in an option file.
        return (0);

    startargv    = 0;
    argState     = INSTALL_STATE_SERVER;
    useHKCU      = FALSE;
    hostFound    = FALSE;
    senderFound  = FALSE;
    tryFound     = FALSE;
    portFound    = FALSE;
    profileFound = FALSE;
    loginFound   = FALSE;
    pwdFound     = FALSE;

    IMAPHost[0]  =
    Profile[0]   = __T('\0');

    Sender.Clear();

    IMAPLogin.Clear();
    IMAPPassword.Clear();

    _tcscpy(IMAPPort, defaultIMAPPort);

    for ( ; argc && argState < INSTALL_STATE_DONE; ) {
        argc--;
        this_arg++;
        startargv++;

        if ( !_tcsicmp(argv[this_arg], __T("-hkcu")) ) {
            useHKCU = TRUE;
            continue;
        }

        if ( !_tcsicmp(argv[this_arg], strServer    )
          || !_tcsicmp(argv[this_arg], strServerIMAP)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                _tcsncpy( IMAPHost, argv[this_arg], SERVER_SIZE );
                IMAPHost[SERVER_SIZE] = __T('\0');
                hostFound = TRUE;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strF       )
          || !_tcsicmp(argv[this_arg], strI       )
          || !_tcsicmp(argv[this_arg], strSender  )
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                Sender = argv[this_arg];
                senderFound = TRUE;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strTry) ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                _tcsncpy( Try, argv[this_arg], TRY_SIZE );
                Try[TRY_SIZE] = __T('\0');
                tryFound = TRUE;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strPort    )
  #if BLAT_LITE
  #else
          || !_tcsicmp(argv[this_arg], strPortIMAP)
  #endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                _tcsncpy( IMAPPort, argv[this_arg], SERVER_SIZE );
                IMAPPort[SERVER_SIZE] = __T('\0');
                portFound = TRUE;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strP      )
          || !_tcsicmp(argv[this_arg], strProfile)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the profile.
                    _tcsncpy( Profile, argv[this_arg], TRY_SIZE );// Keep the profile if not "-"
                    Profile[TRY_SIZE] = __T('\0');
                }
                profileFound = TRUE;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strU           )
          || !_tcsicmp(argv[this_arg], strUsername    )
  #if BLAT_LITE
  #else
          || !_tcsicmp(argv[this_arg], strImapU       )
          || !_tcsicmp(argv[this_arg], strImapUsername)
  #endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the login name.
                    IMAPLogin = argv[this_arg];                   // Keep the login name if not "-"
                }
                loginFound = TRUE;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strPw          )
          || !_tcsicmp(argv[this_arg], strPwd         )
          || !_tcsicmp(argv[this_arg], strPassword    )
  #if BLAT_LITE
  #else
          || !_tcsicmp(argv[this_arg], strImapPw      )
          || !_tcsicmp(argv[this_arg], strImapPassword)
  #endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the password.
                    IMAPPassword = argv[this_arg];                // Keep the password if not "-"
                }
                pwdFound = TRUE;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        switch( argState ) {
        case INSTALL_STATE_SERVER:
            argState++;
            if ( !hostFound ) {
                hostFound = TRUE;
                _tcsncpy( IMAPHost, argv[this_arg], SERVER_SIZE );
                IMAPHost[SERVER_SIZE] = __T('\0');
                continue;
            }

        case INSTALL_STATE_SENDER:
            argState++;
            senderFound = TRUE;
            continue;

        case INSTALL_STATE_TRY:
            argState++;
            tryFound = TRUE;
            continue;

        case INSTALL_STATE_PORT:
            argState++;
            if ( !portFound ) {
                portFound = TRUE;
                _tcsncpy( IMAPPort, argv[this_arg], SERVER_SIZE );
                IMAPPort[SERVER_SIZE] = __T('\0');
                continue;
            }

        case INSTALL_STATE_PROFILE:
            argState++;
            if ( !profileFound ) {
                profileFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the profile.
                    _tcsncpy( Profile, argv[this_arg], TRY_SIZE );// Keep the profile if not "-"
                    Profile[TRY_SIZE] = __T('\0');
                }
                continue;
            }

        case INSTALL_STATE_LOGIN_ID:
            argState++;
            if ( !loginFound ) {
                loginFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the login name.
                    IMAPLogin = argv[this_arg];                   // Keep the login name if not "-"
                }
                continue;
            }

        case INSTALL_STATE_PASSWORD:
            argState++;
            if ( !pwdFound ) {
                pwdFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then delete the password.
                    IMAPPassword = argv[this_arg];                // Keep the password if not "-"
                }
                continue;
            }
        }
    }

    if ( !argState ) {
        if ( hostFound && senderFound )
            argState = INSTALL_STATE_DONE;
    }

    if ( argc ) {
        if ( !_tcsicmp(argv[++this_arg], __T("-hkcu")) ) {
            useHKCU = TRUE;
            startargv++;
        }
    }

    if ( argState ) {
        if ( !_tcscmp(IMAPPort,__T("-")) ) _tcscpy(IMAPPort,defaultIMAPPort);
        if ( !_tcscmp(IMAPPort,__T("0")) ) _tcscpy(IMAPPort,defaultIMAPPort);

  #if INCLUDE_NNTP
        NNTPHost[0] = __T('\0');
  #endif
        SMTPHost[0] = __T('\0');
        if ( CreateRegEntry( useHKCU ) == 0 ) {
            printMsg(__T("IMAP server set to %s on port %s\n"), IMAPHost, IMAPPort );
            regerr = 0;
            printMsg( NULL );
            if ( logOut )
                fclose( logOut );

            exitRequired = TRUE;
            return(0);
        }
    } else {
        printMsg( __T("To set the IMAP server's address and the login name at that address do:\nblat -installIMAP server - - - - loginname loginpwd\n"));
        return(-6);
    }

    return(startargv);
}

static int checkIMAPSrvr ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( IMAPHost, argv[this_arg+1], SERVER_SIZE );
    IMAPHost[SERVER_SIZE] = __T('\0');

    return(1);
}

static int checkIMAPPort ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( IMAPPort, argv[this_arg+1], SERVER_SIZE );
    IMAPPort[SERVER_SIZE] = __T('\0');

    return(1);
}
#endif

#if BLAT_LITE
#else

static int checkOptionFile ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( !optionsFile[0] ) {
        _tcsncpy( optionsFile, argv[this_arg+1], _MAX_PATH - 1);
        optionsFile[_MAX_PATH-1] = __T('\0');
    }
    return(1);
}
#endif

static int checkToOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( destination.Length() ) {
        printMsg(__T("Warning: -t/-to is being used with -tf, or another -t/to.\n\t Previous values will be ignored.\n"));
        destination.Free();
    }

    destination = argv[this_arg+1];

    return(1);
}

#if BLAT_LITE
#else

static int checkToFileOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int ret;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( destination.Length() ) {
        printMsg(__T("Warning: -tf is being used with  -t/-to, or another -tf.\n\t Previous values will be ignored.\n"));
        destination.Free();
    }

    ret = ReadNamesFromFile(__T("TO"), argv[this_arg+1], destination);
    if ( ret )
        return(0 - ret);

    return(1);
}
#endif

static int checkCcOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( cc_list.Length() ) {
        printMsg(__T("Warning: -c/-cc is being used with  -cf, or another -c/-cc.\n\t Previous values will be ignored.\n"));
        cc_list.Free();
    }
    cc_list = argv[this_arg+1];

    return(1);
}

#if BLAT_LITE
#else

static int checkCcFileOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int ret;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( cc_list.Length() ) {
        printMsg(__T("Warning: -cf is being used with  -c/-cc, or another -cf.\n\t Previous values will be ignored.\n"));
        cc_list.Free();
    }

    ret = ReadNamesFromFile(__T("CC"), argv[this_arg+1], cc_list);
    if ( ret )
        return(0 - ret);

    return(1);
}
#endif

static int checkBccOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( bcc_list.Length() ) {
        printMsg(__T("Warning: -b/-bcc is being used with -bf, or another -b/-bcc.\n\t Previous values will be ignored.\n"));
        bcc_list.Free();
    }
    bcc_list = argv[this_arg+1];

    return(1);
}

#if BLAT_LITE
#else

static int checkBccFileOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int ret;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( bcc_list.Length() ) {
        printMsg(__T("Warning: -bf is being used with  -b/-bcc, or another -bf.\n\t Previous values will be ignored.\n"));
        bcc_list.Free();
    }

    ret = ReadNamesFromFile(__T("BCC"), argv[this_arg+1], bcc_list);
    if ( ret )
        return(0 - ret);

    return(1);
}
#endif

static int checkSubjectOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    subject.Free();
    if ( argv[this_arg+1][0] != __T('\0') ) {
  #if defined(_UNICODE) || defined(UNICODE)
        int    utf;

        subject.Add( (_TCHAR)0xFEFF );          /* Prepend a UTF-16 BOM */
        subject.Add( argv[this_arg+1] );
        utf = 0;
        convertUnicode( subject, &utf, NULL, 8 );
  #else
        subject.Add( argv[this_arg+1] );
  #endif
    }
    return(1);
}

#if BLAT_LITE
#else

static int checkSubjectFile ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    FILE * infile;
    int    x;
    LPTSTR pString;
    _TCHAR tmpSubject[SUBJECT_SIZE+1];

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    subject.Free();
    infile = _tfopen(argv[this_arg+1], __T("r"));
    if ( infile ) {
        memset(tmpSubject, 0x00, SUBJECT_SIZE*sizeof(_TCHAR));
        _fgetts(tmpSubject, SUBJECT_SIZE, infile);    //lint !e534 ignore return
        tmpSubject[SUBJECT_SIZE] = __T('\0');
        fclose(infile);
        subject.Add( tmpSubject );

  #if defined(_UNICODE) || defined(UNICODE)
        Buf    sourceText;
        int    utf;

        utf = 0;
        sourceText.Add( subject.Get(), SUBJECT_SIZE );
        convertUnicode( sourceText, &utf, NULL, 8 );
        if ( utf )
            subject = sourceText;

        sourceText.Free();
  #endif
        pString = subject.Get();
        for ( x = 0; pString[x]; x++ ) {
            if ( (pString[x] == __T('\n')) || (pString[x] == __T('\t')) )
                pString[x] = __T(' ');  // convert LF and tabs to spaces
            else
            if ( pString[x] == __T('\r') ) {
                _tcscpy( &pString[x], &pString[x+1] );
                x--;            // Remove CR bytes.
            }
        }
        for ( ; x; ) {
            if ( pString[--x] != __T(' ') )
                break;

            _tcscpy( &pString[x], &pString[x+1] );  // Strip off trailing spaces.
        }
        subject.SetLength();
    } else {
        subject.Add( argv[this_arg+1] );
    }

    return(1);
}
#endif

static int checkSkipSubject ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    ssubject = TRUE;                /*$$ASD*/

    return(0);
}

static int checkBodyText ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcscpy(bodyFilename, __T("-"));
    bodyparameter = argv[this_arg+1];

#if defined(_UNICODE) || defined(UNICODE)
    size_t    length;
    size_t    x;
    _TUCHAR * pStr;

    length = bodyparameter.Length();
    pStr = (_TUCHAR *)bodyparameter.Get();
    for ( x = 0; x < length; x++ ) {
        if ( pStr[x] > 0x00FF ) {
            Buf newbody;

            newbody.Add( (_TCHAR)0xFEFF );          /* Prepend a UTF-16 BOM */
            newbody.Add( (LPTSTR)pStr, length );    /* Now add the user's Unicode message */
            bodyparameter = newbody;
            newbody.Free();

            utf = UTF_REQUESTED;
#if BLAT_LITE
            mime = TRUE;
#else
            eightBitMimeRequested = TRUE;
#endif
            break;
        }
    }
#endif

    return(1);
}

static int checkBodyFile ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( !argv[this_arg+1] )
        return(-1);

    _tcsncpy(bodyFilename, argv[this_arg+1], _MAX_PATH-1 );
    bodyFilename[_MAX_PATH-1] = __T('\0');

    return(1);
}

static int checkProfileEdit ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int useHKCU;

    if ( !startargv )   // -profile is not allowed in an option file.
        return(0);

    useHKCU = FALSE;
    this_arg++;

#if BLAT_LITE
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;

    ShowRegHelp();
    ListProfiles(__T("<all>"));
#else
/*    if ( argc == 0 ) {
        ListProfiles(__T("<all>"));
    } else
 */
    if ( !argv[this_arg] ) {    // argc = 0
        ListProfiles(__T("<all>"));
    } else {
        if ( !_tcscmp(argv[this_arg], __T("-delete")) ) {
            this_arg++;
            if ( argc )
                argc--;

            for ( ; argc; argc-- ) {
                if ( !_tcscmp( argv[this_arg], __T("-hkcu") ) ) {
                    useHKCU = TRUE;
                    this_arg++;
                    continue;
                }
                DeleteRegEntry( argv[this_arg], useHKCU );
                this_arg++;
            }
        } else {
            if ( !_tcscmp(argv[this_arg], __T("-h")) ) {
                if ( argc )
                    argc--;

                this_arg++;
                ShowRegHelp();
            }
            if ( argc == 0 ) {
                ListProfiles(__T("<all>"));
            } else {
                for ( ; argc; argc-- ) {
                    if ( !_tcscmp( argv[this_arg], __T("-hkcu") ) ) {
                        this_arg++; // ignore this option if found
                        continue;
                    }

                    ListProfiles(argv[this_arg]);
                    this_arg++;
                }
            }
        }
    }
#endif
    printMsg( NULL );
    if ( logOut )
        fclose( logOut );

    exitRequired = TRUE;
    return(0);
}

#if BLAT_LITE
#else

static int checkProfileToUse ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( Profile, argv[this_arg+1], TRY_SIZE );
    Profile[TRY_SIZE] = __T('\0');

    return(1);
}
#endif

static int checkServerOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( SMTPHost, argv[this_arg+1], SERVER_SIZE );
    SMTPHost[SERVER_SIZE] = __T('\0');

    return(1);
}

static int checkFromOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( loginname, argv[this_arg+1], SENDER_SIZE );
    loginname[SENDER_SIZE] = __T('\0');

    return(1);
}

static int checkImpersonate ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( senderid, argv[this_arg+1], SENDER_SIZE );
    senderid[SENDER_SIZE] = __T('\0');
    impersonating = TRUE;

    return(1);
}

static int checkPortOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( SMTPPort, argv[this_arg+1], SERVER_SIZE );
    SMTPPort[SERVER_SIZE] = __T('\0');

    return(1);
}

static int checkUserIDOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    AUTHLogin = argv[this_arg+1];

    return(1);
}

static int checkPwdOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    AUTHPassword = argv[this_arg+1];

    return(1);
}

#if INCLUDE_POP3
static int checkPop3UIDOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    POP3Login = argv[this_arg+1];

    return(1);
}

static int checkPop3PwdOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    POP3Password = argv[this_arg+1];

    return(1);
}
#endif

#if INCLUDE_IMAP
static int checkImapUIDOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    IMAPLogin = argv[this_arg+1];

    return(1);
}

static int checkImapPwdOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    IMAPPassword = argv[this_arg+1];

    return(1);
}
#endif

#if SUPPORT_GSSAPI
static int checkGssapiMutual (int argc, LPTSTR *argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

#if SUBMISSION_PORT
    _tcsncpy(SMTPPort, SubmissionPort, SERVER_SIZE);
    SMTPPort[SERVER_SIZE] = __T('\0');
#endif
    authgssapi = TRUE;
    mutualauth = TRUE;
    return(0);
}

static int checkGssapiClient (int argc, LPTSTR *argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

#if SUBMISSION_PORT
    _tcsncpy(SMTPPort, SubmissionPort, SERVER_SIZE );
    SMTPPort[SERVER_SIZE] = __T('\0');
#endif
    authgssapi = TRUE;
    mutualauth = FALSE;
    return(0);
}

static int checkServiceName (int argc, LPTSTR *argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( servicename, argv[this_arg+1], SERVICENAME_SIZE-1 );
    servicename[SERVICENAME_SIZE-1] = __T('\0');

    return(1);
}

static int checkProtectionLevel (int argc, LPTSTR *argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    switch (argv[this_arg+1][0]) {
    case __T('N'):
    case __T('n'):
    case __T('0'):
        gss_protection_level = GSSAUTH_P_NONE;
        break;
    case __T('I'):
    case __T('i'):
    case __T('1'):
        gss_protection_level = GSSAUTH_P_INTEGRITY;
        break;
    default:
        gss_protection_level = GSSAUTH_P_PRIVACY;
    }
    return(1);
}
#endif

#if BLAT_LITE
#else

static int checkBypassCramMD5 (int argc, LPTSTR *argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    ByPassCRAM_MD5 = TRUE;
    return(0);
}

static int checkOrgOption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( organization, argv[this_arg+1], ORG_SIZE );
    organization[ORG_SIZE] = __T('\0');

    return(1);
}

static int checkXHeaders ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    // is argv[] "-x"? If so, argv[3] is an X-Header
    // Header MUST start with X-
    if ( (argv[this_arg+1][0] == __T('X')) && (argv[this_arg+1][1] == __T('-')) ) {
        if ( xheaders[0] ) {
            _tcscat(xheaders, __T("\r\n"));
        }

        _tcscat( xheaders, argv[this_arg+1] );
    }

    return(1);
}

static int checkDisposition ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    disposition = TRUE;

    return(0);
}

static int checkReadReceipt ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    returnreceipt = TRUE;

    return(0);
}

static int checkNoHeaders ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    noheader = 1;

    return(0);
}

static int checkNoHeaders2 ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    noheader = 2;

    return(0);
}
#endif

static int checkPriority ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    // Toby Korn 8/4/1999
    // Check for priority. 0 is Low, 1 is High - you don't need
    // normal, since omission of a priority is normal.

    priority[0] = argv[this_arg+1][0];
    priority[1] = __T('\0');

    return(1);
}

static int checkSensitivity ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    // Check for sensitivity. 0 is personal, 1 is private, 2 is company-confidential
    // normal, since omission of a sensitivity is normal.

    sensitivity[0] = argv[this_arg+1][0];
    sensitivity[1] = __T('\0');

    return(1);
}

static int checkCharset ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( charset, argv[this_arg+1], 40-1 );
    charset[40-1] = __T('\0');

    return(1);
}

#if BLAT_LITE
#else

static int checkDeliveryStat ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcslwr( argv[this_arg+1] );
    if ( _tcschr(argv[this_arg+1], __T('n')) )
        deliveryStatusRequested = DSN_NEVER;
    else {
        if ( _tcschr(argv[this_arg+1], __T('s')) )
            deliveryStatusRequested |= DSN_SUCCESS;

        if ( _tcschr(argv[this_arg+1], __T('f')) )
            deliveryStatusRequested |= DSN_FAILURE;

        if ( _tcschr(argv[this_arg+1], __T('d')) )
            deliveryStatusRequested |= DSN_DELAYED;
    }

    return(1);
}

static int checkHdrEncB ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    forcedHeaderEncoding = __T('b');

    return(0);
}

static int checkHdrEncQ ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    forcedHeaderEncoding = __T('q');

    return(0);
}
#endif

#if SUPPORT_YENC
static int check_yEnc ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    mime     = FALSE;
    base64   = FALSE;
    uuencode = FALSE;
    yEnc     = TRUE;

    return(0);
}
#endif

static int checkMime ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    mime     = TRUE;
#if BLAT_LITE
#else
    base64   = FALSE;
    uuencode = FALSE;
    yEnc     = FALSE;
#endif

    return(0);
}

#if BLAT_LITE
#else

static int checkUUEncode ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    if ( !haveEmbedded ) {
        mime     = FALSE;
        base64   = FALSE;
        uuencode = TRUE;
        yEnc     = FALSE;
    }

    return(0);
}

static int checkLongUUEncode ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    uuencodeBytesLine = 63;     // must be a multiple of three and less than 64.

    return checkUUEncode( argc, argv, this_arg, startargv );
}

static int checkBase64Enc ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    if ( !attach ) {
        mime     = FALSE;
        base64   = TRUE;
        uuencode = FALSE;
        yEnc     = FALSE;
    }

    return(0);
}

static int checkEnriched ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    _tcscpy( textmode, __T("enriched") );

    return checkMime( argc, argv, this_arg, startargv );
}
#endif

static int checkHTML ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    _tcscpy( textmode, __T("html") );

    return checkMime( argc, argv, this_arg, startargv );
}

#if BLAT_LITE
#else

static int checkUnicode ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    utf = UTF_REQUESTED;
    return 0;
}
#endif

static int addToAttachments ( LPTSTR* argv, int this_arg, _TCHAR aType )
{
    Buf    tmpstr;
    LPTSTR srcptr;
    int    i;

    parseCommaDelimitString( argv[this_arg+1], tmpstr, TRUE );
    srcptr = tmpstr.Get();
    if ( srcptr ) {
        for ( ; *srcptr; ) {

            if ( attach == 64 ) {
                printMsg(__T("Max of 64 files allowed!  Others are being ignored.\n"));
                break;
            }

            for ( i = 0; i < attach; i++ ) {
                if ( aType < attachtype[i] )
                    break;
            }

            if ( i == attach ) {
                attachtype[attach] = aType;
                _tcsncpy( (LPTSTR)attachfile[attach], srcptr, _MAX_PATH-1 );
                attachfile[attach++][_MAX_PATH-1] = __T('\0');
            } else {
                for ( int j = 63; j > i; j-- ) {
                    attachtype[j] = attachtype[j-1];
                    memcpy( attachfile[j], attachfile[j-1], _MAX_PATH );
                }
                attachtype[i] = aType;
                _tcsncpy( (LPTSTR)attachfile[i], srcptr, _MAX_PATH-1 );
                attachfile[i][_MAX_PATH-1] = __T('\0');
                attach++;
            }

            srcptr += _tcslen(srcptr) + 1;
#if BLAT_LITE
#else
            base64 = FALSE;
#endif
        }

        tmpstr.Free();
    }

    return(1);
}

static int checkInlineAttach ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    // -attachI added to v2.4.1
    haveAttachments = TRUE;

    return addToAttachments( argv, this_arg, INLINE_ATTACHMENT );
}

static int checkTxtFileAttach ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    // -attacht added 98.4.16 v1.7.2 tcharron@interlog.com
    haveAttachments = TRUE;

    return addToAttachments( argv, this_arg, TEXT_ATTACHMENT );
}

static int checkBinFileAttach ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    // -attach added 98.2.24 v1.6.3 tcharron@interlog.com
    haveAttachments = TRUE;

    return addToAttachments( argv, this_arg, BINARY_ATTACHMENT );
}

#if BLAT_LITE
#else

static int checkBinFileEmbed ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    haveEmbedded = TRUE;
    checkMime( argc, argv, this_arg, startargv );   // Force MIME encoding.

    return addToAttachments( argv, this_arg, EMBED_ATTACHMENT );
}

static int ReadFilenamesFromFile(LPTSTR namesfilename, _TCHAR aType) {
    WinFile fileh;
    DWORD   filesize;
    DWORD   dummy;
    LPTSTR  tmpstr;

    if ( !fileh.OpenThisFile(namesfilename)) {
        printMsg(__T("error opening %s, aborting\n"), namesfilename);
        return(-3);
    }
    filesize = fileh.GetSize();
    tmpstr = (LPTSTR)malloc( (filesize + 1)*sizeof(_TCHAR) );
    if ( !tmpstr ) {
        fileh.Close();
        printMsg(__T("error allocating memory for reading %s, aborting\n"), namesfilename);
        return(-5);
    }

    if ( !fileh.ReadThisFile(tmpstr, filesize, &dummy, NULL) ) {
        fileh.Close();
        free(tmpstr);
        printMsg(__T("error reading %s, aborting\n"), namesfilename);
        return(-5);
    }
    fileh.Close();

    tmpstr[filesize] = __T('\0');
    addToAttachments( &tmpstr, -1, aType );
    free(tmpstr);
    return(1);                                   // indicates no error.
}

static int checkTxtFileAttFil ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int retVal;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    retVal = ReadFilenamesFromFile(argv[this_arg+1], TEXT_ATTACHMENT );

    if ( retVal == 1 )
        haveAttachments = TRUE;

    return retVal;
}

static int checkBinFileAttFil ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int retVal;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    retVal = ReadFilenamesFromFile(argv[this_arg+1], BINARY_ATTACHMENT );

    if ( retVal == 1 )
        haveAttachments = TRUE;

    return retVal;
}

static int checkEmbFileAttFil ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int retVal;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    checkMime( argc, argv, this_arg, startargv );   // Force MIME encoding.

    retVal = ReadFilenamesFromFile(argv[this_arg+1], EMBED_ATTACHMENT );

    if ( retVal == 1 )
        haveEmbedded = TRUE;

    return retVal;
}
#endif

static int checkHelp ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    printUsage( TRUE );

    return(-1);
}

static int checkQuietMode ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    quiet = TRUE;

    return(0);
}

static int checkDebugMode ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    debug = TRUE;

    return(0);
}

static int checkLogMessages ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( logOut ) {
        fclose( logOut );
        logOut = 0;
    }
    if ( !argv[this_arg+1] || !argv[this_arg+1][0] )
        return(-1);

    _tcsncpy( logFile, argv[this_arg+1], _MAX_PATH - 1 );
    logFile[_MAX_PATH - 1] = __T('\0');

    if ( logFile[0] ) {
        if ( clearLogFirst )
            logOut = _tfopen(logFile, fileCreateAttribute);
        else
            logOut = _tfopen(logFile, fileAppendAttribute);
    }

    // if all goes well the file is closed normally
    // if we return before closing the library SHOULD
    // close it for us..

    return(1);
}

static int checkLogOverwrite ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    clearLogFirst = TRUE;
    if ( logOut )
    {
        fclose( logOut );
        logOut = _tfopen(logFile, fileCreateAttribute);
    }

    return(0);
}

static int checkTimestamp ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    timestamp = TRUE;

    return(0);
}

static int checkTimeout ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    globaltimeout = _tstoi(argv[this_arg+1]);

    return(1);
}

static int checkAttempts ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( Try, argv[this_arg+1], TRY_SIZE );
    Try[TRY_SIZE] = __T('\0');

    return(1);
}

static int checkFixPipe ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    bodyconvert = FALSE;

    return(0);
}

static int checkHostname ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( my_hostname_wanted, argv[this_arg+1], 1024-1 );
    my_hostname_wanted[1024-1] = __T('\0');

    return(1);
}

static int checkMailFrom ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( loginname, argv[this_arg+1], SENDER_SIZE );
    loginname[SENDER_SIZE] = __T('\0');

    return(1);
}

static int checkWhoFrom ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( fromid, argv[this_arg+1], SENDER_SIZE );
    fromid[SENDER_SIZE] = __T('\0');

    return(1);
}

static int checkReplyTo ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( replytoid, argv[this_arg+1], SENDER_SIZE );
    replytoid[SENDER_SIZE] = __T('\0');

    return(1);
}

static int checkReturnPath ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( returnpathid, argv[this_arg+1], SENDER_SIZE );
    returnpathid[SENDER_SIZE] = __T('\0');

    return(1);
}

static int checkSender ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( sendername, argv[this_arg+1], SENDER_SIZE );
    sendername[SENDER_SIZE] = __T('\0');

    return(1);
}

static int checkRaw ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    formattedContent = FALSE;

    return(0);
}

#if BLAT_LITE
#else

static int checkA1Headers ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( aheaders1, argv[this_arg+1], DEST_SIZE );
    aheaders1[DEST_SIZE] = __T('\0');

    return(1);
}

static int checkA2Headers ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _tcsncpy( aheaders2, argv[this_arg+1], DEST_SIZE );
    aheaders2[DEST_SIZE] = __T('\0');

    return(1);
}

static int check8bitMime ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    eightBitMimeRequested = TRUE;

    return checkMime( argc, argv, this_arg, startargv );
}

static int checkAltTextFile ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    Buf     tmpstr;
    LPTSTR  srcptr;
    _TCHAR   alternateTextFile[_MAX_PATH];
    WinFile atf;
    DWORD   fileSize;
    DWORD   dummy;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    parseCommaDelimitString( argv[this_arg+1], tmpstr, TRUE );
    srcptr = tmpstr.Get();
    if ( srcptr ) {
        _tcsncpy( alternateTextFile, srcptr, _MAX_PATH-1 );  // Copy only the first filename.
        alternateTextFile[_MAX_PATH-1] = __T('\0');
    }
    tmpstr.Free();

    if ( !atf.OpenThisFile(alternateTextFile) ) {
        printMsg( __T("Error opening %s, Alternate Text will not be added.\n"), alternateTextFile );
    } else {
        fileSize = atf.GetSize();
        alternateText.Clear();

        if ( fileSize ) {
            alternateText.Alloc( fileSize + 1 );
            if ( !atf.ReadThisFile(alternateText.Get(), fileSize, &dummy, NULL) ) {
                printMsg( __T("Error reading %s, Alternate Text will not be added.\n"), alternateTextFile );
            } else {
                *(alternateText.Get()+fileSize) = __T('\0');
                alternateText.SetLength(fileSize);
            }
        }
        atf.Close();
    }
    return(1);
}

static int checkAltText ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    alternateText = argv[this_arg+1];
    return(1);
}
#endif
#if SUPPORT_SIGNATURES

static int checkSigFile ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    WinFile fileh;
    DWORD   sigsize;
    DWORD   dummy;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    this_arg++;
    if ( !fileh.OpenThisFile(argv[this_arg]) ) {
        printMsg( __T("Error opening signature file %s, signature will not be added\n"), argv[this_arg] );
        return(1);
    }

    sigsize = fileh.GetSize();
    signature.Alloc( sigsize + 1 );
    signature.SetLength( sigsize );

    if ( !fileh.ReadThisFile(signature.Get(), sigsize, &dummy, NULL) ) {
        printMsg( __T("Error reading signature file %s, signature will not be added\n"), argv[this_arg] );
        signature.Free();
    }
    else
        *signature.GetTail() = __T('\0');

    fileh.Close();
    return(1);
}
#endif
#if SUPPORT_TAGLINES

static int checkTagFile ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    WinFile fileh;
    DWORD   tagsize;
    DWORD   count;
    DWORD   selectedTag;
    LPTSTR  p;
    Buf     tmpBuf;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    this_arg++;
    if ( !fileh.OpenThisFile(argv[this_arg]) ) {
        printMsg( __T("Error opening tagline file %s, a tagline will not be added.\n"), argv[this_arg] );
        return(1);
    }

    tagsize = fileh.GetSize();
    if ( tagsize ) {
        tmpBuf.Alloc( tagsize + 1 );
        tmpBuf.SetLength( tagsize );

        if ( !fileh.ReadThisFile(tmpBuf.Get(), tagsize, &count, NULL) ) {
            printMsg( __T("Error reading tagline file %s, a tagline will not be added.\n"), argv[this_arg] );
        }

        *tmpBuf.GetTail() = __T('\0');
        p = tmpBuf.Get();
        for ( count = 0; p; ) {
            p = _tcschr( p, __T('\n') );
            if ( p ) {
                p++;
                count++;
            }
        }

        if ( count ) {
            LPTSTR p2;

            selectedTag = rand() % count;
            p = tmpBuf.Get();

            for ( ; selectedTag; selectedTag-- )
                p = _tcschr( p, __T('\n') ) + 1;

            p2 = _tcschr( p, __T('\n') );
            if ( p2 )
                p2[1] = __T('\0');

            tagline = p;
        } else
            tagline.Add( tmpBuf );
    }
    fileh.Close();

    return(1);
}
#endif
#if SUPPORT_POSTSCRIPTS
static int checkPostscript ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    WinFile fileh;
    DWORD   sigsize;
    DWORD   dummy;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    this_arg++;
    if ( !fileh.OpenThisFile(argv[this_arg]) ) {
        printMsg( __T("Error opening P.S. file %s, postscript will not be added\n"), argv[this_arg] );
        return(1);
    }

    sigsize = fileh.GetSize();
    postscript.Alloc( sigsize + 1 );
    postscript.SetLength( sigsize );

    if ( !fileh.ReadThisFile(postscript.Get(), sigsize, &dummy, NULL) ) {
        printMsg( __T("Error reading P.S. file %s, postscript will not be added\n"), argv[this_arg] );
        postscript.Free();
    }
    else
        *postscript.GetTail() = __T('\0');

    fileh.Close();
    return(1);
}
#endif

static int checkUserAgent ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    includeUserAgent = TRUE;

    return(0);
}


_TCHAR         disableMPS    = FALSE;

#if SUPPORT_MULTIPART

unsigned long multipartSize = 0;

// The MultiPart message size value is per 1000 bytes.  e.g. 900 would be 900,000 bytes.

static int checkMultiPartSize ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( argc ) {
        long mps = _tstol( argv[this_arg+1] );
        if ( mps > 0 )
            multipartSize = (unsigned long)mps; // If -mps is used with an invalid size, ignore it.

        if ( multipartSize < 50 )   // Minimum size per message is 50,000 bytes.
            multipartSize = 0;

        if ( multipartSize <= ((DWORD)(-1)/1000) )
            multipartSize *= 1000;

        return(1);
    }

    multipartSize = (unsigned long)(-1);    // If -mps specified without a size, then force mps to use
                                            // the SMTP server's max size to split messages.  This has
                                            // no effect on NNTP because there is no size information.
    return(0);
}


static int checkDisableMPS ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    disableMPS = TRUE;

    return(0);
}
#endif


static int checkUnDisROption ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    sendUndisclosed = TRUE;

    return(0);
}

#if BLAT_LITE
#else
static int checkUserContType ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    startargv = startargv;

    userContentType = argv[this_arg+1];
    return(1);
}


static int checkMaxNames ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    int maxn = _tstoi( argv[this_arg+1] );

    if ( maxn > 0 )
        maxNames = maxn;

    return(1);
}

static int checkCommentChar ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( _tcslen( argv[++this_arg] ) == 1 ) {
        if ( argv[this_arg][0] != __T('-') )
            commentChar = argv[this_arg][0];

        return(1);
    }

    return(0);
}

static int checkDelayTime ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    int timeDelay = _tstoi( argv[this_arg+1] );

    if ( timeDelay > 0 )
        delayBetweenMsgs = timeDelay;

    return(1);
}
#endif

#if INCLUDE_SUPERDEBUG
static int checkSuperDebug ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    superDebug = TRUE;
    return checkDebugMode( argc, argv, this_arg, startargv );
}

static int checkSuperDebugT ( int argc, LPTSTR * argv, int this_arg, int startargv )
{
    superDebug = 2;
    return checkDebugMode( argc, argv, this_arg, startargv );
}
#endif

#define CGI_HIDDEN  (LPTSTR)1

/*    optionString           szCgiEntry               preProcess                       usageText
                                                              additionArgC
                                                                 initFunction
*/
_BLATOPTIONS blatOptionsList[] = {
#if BLAT_LITE | !INCLUDE_NNTP
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("Windows console utility to send mail via SMTP") },
#else
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("Windows console utility to send mail via SMTP or post to usenet via NNTP") },
#endif
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("by P.Mendes,M.Neal,G.Vollant,T.Charron,T.Musson,H.Pesonen,A.Donchey,C.Hyde") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("  http://www.blat.net") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("syntax:") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("  Blat <filename> -to <recipient> [optional switches (see below)]") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("") },
#if BLAT_LITE
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("  Blat -SaveSettings -sender <sender email addy> -server <server addr>") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("       [-port <port>] [-try <try>] [-u <login id>] [-pw <password>]") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("  or") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("  Blat -install <server addr> <sender's email addr> [<try>[<port>]] [-q]") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("  Blat -profile [-q]") },
#else
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("  Blat -SaveSettings -f <sender email addy> -server <server addr>") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("       [-port <port>] [-try <try>] [-profile <profile>]") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("       [-u <login id>] [-pw <password>]") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("  or") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("  Blat -install <server addr> <sender's addr> [<try>[<port>[<profile>]]] [-q]") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("  Blat -profile [-delete | \"<default>\"] [profile1] [profileN] [-q]") },
#endif
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("  Blat -h") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("-------------------------------- Installation ---------------------------------") },
#if BLAT_LITE
#else
    { strInstallSMTP        , CGI_HIDDEN             , FALSE, 1, checkInstallOption  , NULL },
#endif
#if INCLUDE_NNTP
    { strInstallNNTP        , CGI_HIDDEN             , FALSE, 1, checkNNTPInstall    , NULL },
#endif
#if INCLUDE_POP3
    { strInstallPOP3        , CGI_HIDDEN             , FALSE, 1, checkPOP3Install    , NULL },
#endif
#if INCLUDE_IMAP
    { strInstallIMAP        , CGI_HIDDEN             , FALSE, 1, checkIMAPInstall    , NULL },
#endif
/*
-install[SMTP|NNTP|POP3|IMAP] <server addr> <sender email addr> [<try n times>
 */
    { strSaveSettings       , CGI_HIDDEN             , FALSE, 0, checkInstallOption  ,              __T("   : store common settings to the Windows Registry.  Takes the") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("                  same parameters as -install, and is only for SMTP settings.") },
    { strInstall            , CGI_HIDDEN             , FALSE, 1, checkInstallOption  ,
#if BLAT_LITE
#else
                                                                                                          __T("[SMTP")
  #if INCLUDE_NNTP
                                                                                                               __T("|NNTP")
  #endif
  #if INCLUDE_POP3
                                                                                                                    __T("|POP3")
  #endif
  #if INCLUDE_IMAP
                                                                                                                         __T("|IMAP")
  #endif
                                                                                                                              __T("]")
#endif
                                                                                                                               __T(" <server addr> <sender email addr> [<try n times>")
#if BLAT_LITE
#else
                                                                                                                                                       },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("                [<port> [<profile> [<username> [<password>]]]")
#endif
                                                                                                 __T("]]") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("                : set server, sender, number of tries and port")
#if BLAT_LITE
#else
                                                                                                                                                     __T(" for profile")
#endif
                                                                                                                                                                          },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("                  (<try n times> and <port> may be replaced by '-')") },
#if INCLUDE_NNTP | INCLUDE_POP3 | INCLUDE_IMAP
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("                  port defaults are SMTP=25")
  #if INCLUDE_NNTP
                                                                                                                                  __T(", NNTP=119")
  #endif
  #if INCLUDE_POP3
                                                                                                                                            __T(", POP3=110")
  #endif
  #if INCLUDE_IMAP
                                                                                                                                                      __T(", IMAP=143")
  #endif
                                                                                                                                                                         },
#else
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("                  port default is 25") },
#endif
#if BLAT_LITE
#else
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("                  default profile can be specified with a '-'") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("                  username and/or password may be stored to the registry") },
#endif
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("                  order of options is specific") },
#if INCLUDE_NNTP
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("                  use -installNNTP for storing NNTP information") },
#endif
#if INCLUDE_POP3
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("                  use -installPOP3 for storing POP3 information") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("                      (sender and try are ignored, use '-' in place of these)") },
#endif
#if INCLUDE_IMAP
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("                  use -installIMAP for storing IMAP information") },
    {                  NULL ,              NULL      , FALSE, 0, NULL                , __T("                      (sender and try are ignored, use '-' in place of these)") },
#endif
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("--------------------------------- The Basics ----------------------------------") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("<filename>      : file with the message body to be sent") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  if your message body is on the command line, use a hyphen (-)") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  as your first argument, and -body followed by your message") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  if your message will come from the console/keyboard, use the") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  hyphen as your first argument, but do not use -body option.") },
#if BLAT_LITE
#else
    { __T("-@")             ,              NULL      , TRUE , 1, checkOptionFile     , NULL },
    { __T("-optionfile")    ,              NULL      , TRUE , 1, checkOptionFile     , NULL },
    { __T("-of")            ,              NULL      , TRUE , 1, checkOptionFile     ,    __T(" <file>      : text file containing more options (also -optionfile)") },
#endif
    { __T("-t")             ,              NULL      , FALSE, 1, checkToOption       , NULL },
    { __T("-to")            ,              NULL      , FALSE, 1, checkToOption       ,    __T(" <recipient> : recipient list (also -t) (comma separated)") },
#if BLAT_LITE
#else
    { __T("-tf")            ,              NULL      , FALSE, 1, checkToFileOption   ,    __T(" <file>      : recipient list filename") },
#endif
    { __T("-c")             ,              NULL      , FALSE, 1, checkCcOption       , NULL },
    { __T("-cc")            ,              NULL      , FALSE, 1, checkCcOption       ,    __T(" <recipient> : carbon copy recipient list (also -c) (comma separated)") },
#if BLAT_LITE
#else
    { __T("-cf")            ,              NULL      , FALSE, 1, checkCcFileOption   ,    __T(" <file>      : cc recipient list filename") },
#endif
    { __T("-b")             ,              NULL      , FALSE, 1, checkBccOption      , NULL },
    { __T("-bcc")           ,              NULL      , FALSE, 1, checkBccOption      ,     __T(" <recipient>: blind carbon copy recipient list (also -b)") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  (comma separated)") },
#if BLAT_LITE
#else
    { __T("-bf")            ,              NULL      , FALSE, 1, checkBccFileOption  ,    __T(" <file>      : bcc recipient list filename") },
    { __T("-maxNames")      ,              NULL      , FALSE, 1, checkMaxNames       ,          __T(" <x>   : send to groups of <x> number of recipients") },
#endif
    { __T("-ur")            ,              NULL      , FALSE, 0, checkUnDisROption   ,    __T("             : set To: header to Undisclosed Recipients if not using the") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  -to and -cc options") },
    { __T("-s")             ,              NULL      , FALSE, 1, checkSubjectOption  , NULL },
    { __T("-subject")       ,              NULL      , FALSE, 1, checkSubjectOption  ,         __T(" <subj> : subject line, surround with quotes to include spaces(also -s)") },
    { __T("-ss")            ,              NULL      , FALSE, 0, checkSkipSubject    ,    __T("             : suppress subject line if not defined") },
#if BLAT_LITE
#else
    { __T("-sf")            ,              NULL      , FALSE, 1, checkSubjectFile    ,    __T(" <file>      : file containing subject line") },
#endif
    { __T("-bodyF")         ,              NULL      , FALSE, 1, checkBodyFile       ,       __T(" <file>   : file containing the message body") },
    { __T("-body")          ,              NULL      , FALSE, 1, checkBodyText       ,      __T(" <text>    : message body, surround with quotes (\") to include spaces") },
#if SUPPORT_SIGNATURES
    { __T("-sigfile")       ,              NULL      , FALSE, 1, checkSigFile        , NULL },
    { __T("-sig")           ,              NULL      , FALSE, 1, checkSigFile        ,     __T(" <file>     : text file containing your email signature") },
#endif
#if SUPPORT_TAGLINES
    { __T("-tagfile")       ,              NULL      , FALSE, 1, checkTagFile        , NULL },
    { __T("-tag")           ,              NULL      , FALSE, 1, checkTagFile        ,     __T(" <file>     : text file containing taglines, to be randomly chosen") },
#endif
#if SUPPORT_POSTSCRIPTS
    { __T("-ps")            ,              NULL      , FALSE, 1, checkPostscript     ,    __T(" <file>      : final message text, possibly for unsubscribe instructions") },
#endif
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("----------------------------- Registry overrides ------------------------------") },
#if BLAT_LITE
#else
    { strP                  ,              NULL      , TRUE , 1, checkProfileToUse   ,   __T(" <profile>    : send with server, user, and port defined in <profile>") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                : use username and password if defined in <profile>") },
#endif
    { strProfile            , CGI_HIDDEN             , FALSE, 0, checkProfileEdit    ,         __T("        : list all profiles in the Registry") },
    { strServer             ,              NULL      , FALSE, 1, checkServerOption   ,        __T(" <addr>  : specify SMTP server to be used (optionally, addr:port)") },
#if BLAT_LITE
#else
    { strServerSMTP         , CGI_HIDDEN             , FALSE, 1, checkServerOption   ,            __T(" <addr>") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                : same as -server") },
  #if INCLUDE_NNTP
    { strServerNNTP         , CGI_HIDDEN             , FALSE, 1, checkNNTPSrvr       ,            __T(" <addr>") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                : specify NNTP server to be used (optionally, addr:port)") },
  #endif
  #if INCLUDE_POP3
    { strServerPOP3         , CGI_HIDDEN             , FALSE, 1, checkPOP3Srvr       ,            __T(" <addr>") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                : specify POP3 server to be used (optionally, addr:port)") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  when POP3 access is required before sending email") },
  #endif
  #if INCLUDE_IMAP
    { strServerIMAP         , CGI_HIDDEN             , FALSE, 1, checkIMAPSrvr       ,            __T(" <addr>") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                : specify IMAP server to be used (optionally, addr:port)") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  when IMAP access is required before sending email") },
  #endif
#endif
    { strF                  , __T("sender")          , FALSE, 1, checkFromOption     ,   __T(" <sender>     : override the default sender address (must be known to server)") },
    { strI                  , __T("from")            , FALSE, 1, checkImpersonate    ,   __T(" <addr>       : a 'From:' address, not necessarily known to the server") },
    { strPort               ,              NULL      , FALSE, 1, checkPortOption     ,      __T(" <port>    : port to be used on the SMTP server, defaults to SMTP (25)") },
#if BLAT_LITE
#else
    { strPortSMTP           , CGI_HIDDEN             , FALSE, 1, checkPortOption     ,          __T(" <port>: same as -port") },
  #if INCLUDE_NNTP
    { strPortNNTP           , CGI_HIDDEN             , FALSE, 1, checkNNTPPort       ,          __T(" <port>: port to be used on the NNTP server, defaults to NNTP (119)") },
  #endif
  #if INCLUDE_POP3
    { strPortPOP3           , CGI_HIDDEN             , FALSE, 1, checkPOP3Port       ,          __T(" <port>: port to be used on the POP3 server, defaults to POP3 (110)") },
  #endif
  #if INCLUDE_IMAP
    { strPortIMAP           , CGI_HIDDEN             , FALSE, 1, checkIMAPPort       ,          __T(" <port>: port to be used on the IMAP server, defaults to IMAP (110)") },
  #endif
#endif
    { strUsername           ,              NULL      , FALSE, 1, checkUserIDOption   , NULL },
    { strU                  ,              NULL      , FALSE, 1, checkUserIDOption   ,   __T(" <username>   : username for AUTH LOGIN (use with -pw)") },
#if SUPPORT_GSSAPI
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  or for AUTH GSSAPI with -k") },
#endif
    { strPassword           ,              NULL      , FALSE, 1, checkPwdOption      , NULL },
    { strPwd                ,              NULL      , FALSE, 1, checkPwdOption      , NULL },
    { strPw                 ,              NULL      , FALSE, 1, checkPwdOption      ,    __T(" <password>  : password for AUTH LOGIN (use with -u)") },
#if INCLUDE_POP3
    { strPop3Username       ,              NULL      , FALSE, 1, checkPop3UIDOption  , NULL },
    { strPop3U              ,              NULL      , FALSE, 1, checkPop3UIDOption  ,    __T(" <username>  : username for POP3 LOGIN (use with -ppw)") },
    { strPop3Password       ,              NULL      , FALSE, 1, checkPop3PwdOption  , NULL },
    { strPop3Pw             ,              NULL      , FALSE, 1, checkPop3PwdOption  ,     __T(" <password> : password for POP3 LOGIN (use with -pu)") },
#endif
#if INCLUDE_IMAP
    { strImapUsername       ,              NULL      , FALSE, 1, checkImapUIDOption  , NULL },
    { strImapU              ,              NULL      , FALSE, 1, checkImapUIDOption  ,    __T(" <username>  : username for IMAP LOGIN (use with -ppw)") },
    { strImapPassword       ,              NULL      , FALSE, 1, checkImapPwdOption  , NULL },
    { strImapPw             ,              NULL      , FALSE, 1, checkImapPwdOption  ,     __T(" <password> : password for IMAP LOGIN (use with -pu)") },
#endif
#if SUPPORT_GSSAPI
    { __T("-k")             ,              NULL      , FALSE, 0, checkGssapiMutual   ,   __T("              : Use ") MECHTYPE __T(" mutual authentication and AUTH GSSAPI") },
    { __T("-kc")            ,              NULL      , FALSE, 0, checkGssapiClient   ,    __T("             : Use ") MECHTYPE __T(" client-only authentication and AUTH GSSAPI") },
    { __T("-service")       ,              NULL      , FALSE, 1, checkServiceName    ,         __T(" <name> : Set GSSAPI service name (use with -k), default \"smtp@server\"") },
    { __T("-level")         ,              NULL      , FALSE, 1, checkProtectionLevel,       __T(" <lev>    : Set GSSAPI protection level to <lev>, which should be one of") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                : None, Integrity, or Privacy (default GSSAPI level is Privacy)")},
#endif
#if BLAT_LITE
#else
    { __T("-nomd5")         ,              NULL      , FALSE, 0, checkBypassCramMD5  ,       __T("          : Do NOT use CRAM-MD5 authentication.  Use this in cases where")},
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  the server's CRAM-MD5 is broken, such as Network Solutions.")},
#endif
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("---------------------- Miscellaneous RFC header switches ----------------------") },
#if BLAT_LITE
#else
    { __T("-o")             ,              NULL      , FALSE, 1, checkOrgOption      , NULL },
    { __T("-org")           ,              NULL      , FALSE, 1, checkOrgOption      , NULL },
    { __T("-organization")  , __T("organisation")    , FALSE, 1, checkOrgOption      ,              __T(" <organization>") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                : Organization field (also -o and -org)") },
#endif
    { __T("-ua")            ,              NULL      , FALSE, 0, checkUserAgent      ,    __T("             : include User-Agent header line instead of X-Mailer") },
#if BLAT_LITE
#else
    { __T("-x")             , __T("xheader")         , FALSE, 1, checkXHeaders       ,   __T(" <X-Header: detail>") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                : custom 'X-' header.  eg: -x \"X-INFO: Blat is Great!\"") },
    { __T("-noh")           ,              NULL      , FALSE, 0, checkNoHeaders      ,     __T("            : prevent X-Mailer/User-Agent header from showing Blat homepage") },
    { __T("-noh2")          ,              NULL      , FALSE, 0, checkNoHeaders2     ,      __T("           : prevent X-Mailer header entirely") },
    { __T("-d")             , __T("notify")          , FALSE, 0, checkDisposition    ,   __T("              : request disposition notification") },
    { __T("-r")             , __T("requestreceipt")  , FALSE, 0, checkReadReceipt    ,   __T("              : request return receipt") },
#endif
    { __T("-charset")       ,              NULL      , FALSE, 1, checkCharset        ,         __T(" <cs>   : user defined charset.  The default is iso-8859-1") },
#if BLAT_LITE
#else
    { __T("-a1")            ,              NULL      , FALSE, 1, checkA1Headers      ,    __T(" <header>    : add custom header line at the end of the regular headers") },
    { __T("-a2")            ,              NULL      , FALSE, 1, checkA2Headers      ,    __T(" <header>    : same as -a1, for a second custom header line") },
    { __T("-dsn")           ,              NULL      , FALSE, 1, checkDeliveryStat   ,     __T(" <nsfd>     : use Delivery Status Notifications (RFC 3461)") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  n = never, s = successful, f = failure, d = delayed") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  can be used together, however N takes precedence") },
    { __T("-hdrencb")       ,              NULL      , FALSE, 0, checkHdrEncB        ,         __T("        : use base64 for encoding headers, if necessary") },
    { __T("-hdrencq")       ,              NULL      , FALSE, 0, checkHdrEncQ        ,         __T("        : use quoted-printable for encoding headers, if necessary") },
#endif
    { __T("-priority")      ,              NULL      , FALSE, 1, checkPriority       ,          __T(" <pr>  : set message priority 0 for low, 1 for high") },
    { __T("-sensitivity")   ,              NULL      , FALSE, 1, checkSensitivity    ,             __T(" <s>: set message sensitivity 0 for personal, 1 for private,") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  2 for company-confidential") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("----------------------- Attachment and encoding options -----------------------") },
    { __T("-attach")        , CGI_HIDDEN             , FALSE, 1, checkBinFileAttach  ,        __T(" <file>  : attach binary file(s) to message (filenames comma separated)") },
    { __T("-attacht")       , CGI_HIDDEN             , FALSE, 1, checkTxtFileAttach  ,         __T(" <file> : attach text file(s) to message (filenames comma separated)") },
    { __T("-attachi")       , CGI_HIDDEN             , FALSE, 1, checkInlineAttach   ,         __T(" <file> : attach text file(s) as INLINE (filenames comma separated)") },
#if BLAT_LITE
#else
    { __T("-embed")         , CGI_HIDDEN             , FALSE, 1, checkBinFileEmbed   ,       __T(" <file>   : embed file(s) in HTML.  Object tag in HTML must specify") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  content-id using cid: tag.  eg: <img src=\"cid:image.jpg\">") },
    { __T("-af")            ,              NULL      , FALSE, 1, checkBinFileAttFil  ,    __T(" <file>      : file containing list of binary file(s) to attach (comma") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  separated)") },
    { __T("-atf")           ,              NULL      , FALSE, 1, checkTxtFileAttFil  ,     __T(" <file>     : file containing list of text file(s) to attach (comma") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  separated)") },
    { __T("-aef")           ,              NULL      , FALSE, 1, checkEmbFileAttFil  ,     __T(" <file>     : file containing list of embed file(s) to attach (comma") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  separated)") },
    { __T("-base64")        ,              NULL      , FALSE, 0, checkBase64Enc      ,        __T("         : send binary files using base64 (binary MIME)") },
    { __T("-uuencodel")     ,              NULL      , FALSE, 0, checkLongUUEncode   , NULL },
    { __T("-uuencode")      ,              NULL      , FALSE, 0, checkUUEncode       ,          __T("       : send binary files UUEncoded") },
    { __T("-enriched")      ,              NULL      , FALSE, 0, checkEnriched       ,          __T("       : send an enriched text message (Content-Type=text/enriched)") },
    { __T("-unicode")       ,              NULL      , FALSE, 0, checkUnicode        ,         __T("        : message body is in 16- or 32-bit Unicode format") },
#endif
    { __T("-html")          ,              NULL      , FALSE, 0, checkHTML           ,      __T("           : send an HTML message (Content-Type=text/html)") },
#if BLAT_LITE
#else
    { __T("-alttext")       ,              NULL      , FALSE, 1, checkAltText        ,         __T(" <text> : plain text for use as alternate text") },
    { __T("-althtml")       ,              NULL      , FALSE, 1, checkAltTextFile    , NULL },
    { __T("-htmaltf")       ,              NULL      , FALSE, 1, checkAltTextFile    , NULL },
    { __T("-alttextf")      ,              NULL      , FALSE, 1, checkAltTextFile    ,          __T(" <file>: plain text file for use as alternate text") },
#endif
    { __T("-mime")          ,              NULL      , FALSE, 0, checkMime           ,      __T("           : MIME Quoted-Printable Content-Transfer-Encoding") },
#if BLAT_LITE
#else
    { __T("-8bitmime")      ,              NULL      , FALSE, 0, check8bitMime       ,          __T("       : ask for 8bit data support when sending MIME") },
#endif
#if SUPPORT_YENC
    { __T("-yenc")          ,              NULL      , FALSE, 0, check_yEnc          ,      __T("           : send binary files encoded with yEnc") },
#endif
#if SUPPORT_MULTIPART
    { __T("-mps")           ,              NULL      , FALSE, 0, checkMultiPartSize  , NULL },
    { __T("-multipart")     ,              NULL      , FALSE, 0, checkMultiPartSize  ,           __T(" <size>") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                : send multipart messages, breaking attachments on <size>") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  KB boundaries, where <size> is per 1000 bytes") },
    { __T("-nomps")         ,              NULL      , FALSE, 0, checkDisableMPS     ,       __T("          : do not allow multipart messages") },
#endif
#if BLAT_LITE
#else
    { __T("-contentType")   ,              NULL      , FALSE, 1, checkUserContType   ,             __T(" <string>") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                : use <string> in the ContentType header for attachments that") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  do not have a registered content type for the extension") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  For example: -contenttype \"text/calendar\"") },
#endif
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("") },
#if INCLUDE_NNTP
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("---------------------------- NNTP specific options ----------------------------") },
    { __T("-group")         , CGI_HIDDEN             , FALSE, 1, checkNNTPGroups     , NULL },
    { __T("-groups")        , CGI_HIDDEN             , FALSE, 1, checkNNTPGroups     ,        __T(" <usenet groups>") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                : list of newsgroups (comma separated)") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("") },
#endif
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("-------------------------------- Other options --------------------------------") },
#if INCLUDE_POP3
    { __T("-xtndxmit")      ,              NULL      , 0    , 0, checkXtndXmit       ,          __T("       : Attempt to use POP3 to transmit when accessing POP3 first") },
#endif
    { __T("/h")             , CGI_HIDDEN             , FALSE, 0, checkHelp           , NULL },
    { __T("/?")             , CGI_HIDDEN             , FALSE, 0, checkHelp           , NULL },
    { __T("-?")             , CGI_HIDDEN             , FALSE, 0, checkHelp           , NULL },
    { __T("-help")          , CGI_HIDDEN             , FALSE, 0, checkHelp           , NULL },
    { __T("-h")             , CGI_HIDDEN             , FALSE, 0, checkHelp           ,   __T("              : displays this help (also -?, /?, -help or /help)") },
    { __T("-q")             ,              NULL      , TRUE , 0, checkQuietMode      ,   __T("              : suppresses all output to the screen") },
    { __T("-debug")         ,              NULL      , TRUE , 0, checkDebugMode      ,       __T("          : echoes server communications to a log file or screen") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  (overrides -q if echoes to the screen)") },
    { __T("-log")           ,              NULL      , TRUE , 1, checkLogMessages    ,     __T(" <file>     : log everything but usage to <file>") },
    { __T("-timestamp")     ,              NULL      , FALSE, 0, checkTimestamp      ,           __T("      : when -log is used, a timestamp is added to each log line") },
    { __T("-overwritelog")  ,              NULL      , TRUE , 0, checkLogOverwrite   ,              __T("   : when -log is used, overwrite the log file") },
    { __T("-ti")            , __T("timeout")         , FALSE, 1, checkTimeout        ,    __T(" <n>         : set timeout to 'n' seconds.  Blat will wait 'n' seconds for") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  server responses") },
    { strTry                ,              NULL      , FALSE, 1, checkAttempts       ,     __T(" <n times>  : how many times blat should try to send (1 to 'INFINITE')") },
    { __T("-binary")        ,              NULL      , FALSE, 0, checkFixPipe        ,        __T("         : do not convert ASCII | (pipe, 0x7c) to CrLf in the message") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  body") },
    { __T("-hostname")      ,              NULL      , FALSE, 1, checkHostname       ,          __T(" <hst> : select the hostname used to send the message via SMTP") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  this is typically your local machine name") },
    { __T("-penguin")       ,              NULL      , FALSE, 0, checkRaw            , NULL },
    { __T("-raw")           ,              NULL      , FALSE, 0, checkRaw            ,     __T("            : do not add CR/LF after headers") },
#if BLAT_LITE
#else
    { __T("-delay")         ,              NULL      , FALSE, 1, checkDelayTime      ,       __T(" <x>      : wait x seconds between messages being sent when used with") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  -maxnames")
  #if SUPPORT_MULTIPART
                                                                                                            __T(" or -multipart")
  #endif
                                                                                                                                   },
    { __T("-comment")       ,              NULL      , TRUE , 1, checkCommentChar    ,         __T(" <char> : use this character to mark the start of comments in") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  options files and recipient list files.  The default is ;") },
#endif
#if INCLUDE_SUPERDEBUG
    { __T("-superdebug")    ,              NULL      , TRUE , 0, checkSuperDebug     ,            __T("     : hex/ascii dump the data between Blat and the server") },
    { __T("-superdebugT")   ,              NULL      , TRUE , 0, checkSuperDebugT    ,             __T("    : ascii dump the data between Blat and the server") },
#endif
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("-------------------------------------------------------------------------------") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("Note that if the '-i' option is used, <sender> is included in 'Reply-to:'") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("and 'Sender:' fields in the header of the message.") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("Optionally, the following options can be used instead of the -f and -i") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("options:") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("") },
    { strMailFrom           ,              NULL      , FALSE, 1, checkMailFrom       ,          __T(" <addr>   The RFC 821 MAIL From: statement") },
    { strFrom               ,              NULL      , FALSE, 1, checkWhoFrom        ,      __T(" <addr>       The RFC 822 From: statement") },
    { __T("-replyto")       ,              NULL      , FALSE, 1, checkReplyTo        ,         __T(" <addr>    The RFC 822 Reply-To: statement") },
    { __T("-returnpath")    ,              NULL      , FALSE, 1, checkReturnPath     ,            __T(" <addr> The RFC 822 Return-Path: statement") },
    { strSender             ,              NULL      , FALSE, 1, checkSender         ,        __T(" <addr>     The RFC 822 Sender: statement") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("For backward consistency, the -f and -i options have precedence over these") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("RFC 822 defined options.  If both -f and -i options are omitted then the") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("RFC 821 MAIL FROM statement will be defaulted to use the installation-defined") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("default sender address.") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , NULL } };


#ifdef _WIN64
#define BIT_SIZE        __T("64")
#else
#define BIT_SIZE        __T("32")
#endif

#if BLAT_LITE
#define FEATURES_TEXT   __T("Lite")
#elif BASE_SMTP_ONLY
#define FEATURES_TEXT   __T("SMTP only")
#elif SUPPORT_YENC
#define FEATURES_TEXT   __T("Full, yEnc")
#else
#define FEATURES_TEXT   __T("Full")
#endif
#if defined(_UNICODE) || defined(UNICODE)
#define UNICODE_TEXT    __T(", Unicode")
#else
#define UNICODE_TEXT    __T("")
#endif

void printTitleLine( int quiet )
{
    static int titleLinePrinted = FALSE;
    _TCHAR     tmpstr[1024];



    if ( !titleLinePrinted ) {
        _stprintf( tmpstr, __T("Blat v%s%s (build : %s %s)\n") BIT_SIZE __T("-bit Windows, ") FEATURES_TEXT UNICODE_TEXT __T("\n\n"), blatVersion, blatVersionSuf, blatBuildDate, blatBuildTime );
        if ( !quiet )
            pMyPrintDLL( tmpstr );

        if ( logOut )
            printMsg( __T("%s"), tmpstr );
    }

    titleLinePrinted = TRUE;
}

void print_usage_line( LPTSTR pUsageLine )
{
#if SUPPORT_GSSAPI
    _TCHAR     * match;
    _TCHAR       beginning[USAGE_LINE_SIZE];
    static BOOL have_mechtype = FALSE;
    static _TCHAR mechtype[MECHTYPE_SIZE] = __T("UNKNOWN");

    //Make sure all lines about GSSAPI options contain "GSSAPI"
    if ( bSuppressGssOptionsAtRuntime && _tcsstr(pUsageLine,__T("GSSAPI")) )
        return;

    if ( _tcsstr( pUsageLine, __T("GSSAPI") ) ) {
        if ( !have_mechtype ) {
            try {
                if (!pGss)
                {
                    static GssSession TheSession;
                    pGss = &TheSession;
                }
                pGss->MechtypeText(mechtype);
            }
            catch (GssSession::GssException &e) {
                printMsg(__T("GssSession Error: %s\n"),e.message());
                pGss = NULL;
                _tcscpy( mechtype,__T("UNAVAILABLE") );
            }

            have_mechtype = TRUE;
        }

        match = _tcsstr(pUsageLine,MECHTYPE);
        if ( match ) {
            memcpy( beginning, pUsageLine, match - pUsageLine );
            beginning[match - pUsageLine] = __T('\0');

            pMyPrintDLL(beginning);
            pMyPrintDLL(mechtype);
            pMyPrintDLL(match + _tcslen(MECHTYPE));
            fflush(stdout);
            return;
        }
    }
#endif
    pMyPrintDLL(pUsageLine);
    fflush(stdout);
}


int printUsage( int optionPtr )
{
    int  i;
    _TCHAR printLine[USAGE_LINE_SIZE];

    printTitleLine( FALSE );

    if ( !optionPtr ) {
        for ( i = 0; ;  ) {
            if ( !blatOptionsList[i].optionString &&
                 !blatOptionsList[i].preprocess   &&
                 !blatOptionsList[i].additionArgC &&
                 !blatOptionsList[i].initFunction &&
                 !blatOptionsList[i].usageText    )
                break;

            if ( blatOptionsList[i].usageText ) {
                _stprintf( printLine, __T("%s%s\n"),
                           blatOptionsList[i].optionString ? blatOptionsList[i].optionString : __T(""),
                           blatOptionsList[i].usageText );
                print_usage_line( printLine );
            }

            if ( blatOptionsList[++i].usageText ) {
                if ( (blatOptionsList[i].usageText[0] == __T('-')) && (blatOptionsList[i].usageText[1] == __T('-')) )
                    break;
            }
        }
    } else
    if ( optionPtr == TRUE ) {
        for ( i = 0; ; i++ ) {
            if ( !blatOptionsList[i].optionString &&
                 !blatOptionsList[i].preprocess   &&
                 !blatOptionsList[i].additionArgC &&
                 !blatOptionsList[i].initFunction &&
                 !blatOptionsList[i].usageText    )
                break;

            if ( blatOptionsList[i].usageText ) {
                _stprintf( printLine, __T("%s%s\n"),
                           blatOptionsList[i].optionString ? blatOptionsList[i].optionString : __T(""),
                           blatOptionsList[i].usageText );
                if ( printLine[0] != __T('\0') )
                    print_usage_line( printLine );
            }
        }
    } else {
        pMyPrintDLL( __T("Blat found fault with: ") );
        pMyPrintDLL( blatOptionsList[optionPtr].optionString );
        pMyPrintDLL( __T("\n\n") );
        for ( ; !blatOptionsList[optionPtr].usageText; optionPtr++ )
            ;

        for ( ; ; optionPtr++ ) {
            if ( !blatOptionsList[optionPtr].usageText    ||
                 !blatOptionsList[optionPtr].usageText[0] ||
                 (blatOptionsList[optionPtr].usageText[0] == __T('-')) )
                break;

            _stprintf( printLine, __T("%s%s\n"),
                       blatOptionsList[optionPtr].optionString ? blatOptionsList[optionPtr].optionString : __T(""),
                       blatOptionsList[optionPtr].usageText );
            print_usage_line( printLine );

            if ( blatOptionsList[optionPtr+1].optionString && blatOptionsList[optionPtr+1].optionString[0] == __T('-') )
                break;
        }
    }

    return 1;
}


int processOptions( int argc, LPTSTR * argv, int startargv, int preprocessing )
{
    int i, this_arg, retval;

#if SUPPORT_GSSAPI
    // If the GSSAPI library is unavailable, make the GSSAPI options simply
    // unavailable at runtime.  It's less efficient to do it this way but
    // it doesn't intrude into the rest of the code below.
    if (bSuppressGssOptionsAtRuntime)
    {
         for (i = 0; blatOptionsList[i].optionString; i++)
         {
             if ( (blatOptionsList[i].initFunction == checkGssapiMutual   ) ||
                  (blatOptionsList[i].initFunction == checkGssapiClient   ) ||
                  (blatOptionsList[i].initFunction == checkServiceName    ) ||
                  (blatOptionsList[i].initFunction == checkProtectionLevel) )
                 blatOptionsList[i].optionString = __T("");

         }
    }
#endif

/* To process the actual command line options, startargv needs to be 1.
 * To process options from a file, then startargv needs to be 0.
 *
 * As such, if startargv == 0, then do not allow the following:
 *   -install
 *   -installsmtp
 *   -installnntp
 *   -profile
 *   processing a filename as the first entry (for message body)
 */
    processedOptions.Free();
    for ( this_arg = startargv; this_arg < argc; this_arg++ ) {
        if ( !argv[this_arg][0] )   // If we already dealt with a given option,
            continue;               // it will have been removed from the list.

        if ( argv[this_arg][0] == __T('/') )
            argv[this_arg][0] = __T('-');

#if INCLUDE_SUPERDEBUG
        if ( superDebug ) {
            pMyPrintDLL( __T("Checking option ") );
            pMyPrintDLL( argv[this_arg] );
            pMyPrintDLL( __T("\n") );
        }
#endif

        for ( i = 0; ; i++ ) {
            if ( !blatOptionsList[i].optionString &&
                 !blatOptionsList[i].preprocess   &&
                 !blatOptionsList[i].additionArgC &&
                 !blatOptionsList[i].initFunction &&
                 !blatOptionsList[i].usageText    )
                break;

            if ( !blatOptionsList[i].optionString )
                continue;

            if ( _tcsicmp(blatOptionsList[i].optionString, argv[this_arg]) == 0 ) {
                int next_arg;

                if ( _tcsnicmp(argv[this_arg], strInstall, 8 ) == 0 ) {
                    next_arg = argc;
                } else {
                    for ( next_arg = this_arg + 1; next_arg < argc; next_arg++ ) {
                        if ( next_arg == (this_arg + 1) )
                            if ( _tcscmp( argv[this_arg], __T("-profile") ) == 0 )
                                if ( _tcscmp( argv[next_arg], __T("-delete") ) == 0 )
                                    continue;

                        if ( argv[next_arg][0] == __T('-') )
                            if ( argv[next_arg][1] ) {
                                int check, ok;

                                ok = TRUE;
                                for ( check = 0; ; check++ ) {
                                    if ( !blatOptionsList[check].optionString &&
                                         !blatOptionsList[check].preprocess   &&
                                         !blatOptionsList[check].additionArgC &&
                                         !blatOptionsList[check].initFunction &&
                                         !blatOptionsList[check].usageText    )
                                        break;

                                    if ( !blatOptionsList[check].optionString )
                                        continue;

                                    if ( _tcsicmp(blatOptionsList[check].optionString, argv[next_arg]) == 0 ) {
                                        ok = FALSE;
                                        break;
                                    }
                                }
                                if ( !ok )
                                    break;
                            }

                        if ( !argv[next_arg][0] )
                            break;
                    }
                }

                if ( blatOptionsList[i].additionArgC <= (next_arg - (this_arg + 1)) ) {
                    if ( blatOptionsList[i].preprocess == preprocessing ) {
                        retval = (*blatOptionsList[i].initFunction)(next_arg - (this_arg + 1), argv, this_arg, startargv);
                        if ( exitRequired || (retval < 0) ) {
                            processedOptions.Free();
                            return( 0 - retval );
                        }

                        processedOptions.Add( __T("    ") );
                        processedOptions.Add( argv[this_arg] );

                        // Remove the options we processed, so we do not attempt
                        // to process them a second time.
                        argv[this_arg][0] = __T('\0');
                        for ( ; retval; retval-- ) {
                            processedOptions.Add( __T(' ') );
                            processedOptions.Add( argv[++this_arg] );
                            argv[this_arg][0] = __T('\0');
                        }
                        processedOptions.Add( __T("\r\n") );
                    } else
                        this_arg = next_arg - 1;
                } else {
                    if ( !preprocessing ) {
                        processedOptions.Add( __T("    ") );
                        processedOptions.Add( argv[this_arg] );
                        printMsg( __T("Blat saw and processed these options, and %s the last one...\n%s\n\n"),
                                  __T("found fault with"), processedOptions.Get() );
                        printMsg( __T("Not enough arguments supplied for option: %s\n"),
                                  argv[this_arg] );

                        for ( ; ; ) {
                            if ( printUsage( i ) )
                                break;

                            i++;    // Attempt to locate the main option name for the abbreviation we found.
                        }

                        processedOptions.Free();
                        return (1);
                    }
                }

                break;
            }
        }

        if ( !blatOptionsList[i].optionString && !preprocessing ) {
            processedOptions.Add( __T("    ") );
            processedOptions.Add( argv[this_arg] );
            printMsg( __T("Blat saw and processed these options, and %s the last one...\n%s\n\n"),
                      __T("was confused by"), processedOptions.Get() );
            printMsg( __T("Do not understand argument: %s\n"),
                      argv[this_arg] );
            printUsage( 0 );
            processedOptions.Free();
            return(1);
        }
    }

    if ( !logOut ) {
        if ( debug ) {
            quiet = FALSE;
        }
    }

    processedOptions.Free();
    return(0);
}
