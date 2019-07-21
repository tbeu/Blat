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
#include "common_data.h"
#include "blatext.hpp"
#include "macros.h"
#include "gensock.h"
#include "options.hpp"
#include "reg.hpp"
#include "parsing.hpp"
#include "unicode.hpp"
#include "sendsmtp.hpp"
#include "sendnntp.hpp"

static _TCHAR strInstall[]       = __T("-install");
static _TCHAR strSaveSettings[]  = __T("-SaveSettings");
#if BLAT_LITE
#else
static _TCHAR strInstallSMTP[]   = __T("-installSMTP");
  #if INCLUDE_NNTP
static _TCHAR strInstallNNTP[]   = __T("-installNNTP");
  #endif
  #if INCLUDE_POP3
static _TCHAR strInstallPOP3[]   = __T("-installPOP3");
  #endif
  #if INCLUDE_IMAP
static _TCHAR strInstallIMAP[]   = __T("-installIMAP");
  #endif
#endif

static _TCHAR strP[]             = __T("-p");
static _TCHAR strProfile[]       = __T("-profile");
static _TCHAR strServer[]        = __T("-server");
#if BLAT_LITE
#else
static _TCHAR strServerSMTP[]    = __T("-serverSMTP");
  #if INCLUDE_NNTP
static _TCHAR strServerNNTP[]    = __T("-serverNNTP");
  #endif
  #if INCLUDE_POP3
static _TCHAR strServerPOP3[]    = __T("-serverPOP3");
  #endif
  #if INCLUDE_IMAP
static _TCHAR strServerIMAP[]    = __T("-serverIMAP");
  #endif
#endif
static _TCHAR strF[]             = __T("-f");
static _TCHAR strI[]             = __T("-i");
static _TCHAR strPort[]          = __T("-port");
#if BLAT_LITE
#else
static _TCHAR strPortSMTP[]      = __T("-portSMTP");
  #if INCLUDE_NNTP
static _TCHAR strPortNNTP[]      = __T("-portNNTP");
  #endif
  #if INCLUDE_POP3
static _TCHAR strPortPOP3[]      = __T("-portPOP3");
  #endif
  #if INCLUDE_IMAP
static _TCHAR strPortIMAP[]      = __T("-portIMAP");
  #endif
#endif
static _TCHAR strUsername[]      = __T("-username");
static _TCHAR strU[]             = __T("-u");
static _TCHAR strPassword[]      = __T("-password");
static _TCHAR strPwd[]           = __T("-pwd");
static _TCHAR strPw[]            = __T("-pw");
#if INCLUDE_POP3
static _TCHAR strPop3Username[]  = __T("-pop3username");
static _TCHAR strPop3U[]         = __T("-pu");
static _TCHAR strPop3Password[]  = __T("-pop3password");
static _TCHAR strPop3Pw[]        = __T("-ppw");
#endif
#if INCLUDE_IMAP
static _TCHAR strImapUsername[]  = __T("-imapusername");
static _TCHAR strImapU[]         = __T("-iu");
static _TCHAR strImapPassword[]  = __T("-imappassword");
static _TCHAR strImapPw[]        = __T("-ipw");
#endif
static _TCHAR strTry[]           = __T("-try");
static _TCHAR strMailFrom[]      = __T("-mailfrom");
static _TCHAR strFrom[]          = __T("-from");
static _TCHAR strSender[]        = __T("-sender");


#if BLAT_LITE
#else

static int ReadNamesFromFile(COMMON_DATA & CommonData, LPTSTR type, LPTSTR namesfilename, Buf &listofnames) {
    WinFile fileh;
    int     found;
    DWORD   filesize;
    DWORD   dummy;
    Buf     p;
    Buf     tmpstr;
    LPTSTR  pString;

    if ( !fileh.OpenThisFile(namesfilename)) {
        printMsg(CommonData, __T("error opening %s, aborting\n"), namesfilename);
        return(3);
    }
    filesize = fileh.GetSize();
    tmpstr.Alloc( filesize + 1 );
    //if ( !tmpstr ) {
    //    fileh.Close();
    //    printMsg(CommonData, __T("error allocating memory for reading %s, aborting\n"), namesfilename);
    //    return(5);
    //}

    if ( !fileh.ReadThisFile(tmpstr.Get(), filesize, &dummy, NULL) ) {
        fileh.Close();
        tmpstr.Free();
        printMsg(CommonData, __T("error reading %s, aborting\n"), namesfilename);
        return(5);
    }
    fileh.Close();
    tmpstr.SetLength( filesize );
  #if defined(_UNICODE) || defined(UNICODE)
    Buf sourceText;

    sourceText.Clear();
    sourceText.Add( tmpstr );
    checkInputForUnicode( CommonData, sourceText );
    tmpstr = sourceText;
    tmpstr.Add( __T('\0') );
  #endif
    parseCommaDelimitString( CommonData, tmpstr.Get(), p, FALSE );
    tmpstr = p;
    tmpstr.Add( __T('\0') );
    pString = tmpstr.Get();

    if ( !pString ) {
        printMsg(CommonData, __T("error finding email addresses in %s, aborting\n"), namesfilename);
        return(5);
    }

    // Count and consolidate addresses.
    found = 0;
    for ( ; *pString; pString += _tcslen(pString) + 1 ) {
        if ( found )
            listofnames.Add( __T(',') );

        listofnames.Add( pString );
        found++;
    }

    printMsg(CommonData, __T("Read %d %s address%s from %s\n"), found, type, (found == 1) ? __T("") : __T("es"), namesfilename);

    p.Free();
    tmpstr.Free();
    return(0);                                   // indicates no error.
}
#endif

static void checkProfileNameLength( COMMON_DATA & CommonData )
{
    if ( CommonData.Profile.Length() > TRY_SIZE ) {
        CommonData.Profile.Get()[TRY_SIZE] = __T('\0');
        CommonData.Profile.SetLength();
    }
}

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
    INSTALL_STATE_ANYTHING_MISSED,
    INSTALL_STATE_DONE
} _INSTALL_STATES;


static int checkInstallOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
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

    CommonData.SMTPHost.Clear();
    CommonData.Profile.Clear();

    CommonData.Sender.Clear();

    CommonData.AUTHLogin.Clear();
    CommonData.AUTHPassword.Clear();

    CommonData.SMTPPort = defaultSMTPPort;
    CommonData.Try      = __T("1");

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
                CommonData.SMTPHost = argv[this_arg];
                hostFound = TRUE;
                argState = INSTALL_STATE_SERVER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strF       )
          || !_tcsicmp(argv[this_arg], strFrom    )
          || !_tcsicmp(argv[this_arg], strI       )
          || !_tcsicmp(argv[this_arg], strSender  )
          || !_tcsicmp(argv[this_arg], strMailFrom)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                CommonData.Sender = argv[this_arg];
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
                CommonData.Try = argv[this_arg];
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
                CommonData.SMTPPort = argv[this_arg];
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
                    CommonData.Profile = argv[this_arg];          // Keep the profile if not "-"
                    checkProfileNameLength( CommonData );
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
                    CommonData.AUTHLogin = argv[this_arg];        // Keep the login name if not "-"
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
                    CommonData.AUTHPassword = argv[this_arg];     // Keep the password if not "-"
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
                CommonData.SMTPHost = argv[this_arg];
                continue;
            }

        case INSTALL_STATE_SENDER:
            argState++;
            if ( !senderFound ) {
                senderFound = TRUE;
                CommonData.Sender = argv[this_arg];
                continue;
            }

        case INSTALL_STATE_TRY:
            argState++;
            if ( !tryFound ) {
                tryFound = TRUE;
                CommonData.Try = argv[this_arg];
                continue;
            }

        case INSTALL_STATE_PORT:
            argState++;
            if ( !portFound ) {
                portFound = TRUE;
                CommonData.SMTPPort = argv[this_arg];
                continue;
            }

        case INSTALL_STATE_PROFILE:
            argState++;
            if ( !profileFound ) {
                profileFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the profile.
                    CommonData.Profile = argv[this_arg];          // Keep the profile if not "-"
                    checkProfileNameLength( CommonData );
                }
                continue;
            }

        case INSTALL_STATE_LOGIN_ID:
            argState++;
            if ( !loginFound ) {
                loginFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the login name.
                    CommonData.AUTHLogin = argv[this_arg];        // Keep the login name if not "-"
                }
                continue;
            }

        case INSTALL_STATE_PASSWORD:
            argState++;
            if ( !pwdFound ) {
                pwdFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then delete the password.
                    CommonData.AUTHPassword = argv[this_arg];     // Keep the password if not "-"
                }
                continue;
            }

        case INSTALL_STATE_ANYTHING_MISSED:
            argState++;
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
        if ( !_tcscmp(CommonData.Try.Get(),__T("-")) ) CommonData.Try = __T("1");
        if ( !_tcscmp(CommonData.Try.Get(),__T("0")) ) CommonData.Try = __T("1");

        if ( !_tcscmp(CommonData.SMTPPort.Get(), __T("-")) ) CommonData.SMTPPort = defaultSMTPPort;
        if ( !_tcscmp(CommonData.SMTPPort.Get(), __T("0")) ) CommonData.SMTPPort = defaultSMTPPort;

#if INCLUDE_NNTP
        CommonData.NNTPHost.Clear();
#endif
#if INCLUDE_POP3
        CommonData.POP3Host.Clear();
#endif
#if INCLUDE_IMAP
        CommonData.IMAPHost.Clear();
#endif
        if ( CreateRegEntry( CommonData, useHKCU ) == 0 ) {
            printMsg( CommonData, __T("SMTP server set to %s on port %s with user %s, retry %s time(s)\n"), CommonData.SMTPHost.Get(), CommonData.SMTPPort.Get(), CommonData.Sender.Get(), CommonData.Try.Get() );
            CommonData.regerr = 0;
            printMsg( CommonData, NULL );
            if ( CommonData.logOut )
                if ( CommonData.logOut != stdout )
                    fclose( CommonData.logOut );

            CommonData.exitRequired = TRUE;
            return(0);
        }
    } else {
        printMsg( CommonData, __T("To set the SMTP server's name/address and your username/email address for that\n") \
                              __T("server machine do:\n") \
                              __T("blat -install  server_name  your_email_address\n") \
                              __T("or use '-server <server_name>' and '-f <your_email_address>'\n") );
        return(-6);
    }

    return(startargv);
}

#if INCLUDE_NNTP

static int checkNNTPInstall ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
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

    CommonData.NNTPHost.Clear();
    CommonData.Profile.Clear();

    CommonData.Sender.Clear();

    CommonData.AUTHLogin.Clear();
    CommonData.AUTHPassword.Clear();

    CommonData.NNTPPort = defaultNNTPPort;
    CommonData.Try = __T("1");

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
                CommonData.NNTPHost = argv[this_arg];
                hostFound = TRUE;
                argState = INSTALL_STATE_SERVER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strF       )
          || !_tcsicmp(argv[this_arg], strFrom    )
          || !_tcsicmp(argv[this_arg], strI       )
          || !_tcsicmp(argv[this_arg], strSender  )
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                CommonData.Sender = argv[this_arg];
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
                CommonData.Try = argv[this_arg];
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
                CommonData.NNTPPort = argv[this_arg];
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
                    CommonData.Profile = argv[this_arg];          // Keep the profile if not "-"
                    checkProfileNameLength( CommonData );
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
                    CommonData.AUTHLogin = argv[this_arg];        // Keep the login name if not "-"
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
                    CommonData.AUTHPassword = argv[this_arg];     // Keep the password if not "-"
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
                CommonData.NNTPHost = argv[this_arg];
                continue;
            }

        case INSTALL_STATE_SENDER:
            argState++;
            if ( !senderFound ) {
                senderFound = TRUE;
                CommonData.Sender = argv[this_arg];
                continue;
            }

        case INSTALL_STATE_TRY:
            argState++;
            if ( !tryFound ) {
                tryFound = TRUE;
                CommonData.Try = argv[this_arg];
                continue;
            }

        case INSTALL_STATE_PORT:
            argState++;
            if ( !portFound ) {
                portFound = TRUE;
                CommonData.NNTPPort = argv[this_arg];
                continue;
            }

        case INSTALL_STATE_PROFILE:
            argState++;
            if ( !profileFound ) {
                profileFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the profile.
                    CommonData.Profile = argv[this_arg];          // Keep the profile if not "-"
                    checkProfileNameLength( CommonData );
                }
                continue;
            }

        case INSTALL_STATE_LOGIN_ID:
            argState++;
            if ( !loginFound ) {
                loginFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the login name.
                    CommonData.AUTHLogin = argv[this_arg];        // Keep the login name if not "-"
                }
                continue;
            }

        case INSTALL_STATE_PASSWORD:
            argState++;
            if ( !pwdFound ) {
                pwdFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then delete the password.
                    CommonData.AUTHPassword = argv[this_arg];     // Keep the password if not "-"
                }
                continue;
            }

        case INSTALL_STATE_ANYTHING_MISSED:
            argState++;
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
        if ( !_tcscmp(CommonData.Try.Get(),__T("-")) ) CommonData.Try = __T("1");
        if ( !_tcscmp(CommonData.Try.Get(),__T("0")) ) CommonData.Try = __T("1");

        if ( !_tcscmp(CommonData.NNTPPort.Get(),__T("-")) ) CommonData.NNTPPort = defaultNNTPPort;
        if ( !_tcscmp(CommonData.NNTPPort.Get(),__T("0")) ) CommonData.NNTPPort = defaultNNTPPort;

  #if INCLUDE_POP3
        CommonData.POP3Host.Clear();
  #endif
  #if INCLUDE_IMAP
        CommonData.IMAPHost.Clear();
  #endif
        CommonData.SMTPHost.Clear();
        if ( CreateRegEntry( CommonData, useHKCU ) == 0 ) {
            printMsg( CommonData, __T("NNTP server set to %s on port %s with user %s, retry %s time(s)\n"), CommonData.NNTPHost.Get(), CommonData.NNTPPort.Get(), CommonData.Sender.Get(), CommonData.Try.Get() );
            CommonData.regerr = 0;
            printMsg( CommonData, NULL );
            if ( CommonData.logOut )
                if ( CommonData.logOut != stdout )
                    fclose( CommonData.logOut );

            CommonData.exitRequired = TRUE;
            return(0);
        }
    } else {
        printMsg( CommonData, __T("To set the NNTP server's address and the user name at that address do:\nblat -installNNTP server username\n"));
        return(-6);
    }

    return(startargv);
}

static int checkNNTPSrvr ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.NNTPHost = argv[this_arg+1];
    return(1);
}

static int checkNNTPPort ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.NNTPPort = argv[this_arg+1];
    return(1);
}

static int checkNNTPGroups ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    if ( CommonData.groups.Length() ) {
        printMsg(CommonData, __T("Only one -groups can be used at a time.  Aborting.\n"));
        return(-1);
    }
    CommonData.groups = argv[this_arg+1];

    return(1);
}
#endif

#if INCLUDE_POP3

static int checkXtndXmit ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.xtnd_xmit_wanted = TRUE;

    return(0);
}

static int checkPOP3Install ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
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

    CommonData.POP3Host.Clear();
    CommonData.Profile.Clear();

    CommonData.Sender.Clear();

    CommonData.POP3Login.Clear();
    CommonData.POP3Password.Clear();

    CommonData.POP3Port = defaultPOP3Port;

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
                CommonData.POP3Host = argv[this_arg];
                hostFound = TRUE;
                argState = INSTALL_STATE_SERVER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strF       )
          || !_tcsicmp(argv[this_arg], strFrom    )
          || !_tcsicmp(argv[this_arg], strI       )
          || !_tcsicmp(argv[this_arg], strSender  )
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                CommonData.Sender = argv[this_arg];
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
                CommonData.Try = argv[this_arg];
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
                CommonData.POP3Port = argv[this_arg];
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
                    CommonData.Profile = argv[this_arg];          // Keep the profile if not "-"
                    checkProfileNameLength( CommonData );
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
                    CommonData.POP3Login = argv[this_arg];        // Keep the login name if not "-"
                }
                loginFound = TRUE;
                argState = INSTALL_STATE_LOGIN_ID+1;
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
          || !_tcsicmp(argv[this_arg], strPop3Pw      )
          || !_tcsicmp(argv[this_arg], strPop3Password)
  #endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the password.
                    CommonData.POP3Password = argv[this_arg];     // Keep the password if not "-"
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
                CommonData.POP3Host = argv[this_arg];
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
                CommonData.POP3Port = argv[this_arg];
                continue;
            }

        case INSTALL_STATE_PROFILE:
            argState++;
            if ( !profileFound ) {
                profileFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the profile.
                    CommonData.Profile = argv[this_arg];          // Keep the profile if not "-"
                    checkProfileNameLength( CommonData );
                }
                continue;
            }

        case INSTALL_STATE_LOGIN_ID:
            argState++;
            if ( !loginFound ) {
                loginFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the login name.
                    CommonData.POP3Login = argv[this_arg];        // Keep the login name if not "-"
                }
                continue;
            }

        case INSTALL_STATE_PASSWORD:
            argState++;
            if ( !pwdFound ) {
                pwdFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then delete the password.
                    CommonData.POP3Password = argv[this_arg];     // Keep the password if not "-"
                }
                continue;
            }

        case INSTALL_STATE_ANYTHING_MISSED:
            argState++;
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
        if ( !_tcscmp(CommonData.POP3Port.Get(), __T("-")) ) CommonData.POP3Port = defaultPOP3Port;
        if ( !_tcscmp(CommonData.POP3Port.Get(), __T("0")) ) CommonData.POP3Port = defaultPOP3Port;

  #if INCLUDE_NNTP
        CommonData.NNTPHost.Clear();
  #endif
        CommonData.SMTPHost.Clear();
        if ( CreateRegEntry( CommonData, useHKCU ) == 0 ) {
            printMsg( CommonData, __T("POP3 server set to %s on port %s\n"), CommonData.POP3Host.Get(), CommonData.POP3Port.Get() );
            CommonData.regerr = 0;
            printMsg( CommonData, NULL );
            if ( CommonData.logOut )
                if ( CommonData.logOut != stdout )
                    fclose( CommonData.logOut );

            CommonData.exitRequired = TRUE;
            return(0);
        }
    } else {
        printMsg( CommonData, __T("To set the POP3 server's address and the login name at that address do:\nblat -installPOP3 server - - - - loginname loginpwd\n"));
        return(-6);
    }

    return(startargv);
}

static int checkPOP3Srvr ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.POP3Host = argv[this_arg+1];
    return(1);
}

static int checkPOP3Port ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.POP3Port = argv[this_arg+1];
    return(1);
}
#endif

#if INCLUDE_IMAP

static int checkIMAPInstall ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
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

    CommonData.IMAPHost.Clear();
    CommonData.Profile.Clear();

    CommonData.Sender.Clear();

    CommonData.IMAPLogin.Clear();
    CommonData.IMAPPassword.Clear();

    CommonData.IMAPPort = defaultIMAPPort;

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
                CommonData.IMAPHost = argv[this_arg];
                hostFound = TRUE;
                argState = INSTALL_STATE_SERVER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_tcsicmp(argv[this_arg], strF       )
          || !_tcsicmp(argv[this_arg], strFrom    )
          || !_tcsicmp(argv[this_arg], strI       )
          || !_tcsicmp(argv[this_arg], strSender  )
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                CommonData.Sender = argv[this_arg];
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
                CommonData.Try = argv[this_arg];
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
          || !_tcsicmp(argv[this_arg], strPortIMAP)
  #endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                CommonData.IMAPPort = argv[this_arg];
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
                    CommonData.Profile = argv[this_arg];          // Keep the profile if not "-"
                    checkProfileNameLength( CommonData );
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
          || !_tcsicmp(argv[this_arg], strImapU       )
          || !_tcsicmp(argv[this_arg], strImapUsername)
  #endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the login name.
                    CommonData.IMAPLogin = argv[this_arg];        // Keep the login name if not "-"
                }
                loginFound = TRUE;
                argState = INSTALL_STATE_LOGIN_ID+1;
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
                    CommonData.IMAPPassword = argv[this_arg];     // Keep the password if not "-"
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
                CommonData.IMAPHost = argv[this_arg];
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
                CommonData.IMAPPort = argv[this_arg];
                continue;
            }

        case INSTALL_STATE_PROFILE:
            argState++;
            if ( !profileFound ) {
                profileFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the profile.
                    CommonData.Profile = argv[this_arg];          // Keep the profile if not "-"
                    checkProfileNameLength( CommonData );
                }
                continue;
            }

        case INSTALL_STATE_LOGIN_ID:
            argState++;
            if ( !loginFound ) {
                loginFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then skip the login name.
                    CommonData.IMAPLogin = argv[this_arg];        // Keep the login name if not "-"
                }
                continue;
            }

        case INSTALL_STATE_PASSWORD:
            argState++;
            if ( !pwdFound ) {
                pwdFound = TRUE;
                if ( _tcscmp(argv[this_arg], __T("-")) ) {        // If "-" found, then delete the password.
                    CommonData.IMAPPassword = argv[this_arg];     // Keep the password if not "-"
                }
                continue;
            }

        case INSTALL_STATE_ANYTHING_MISSED:
            argState++;
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
        if ( !_tcscmp(CommonData.IMAPPort.Get(), __T("-")) ) CommonData.IMAPPort = defaultIMAPPort;
        if ( !_tcscmp(CommonData.IMAPPort.Get(), __T("0")) ) CommonData.IMAPPort = defaultIMAPPort;

  #if INCLUDE_NNTP
        CommonData.NNTPHost.Clear();
  #endif
        CommonData.SMTPHost.Clear();
        if ( CreateRegEntry( CommonData, useHKCU ) == 0 ) {
            printMsg( CommonData, __T("IMAP server set to %s on port %s\n"), CommonData.IMAPHost.Get(), CommonData.IMAPPort.Get() );
            CommonData.regerr = 0;
            printMsg( CommonData, NULL );
            if ( CommonData.logOut )
                if ( CommonData.logOut != stdout )
                    fclose( CommonData.logOut );

            CommonData.exitRequired = TRUE;
            return(0);
        }
    } else {
        printMsg( CommonData, __T("To set the IMAP server's address and the login name at that address do:\nblat -installIMAP server - - - - loginname loginpwd\n"));
        return(-6);
    }

    return(startargv);
}

static int checkIMAPSrvr ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.IMAPHost = argv[this_arg+1];
    return(1);
}

static int checkIMAPPort ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.IMAPPort = argv[this_arg+1];
    return(1);
}
#endif

#if BLAT_LITE
#else

static int checkOptionFile ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    if ( !CommonData.optionsFile.Get()[0] ) {
        CommonData.optionsFile = argv[this_arg+1];
    }
    return(1);
}
#endif

static int checkToOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    if ( CommonData.destination.Length() ) {
        printMsg(CommonData, __T("Warning: -t/-to is being used with -tf, or another -t/to.\n\t Previous values will be ignored.\n"));
        CommonData.destination.Free();
    }

    CommonData.destination = argv[this_arg+1];

    return(1);
}

#if BLAT_LITE
#else

static int checkToFileOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int ret;

    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    if ( CommonData.destination.Length() ) {
        printMsg(CommonData, __T("Warning: -tf is being used with  -t/-to, or another -tf.\n\t Previous values will be ignored.\n"));
        CommonData.destination.Free();
    }

    ret = ReadNamesFromFile(CommonData, __T("TO"), argv[this_arg+1], CommonData.destination);
    if ( ret )
        return(0 - ret);

    return(1);
}
#endif

static int checkCcOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    if ( CommonData.cc_list.Length() ) {
        printMsg(CommonData, __T("Warning: -c/-cc is being used with  -cf, or another -c/-cc.\n\t Previous values will be ignored.\n"));
        CommonData.cc_list.Free();
    }
    CommonData.cc_list = argv[this_arg+1];

    return(1);
}

#if BLAT_LITE
#else

static int checkCcFileOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int ret;

    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    if ( CommonData.cc_list.Length() ) {
        printMsg(CommonData, __T("Warning: -cf is being used with  -c/-cc, or another -cf.\n\t Previous values will be ignored.\n"));
        CommonData.cc_list.Free();
    }

    ret = ReadNamesFromFile(CommonData, __T("CC"), argv[this_arg+1], CommonData.cc_list);
    if ( ret )
        return(0 - ret);

    return(1);
}
#endif

static int checkBccOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    if ( CommonData.bcc_list.Length() ) {
        printMsg(CommonData, __T("Warning: -b/-bcc is being used with -bf, or another -b/-bcc.\n\t Previous values will be ignored.\n"));
        CommonData.bcc_list.Free();
    }
    CommonData.bcc_list = argv[this_arg+1];

    return(1);
}

#if BLAT_LITE
#else

static int checkBccFileOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int ret;

    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    if ( CommonData.bcc_list.Length() ) {
        printMsg(CommonData, __T("Warning: -bf is being used with  -b/-bcc, or another -bf.\n\t Previous values will be ignored.\n"));
        CommonData.bcc_list.Free();
    }

    ret = ReadNamesFromFile(CommonData, __T("BCC"), argv[this_arg+1], CommonData.bcc_list);
    if ( ret )
        return(0 - ret);

    return(1);
}
#endif

static int checkSubjectOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.subject.Free();
    if ( argv[this_arg+1][0] != __T('\0') ) {
        CommonData.subject.Add( argv[this_arg+1] );
#if defined(_UNICODE) || defined(UNICODE)
        checkInputForUnicode( CommonData, CommonData.subject );
#endif
    }
    return(1);
}

#if BLAT_LITE
#else

static int checkSubjectFile ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    WinFile fileh;
    DWORD   filesize;
    DWORD   dummy;
    DWORD   x;
    LPTSTR  pString;
    Buf     tmpSubject;

    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.subject.Free();
    if ( fileh.OpenThisFile(argv[this_arg+1])) {
        filesize = fileh.GetSize();
        tmpSubject.Alloc( filesize + 1 );

        if ( fileh.ReadThisFile(tmpSubject.Get(), filesize, &dummy, NULL) ) {
            fileh.Close();
            tmpSubject.SetLength( filesize );
          #if defined(_UNICODE) || defined(UNICODE)
            Buf sourceText;

            sourceText.Clear();
            sourceText.Add( tmpSubject );
            checkInputForUnicode( CommonData, sourceText );
            tmpSubject = sourceText;
            tmpSubject.Add( __T('\0') );
          #endif
            if ( tmpSubject.Length() > SUBJECT_SIZE ) {
                tmpSubject.Get()[SUBJECT_SIZE] = __T('\0');
                tmpSubject.SetLength();
            }
            pString = tmpSubject.Get();
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
            tmpSubject.SetLength();
            CommonData.subject = tmpSubject;
        } else {
            CommonData.subject.Add( argv[this_arg+1] );
            fileh.Close();
            tmpSubject.Free();
        }
    } else {
        CommonData.subject.Add( argv[this_arg+1] );
    }

    return(1);
}
#endif

static int checkSkipSubject ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.ssubject = TRUE;                /*$$ASD*/

    return(0);
}

static int checkBodyText ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    Buf tempBodyParameter;

    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    tempBodyParameter = argv[this_arg+1];
#if defined(_UNICODE) || defined(UNICODE)
    checkInputForUnicode( CommonData, tempBodyParameter );
#endif
    CommonData.bodyparameter.Add( tempBodyParameter );
    CommonData.bodyFilename = __T("-");

    return(1);
}

static int checkBodyFile ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    if ( !argv[this_arg+1] )
        return(-1);

    CommonData.bodyFilename = argv[this_arg+1];
    return(1);
}

static int checkProfileEdit ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int useHKCU;

    if ( !startargv )   // -profile is not allowed in an option file.
        return(0);

    useHKCU = FALSE;
    this_arg++;

#if BLAT_LITE
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;

    ShowRegHelp( CommonData );
    ListProfiles(CommonData, __T("<all>"));
#else
/*    if ( argc == 0 ) {
        ListProfiles(CommonData, __T("<all>"));
    } else
 */
    if ( !argv[this_arg] ) {    // argc = 0
        ListProfiles(CommonData, __T("<all>"));
    } else {
        if ( !_tcscmp(argv[this_arg], __T("-delete")) ) {
            this_arg++;
            if ( argc )
                argc--;

            for ( ; argc; argc-- ) {
                Buf profileKey;
                if ( !_tcscmp( argv[this_arg], __T("-hkcu") ) ) {
                    useHKCU = TRUE;
                    this_arg++;
                    continue;
                }
                profileKey = argv[this_arg];
                DeleteRegEntry( CommonData, useHKCU ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE, NULL, profileKey );
                profileKey.Free();
                this_arg++;
            }
        } else {
            if ( !_tcscmp(argv[this_arg], __T("-h")) ) {
                if ( argc )
                    argc--;

                this_arg++;
                ShowRegHelp( CommonData );
            }
            if ( argc == 0 ) {
                ListProfiles(CommonData, __T("<all>"));
            } else {
                bool listAll;

                listAll = TRUE;
                for ( ; argc; argc-- ) {
                    if ( !_tcscmp( argv[this_arg], __T("-hkcu") ) ) {
                        this_arg++; // ignore this option if found
                        continue;
                    }

                    ListProfiles(CommonData, argv[this_arg]);
                    this_arg++;
                    listAll = FALSE;
                }
                if ( listAll )
                    ListProfiles(CommonData, __T("<all>"));
            }
        }
    }
#endif
    printMsg( CommonData, NULL );
    if ( CommonData.logOut )
        if ( CommonData.logOut != stdout )
            fclose( CommonData.logOut );

    CommonData.exitRequired = TRUE;
    return(0);
}

#if BLAT_LITE
#else

static int checkProfileToUse ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.Profile = argv[this_arg+1];
    checkProfileNameLength( CommonData );
    return(1);
}
#endif

static int checkServerOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.SMTPHost = argv[this_arg+1];
    return(1);
}

static int checkFromOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.loginname = argv[this_arg+1];
    return(1);
}

static int checkImpersonate ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.senderid = argv[this_arg+1];
    CommonData.impersonating = TRUE;

    return(1);
}

static int checkPortOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.SMTPPort = argv[this_arg+1];
    return(1);
}

static int checkUserIDOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.AUTHLogin = argv[this_arg+1];
    return(1);
}

static int checkPwdOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.AUTHPassword = argv[this_arg+1];

    return(1);
}

#if INCLUDE_POP3
static int checkPop3UIDOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.POP3Login = argv[this_arg+1];

    return(1);
}

static int checkPop3PwdOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.POP3Password = argv[this_arg+1];

    return(1);
}
#endif

#if INCLUDE_IMAP
static int checkImapUIDOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.IMAPLogin = argv[this_arg+1];

    return(1);
}

static int checkImapPwdOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.IMAPPassword = argv[this_arg+1];

    return(1);
}
#endif

#if SUPPORT_GSSAPI
static int checkGssapiMutual ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

#if SUBMISSION_PORT
    _tcsncpy(CommonData.SMTPPort, SubmissionPort, SERVER_SIZE);
    CommonData.SMTPPort[SERVER_SIZE] = __T('\0');
#endif
    CommonData.authgssapi = TRUE;
    CommonData.mutualauth = TRUE;
    return(0);
}

static int checkGssapiClient ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

#if SUBMISSION_PORT
    _tcsncpy(CommonData.SMTPPort, SubmissionPort, SERVER_SIZE );
    CommonData.SMTPPort[SERVER_SIZE] = __T('\0');
#endif
    CommonData.authgssapi = TRUE;
    CommonData.mutualauth = FALSE;
    return(0);
}

static int checkServiceName ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.servicename = argv[this_arg+1];
    return(1);
}

static int checkProtectionLevel ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    switch (argv[this_arg+1][0]) {
    case __T('N'):
    case __T('n'):
    case __T('0'):
        CommonData.gss_protection_level = GSSAUTH_P_NONE;
        break;
    case __T('I'):
    case __T('i'):
    case __T('1'):
        CommonData.gss_protection_level = GSSAUTH_P_INTEGRITY;
        break;
    default:
        CommonData.gss_protection_level = GSSAUTH_P_PRIVACY;
    }
    return(1);
}
#endif

#if BLAT_LITE
#else

static int checkBypassCramMD5 ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.ByPassCRAM_MD5 = TRUE;
    return(0);
}

static int checkOrgOption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.organization.Add( argv[this_arg+1] );

    return(1);
}

static int checkXHeaders ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    // is argv[] "-x"? If so, argv[3] is an X-Header
    // Header MUST start with X-
    if ( (argv[this_arg+1][0] == __T('X')) && (argv[this_arg+1][1] == __T('-')) ) {
        if ( CommonData.xheaders.Length() )
            CommonData.xheaders.Add( __T("\r\n") );

        CommonData.xheaders.Add( argv[this_arg+1] );
    }

    return(1);
}

static int checkDisposition ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.disposition = TRUE;

    return(0);
}

static int checkReadReceipt ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.returnreceipt = TRUE;

    return(0);
}

static int checkNoHeaders ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.noheader = 1;

    return(0);
}

static int checkNoHeaders2 ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.noheader = 2;

    return(0);
}
#endif

static int checkPriority ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    // Toby Korn 8/4/1999
    // Check for priority. 0 is Low, 1 is High - you don't need
    // normal, since omission of a priority is normal.

    CommonData.priority = argv[this_arg+1][0];

    return(1);
}

static int checkSensitivity ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    // Check for sensitivity. 0 is personal, 1 is private, 2 is company-confidential
    // normal, since omission of a sensitivity is normal.

    CommonData.sensitivity = argv[this_arg+1][0];

    return(1);
}

#if BLAT_LITE
#else
static int checkMDN ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int ret;

    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    // Check for message disposition notification type.

    ret = 0;
    if ( 0 == _tcscmp( argv[this_arg+1], __T("displayed") ) ) {
        CommonData.mdn_type = MDN_DISPLAYED;
        ret = 1;
    } else
    if ( 0 == _tcscmp( argv[this_arg+1], __T("dispatched") ) ) {
        CommonData.mdn_type = MDN_DISPATCHED;
        ret = 1;
    } else
    if ( 0 == _tcscmp( argv[this_arg+1], __T("processed") ) ) {
        CommonData.mdn_type = MDN_PROCESSED;
        ret = 1;
    } else
    if ( 0 == _tcscmp( argv[this_arg+1], __T("deleted") ) ) {
        CommonData.mdn_type = MDN_DELETED;
        ret = 1;
    } else
    if ( 0 == _tcscmp( argv[this_arg+1], __T("denied") ) ) {
        CommonData.mdn_type = MDN_DENIED;
        ret = 1;
    } else
    if ( 0 == _tcscmp( argv[this_arg+1], __T("failed") ) ) {
        CommonData.mdn_type = MDN_FAILED;
        ret = 1;
    }

    return ret;
}
#endif

static int checkCharset ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.charset = argv[this_arg+1];
    _tcsupr( CommonData.charset.Get() );
    if ( _tcscmp( CommonData.charset.Get(), __T("UTF-8") ) == 0 ) {
        CommonData.utf = UTF_REQUESTED;
#if BLAT_LITE
        CommonData.mime = 1;
#else
        CommonData.eightBitMimeRequested = TRUE;
#endif
    } else
    if ( _tcscmp( CommonData.charset.Get(), __T("UTF-7") ) == 0 ) {
        CommonData.utf = UTF_REQUESTED;
#if BLAT_LITE
        CommonData.mime = 1;
#endif
    }
    return(1);
}

#if BLAT_LITE
#else

static int checkDeliveryStat ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    _tcslwr( argv[this_arg+1] );
    if ( _tcschr(argv[this_arg+1], __T('n')) )
        CommonData.deliveryStatusRequested = DSN_NEVER;
    else {
        if ( _tcschr(argv[this_arg+1], __T('s')) )
            CommonData.deliveryStatusRequested |= DSN_SUCCESS;

        if ( _tcschr(argv[this_arg+1], __T('f')) )
            CommonData.deliveryStatusRequested |= DSN_FAILURE;

        if ( _tcschr(argv[this_arg+1], __T('d')) )
            CommonData.deliveryStatusRequested |= DSN_DELAYED;
    }

    return(1);
}

static int checkHdrEncB ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.forcedHeaderEncoding = __T('b');

    return(0);
}

static int checkHdrEncQ ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.forcedHeaderEncoding = __T('q');

    return(0);
}
#endif

#if SUPPORT_YENC
static int check_yEnc ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.mime     = 0;
    CommonData.base64   = FALSE;
    CommonData.uuencode = FALSE;
    CommonData.yEnc     = TRUE;

    return(0);
}
#endif

static int checkMime ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.mime     = 1;
#if BLAT_LITE
#else
    CommonData.base64   = FALSE;
    CommonData.uuencode = FALSE;
    CommonData.yEnc     = FALSE;
#endif

    return(0);
}

#if BLAT_LITE
#else

static int checkUUEncode ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    if ( !CommonData.haveEmbedded ) {
        CommonData.mime     = 0;
        CommonData.base64   = FALSE;
        CommonData.uuencode = TRUE;
        CommonData.yEnc     = FALSE;
    }

    return(0);
}

static int checkLongUUEncode ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    CommonData.uuencodeBytesLine = 63;     // must be a multiple of three and less than 64.

    return checkUUEncode( CommonData, argc, argv, this_arg, startargv );
}

static int checkBase64Enc ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    if ( !CommonData.attach ) {
        CommonData.mime     = 0;
        CommonData.base64   = TRUE;
        CommonData.uuencode = FALSE;
        CommonData.yEnc     = FALSE;
    }

    return(0);
}

static int checkEnriched ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    CommonData.textmode = __T("enriched");

    return checkMime( CommonData, argc, argv, this_arg, startargv );
}
#endif

static int checkHTML ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    CommonData.textmode = __T("html");

    return checkMime( CommonData, argc, argv, this_arg, startargv );
}

#if BLAT_LITE
#else

static int checkUnicode ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.utf = UTF_REQUESTED;
    return 0;
}
#endif

static int addToAttachments ( COMMON_DATA & CommonData, LPTSTR * argv, int this_arg, _TCHAR aType )
{
    Buf    tmpstr;
    LPTSTR srcptr;
    int    i;

    parseCommaDelimitString( CommonData, argv[this_arg+1], tmpstr, TRUE );
    srcptr = tmpstr.Get();
    if ( srcptr ) {
        for ( ; *srcptr; ) {

            if ( CommonData.attach == 64 ) {
                printMsg(CommonData, __T("Max of 64 files allowed!  Others are being ignored.\n"));
                break;
            }

            for ( i = 0; i < CommonData.attach; i++ ) {
                if ( aType < CommonData.attachtype[i] )
                    break;
            }

            if ( i == CommonData.attach ) {
                CommonData.attachtype[CommonData.attach] = aType;
                CommonData.attachfile[CommonData.attach] = srcptr;
                CommonData.attach++;
            } else {
                for ( int j = 63; j > i; j-- ) {
                    CommonData.attachtype[j] = CommonData.attachtype[j-1];
                    CommonData.attachfile[j] = CommonData.attachfile[j-1];
                }
                CommonData.attachtype[i] = aType;
                CommonData.attachfile[i] = srcptr;
                CommonData.attach++;
            }

            srcptr += _tcslen(srcptr) + 1;
#if BLAT_LITE
#else
            CommonData.base64 = FALSE;
#endif
        }

        tmpstr.Free();
    }

    return(1);
}

static int checkMissingAttach ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.ignoreMissingAttachmentFiles = TRUE;
    return(0);
}

static int checkInlineAttach ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    // -attachI added to v2.4.1
    CommonData.haveAttachments = TRUE;

    return addToAttachments( CommonData, argv, this_arg, INLINE_ATTACHMENT );
}

static int checkTxtFileAttach ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    // -attacht added 98.4.16 v1.7.2 tcharron@interlog.com
    CommonData.haveAttachments = TRUE;

    return addToAttachments( CommonData, argv, this_arg, TEXT_ATTACHMENT );
}

static int checkBinFileAttach ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    // -attach added 98.2.24 v1.6.3 tcharron@interlog.com
    CommonData.haveAttachments = TRUE;

    return addToAttachments( CommonData, argv, this_arg, BINARY_ATTACHMENT );
}

#if BLAT_LITE
#else

static int checkBinFileEmbed ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.haveEmbedded = TRUE;
    checkMime( CommonData, argc, argv, this_arg, startargv );   // Force MIME encoding.

    return addToAttachments( CommonData, argv, this_arg, EMBED_ATTACHMENT );
}

static int ReadFilenamesFromFile(COMMON_DATA & CommonData, LPTSTR namesfilename, _TCHAR aType) {
    WinFile fileh;
    DWORD   filesize;
    DWORD   dummy;
    Buf     tmpstr;
    LPTSTR  pString;

    if ( !fileh.OpenThisFile(namesfilename)) {
        printMsg(CommonData, __T("error opening %s, aborting\n"), namesfilename);
        return(-3);
    }
    filesize = fileh.GetSize();
    tmpstr.Alloc( filesize+1 );
    //if ( !tmpstr ) {
    //    fileh.Close();
    //    printMsg(CommonData, __T("error allocating memory for reading %s, aborting\n"), namesfilename);
    //    return(-5);
    //}

    if ( !fileh.ReadThisFile(tmpstr.Get(), filesize, &dummy, NULL) ) {
        fileh.Close();
        tmpstr.Free();
        printMsg(CommonData, __T("error reading %s, aborting\n"), namesfilename);
        return(-5);
    }
    fileh.Close();
    tmpstr.SetLength( filesize );
    tmpstr.Add( __T('\0') );
  #if defined(_UNICODE) || defined(UNICODE)
    Buf sourceText;

    sourceText.Clear();
    sourceText.Add( tmpstr.Get(), filesize );
    checkInputForUnicode( CommonData, sourceText );
    tmpstr = sourceText;
    if ( tmpstr.Get()[0] == 0xFEFF )            // Unicode BOM ?
        tmpstr.Remove(0);
  #endif
    tmpstr.Add( __T('\0') );
    pString = tmpstr.Get();
    addToAttachments( CommonData, &pString, -1, aType );
    tmpstr.Free();
    return(1);                                  // indicates no error.
}

static int checkTxtFileAttFil ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int retVal;

    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    retVal = ReadFilenamesFromFile(CommonData, argv[this_arg+1], TEXT_ATTACHMENT );

    if ( retVal == 1 )
        CommonData.haveAttachments = TRUE;

    return retVal;
}

static int checkBinFileAttFil ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int retVal;

    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    retVal = ReadFilenamesFromFile(CommonData, argv[this_arg+1], BINARY_ATTACHMENT );

    if ( retVal == 1 )
        CommonData.haveAttachments = TRUE;

    return retVal;
}

static int checkEmbFileAttFil ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    int retVal;

    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    checkMime( CommonData, argc, argv, this_arg, startargv );   // Force MIME encoding.

    retVal = ReadFilenamesFromFile(CommonData, argv[this_arg+1], EMBED_ATTACHMENT );

    if ( retVal == 1 )
        CommonData.haveEmbedded = TRUE;

    return retVal;
}
#endif

static int checkHelp ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    printUsage( CommonData, TRUE );

    return(-1);
}

static int checkQuietMode ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.quiet = TRUE;

    return(0);
}

static int checkDebugMode ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.debug = TRUE;

    return(0);
}

static int checkLogCommands ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.logCommands = TRUE;

    return(0);
}

static int checkLogMessages ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    if ( CommonData.logOut ) {
        if ( CommonData.logOut != stdout ) {
            fclose( CommonData.logOut );
            CommonData.logOut = NULL;
        }
    }
    if ( !argv[this_arg+1]                  ||
         (argv[this_arg+1][0] == __T('\0')) ||
         (argv[this_arg+1][0] == __T('-') ) ||
         (argv[this_arg+1][0] == __T('/') ) ) {
        CommonData.logFile.Clear();
        CommonData.logOut = stdout;
        return(0);
    }

    CommonData.logFile = argv[this_arg+1];

    if ( CommonData.logFile.Get()[0] ) {
        if ( CommonData.clearLogFirst ) {
            CommonData.logOut = _tfopen(CommonData.logFile.Get(), fileCreateAttribute);
#if (defined(_UNICODE) || defined(UNICODE)) && defined(_MSC_VER) && (_MSC_VER < 1400)
            if ( CommonData.logOut )
                _ftprintf( CommonData.logOut, __T("%s"), utf8BOM );
#endif
        } else
            CommonData.logOut = _tfopen(CommonData.logFile.Get(), fileAppendAttribute);
    }

    // if all goes well the file is closed normally
    // if we return before closing the library SHOULD
    // close it for us..

    return(1);
}

static int checkLogOverwrite ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.clearLogFirst = TRUE;
    if ( CommonData.logOut ) {
        fclose( CommonData.logOut );
        CommonData.logOut = _tfopen(CommonData.logFile.Get(), fileCreateAttribute);
#if (defined(_UNICODE) || defined(UNICODE)) && defined(_MSC_VER) && (_MSC_VER < 1400)
        if ( CommonData.logOut )
            _ftprintf( CommonData.logOut, __T("%s"), utf8BOM );
#endif
    }

    return(0);
}

static int checkTimestamp ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.timestamp = TRUE;

    return(0);
}

static int checkTimeout ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.globaltimeout = _tstoi(argv[this_arg+1]);

    return(1);
}

static int checkAttempts ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.Try = argv[this_arg+1];
    return(1);
}

static int checkFixPipe ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.bodyconvert = FALSE;

    return(0);
}

static int checkHostname ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.my_hostname_wanted = argv[this_arg+1];
    return(1);
}

static int checkMailFrom ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.loginname = argv[this_arg+1];
    return(1);
}

static int checkWhoFrom ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.fromid = argv[this_arg+1];
    return(1);
}

static int checkReplyTo ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.replytoid = argv[this_arg+1];
    return(1);
}

static int checkReturnPath ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.returnpathid = argv[this_arg+1];
    return(1);
}

static int checkSender ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.sendername = argv[this_arg+1];
    return(1);
}

static int checkRaw ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.formattedContent = FALSE;
    return(0);
}

#if BLAT_LITE
#else

static int checkA1Headers ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    unsigned index;
    unsigned needCap;
    LPTSTR   pString;

    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.aheaders1.Add( argv[this_arg+1] );
    pString = CommonData.aheaders1.Get();
    if ( pString ) {
        needCap = TRUE;
        for ( index = 0; pString[index]; index++ ) {
            if ( (pString[index] == __T(':')) ||
                 (pString[index] == __T(' ')) )
                break;

            if ( isalpha(pString[index]) ) {
                if ( needCap ) {
                    pString[index] = (TCHAR)toupper(pString[index]);
                    needCap = FALSE;
                } else {
                    pString[index] = (TCHAR)tolower(pString[index]);
                }
            } else {
                needCap = TRUE;
            }
        }
    }
    return(1);
}

static int checkA2Headers ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    unsigned index;
    unsigned needCap;
    LPTSTR   pString;

    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.aheaders2.Add( argv[this_arg+1] );
    pString = CommonData.aheaders2.Get();
    if ( pString ) {
        needCap = TRUE;
        for ( index = 0; pString[index]; index++ ) {
            if ( (pString[index] == __T(':')) ||
                 (pString[index] == __T(' ')) )
                break;

            if ( isalpha(pString[index]) ) {
                if ( needCap ) {
                    pString[index] = (TCHAR)toupper(pString[index]);
                    needCap = FALSE;
                } else {
                    pString[index] = (TCHAR)tolower(pString[index]);
                }
            } else {
                needCap = TRUE;
            }
        }
    }
    return(1);
}

static int check8bitMime ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    CommonData.eightBitMimeRequested = TRUE;

    return checkMime( CommonData, argc, argv, this_arg, startargv );
}

static int checkForce8bit ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;   // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.force8BitMime = TRUE;
    return(0);
}

static int checkAltTextFile ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    Buf     tmpstr;
    LPTSTR  srcptr;
    _TCHAR   alternateTextFile[_MAX_PATH];
    WinFile atf;
    DWORD   fileSize;
    DWORD   dummy;

    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    parseCommaDelimitString( CommonData, argv[this_arg+1], tmpstr, TRUE );
    srcptr = tmpstr.Get();
    if ( srcptr ) {
        _tcsncpy( alternateTextFile, srcptr, _MAX_PATH-1 );  // Copy only the first filename.
        alternateTextFile[_MAX_PATH-1] = __T('\0');
    }
    tmpstr.Free();

    if ( !atf.OpenThisFile(alternateTextFile) ) {
        printMsg( CommonData, __T("Error opening %s, Alternate Text will not be added.\n"), alternateTextFile );
    } else {
        fileSize = atf.GetSize();
        CommonData.alternateText.Clear();

        if ( fileSize ) {
            CommonData.alternateText.Alloc( fileSize + 1 );
            CommonData.alternateText.SetLength( fileSize );
            if ( !atf.ReadThisFile(CommonData.alternateText.Get(), fileSize, &dummy, NULL) ) {
                printMsg( CommonData, __T("Error reading %s, Alternate Text will not be added.\n"), alternateTextFile );
            } else {
                *CommonData.alternateText.GetTail() = __T('\0');
  #if defined(_UNICODE) || defined(UNICODE)
                checkInputForUnicode( CommonData, CommonData.alternateText );
  #endif
            }
        }
        atf.Close();
    }
    return(1);
}

static int checkAltText ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    CommonData.alternateText = argv[this_arg+1];
    return(1);
}
#endif
#if SUPPORT_SIGNATURES

static int checkSigFile ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    WinFile fileh;
    DWORD   sigsize;
    DWORD   dummy;

    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    this_arg++;
    if ( !fileh.OpenThisFile(argv[this_arg]) ) {
        printMsg( CommonData, __T("Error opening signature file %s, signature will not be added\n"), argv[this_arg] );
        return(1);
    }

    sigsize = fileh.GetSize();
    CommonData.signature.Alloc( sigsize + 1 );
    CommonData.signature.SetLength( sigsize );

    if ( !fileh.ReadThisFile(CommonData.signature.Get(), sigsize, &dummy, NULL) ) {
        printMsg( CommonData, __T("Error reading signature file %s, signature will not be added\n"), argv[this_arg] );
        CommonData.signature.Free();
    } else {
        *CommonData.signature.GetTail() = __T('\0');
  #if defined(_UNICODE) || defined(UNICODE)
        checkInputForUnicode( CommonData, CommonData.signature );
  #endif
    }
    fileh.Close();
    return(1);
}
#endif
#if SUPPORT_TAGLINES

static int checkTagFile ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    WinFile fileh;
    DWORD   tagsize;
    DWORD   count;
    DWORD   selectedTag;
    LPTSTR  p;
    Buf     tmpBuf;

    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    this_arg++;
    if ( !fileh.OpenThisFile(argv[this_arg]) ) {
        printMsg( CommonData, __T("Error opening tagline file %s, a tagline will not be added.\n"), argv[this_arg] );
        return(1);
    }

    tagsize = fileh.GetSize();
    if ( tagsize ) {
        tmpBuf.Alloc( tagsize + 1 );
        tmpBuf.SetLength( tagsize );

        if ( !fileh.ReadThisFile(tmpBuf.Get(), tagsize, &count, NULL) ) {
            printMsg( CommonData, __T("Error reading tagline file %s, a tagline will not be added.\n"), argv[this_arg] );
        }

        *tmpBuf.GetTail() = __T('\0');
  #if defined(_UNICODE) || defined(UNICODE)
        checkInputForUnicode( CommonData, tmpBuf );
  #endif
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

            CommonData.tagline = p;
        } else
            CommonData.tagline.Add( tmpBuf );
    }
    fileh.Close();

    return(1);
}
#endif
#if SUPPORT_POSTSCRIPTS
static int checkPostscript ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    WinFile fileh;
    DWORD   sigsize;
    DWORD   dummy;

    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    this_arg++;
    if ( !fileh.OpenThisFile(argv[this_arg]) ) {
        printMsg( CommonData, __T("Error opening P.S. file %s, postscript will not be added\n"), argv[this_arg] );
        return(1);
    }

    sigsize = fileh.GetSize();
    CommonData.postscript.Alloc( sigsize + 1 );
    CommonData.postscript.SetLength( sigsize );

    if ( !fileh.ReadThisFile(CommonData.postscript.Get(), sigsize, &dummy, NULL) ) {
        printMsg( CommonData, __T("Error reading P.S. file %s, postscript will not be added\n"), argv[this_arg] );
        CommonData.postscript.Free();
    } else {
        *CommonData.postscript.GetTail() = __T('\0');
  #if defined(_UNICODE) || defined(UNICODE)
        checkInputForUnicode( CommonData, CommonData.postscript );
  #endif
    }
    fileh.Close();
    return(1);
}
#endif

static int checkUserAgent ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.includeUserAgent = TRUE;

    return(0);
}


#if SUPPORT_MULTIPART

// The MultiPart message size value is per 1000 bytes.  e.g. 900 would be 900,000 bytes.

static int checkMultiPartSize ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    if ( argc ) {
        long mps = _tstol( argv[this_arg+1] );
        if ( mps > 0 )
            CommonData.multipartSize = (DWORD)mps; // If -mps is used with an invalid size, ignore it.

        if ( CommonData.multipartSize < 50 )   // Minimum size per message is 50,000 bytes.
            CommonData.multipartSize = 0;

        if ( CommonData.multipartSize <= ((DWORD)(-1)/1000) )
            CommonData.multipartSize *= 1000;

        return(1);
    }

    CommonData.multipartSize = (DWORD)(-1); // If -mps specified without a size, then force mps to use
                                            // the CommonData.SMTP server's max size to split messages.  This has
                                            // no effect on NNTP because there is no size information.
    return(0);
}


static int checkDisableMPS ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.disableMPS = TRUE;

    return(0);
}
#endif


static int checkUnDisROption ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)this_arg;
    (void)startargv;

    CommonData.sendUndisclosed = TRUE;

    return(0);
}

#if BLAT_LITE
#else
static int checkUserContType ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)argv;
    (void)startargv;

    CommonData.userContentType = argv[this_arg+1];
    return(1);
}


static int checkMaxNames ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    int maxn = _tstoi( argv[this_arg+1] );

    if ( maxn > 0 )
        CommonData.maxNames = maxn;

    return(1);
}

static int checkCommentChar ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    if ( _tcslen( argv[++this_arg] ) == 1 ) {
        if ( argv[this_arg][0] != __T('-') )
            CommonData.commentChar = argv[this_arg][0];

        return(1);
    }

    return(0);
}

static int checkDelayTime ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    (void)argc;         // For eliminating compiler warnings.
    (void)startargv;

    int timeDelay = _tstoi( argv[this_arg+1] );

    if ( timeDelay > 0 )
        CommonData.delayBetweenMsgs = timeDelay;

    return(1);
}
#endif

#if INCLUDE_SUPERDEBUG
static int checkSuperDebug ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    CommonData.superDebug = 1;
    return checkDebugMode( CommonData, argc, argv, this_arg, startargv );
}

static int checkSuperDebugT ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    CommonData.superDebug = 2;
    return checkDebugMode( CommonData, argc, argv, this_arg, startargv );
}

static int checkSuperDuperDebug ( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int this_arg, int startargv )
{
    CommonData.superDuperDebug = TRUE;
    return checkSuperDebug( CommonData, argc, argv, this_arg, startargv );
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
    { strImapU              ,              NULL      , FALSE, 1, checkImapUIDOption  ,    __T(" <username>  : username for IMAP LOGIN (use with -ipw)") },
    { strImapPassword       ,              NULL      , FALSE, 1, checkImapPwdOption  , NULL },
    { strImapPw             ,              NULL      , FALSE, 1, checkImapPwdOption  ,     __T(" <password> : password for IMAP LOGIN (use with -iu)") },
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
#if BLAT_LITE
#else
    { __T("-mdn")           ,              NULL      , FALSE, 1, checkMDN            ,     __T(" <type>     : set Message Disposition Notification to <type> where type") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  can be displayed, dispatched, processed, deleted, denied, or") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  failed.  The message will be marked \"MDN-sent-automatically\"") },
#endif
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("----------------------- Attachment and encoding options -----------------------") },
    { __T("-attach")        , CGI_HIDDEN             , FALSE, 1, checkBinFileAttach  ,        __T(" <file>  : attach binary file(s) to message (filenames comma separated)") },
    { __T("-attacht")       , CGI_HIDDEN             , FALSE, 1, checkTxtFileAttach  ,         __T(" <file> : attach text file(s) to message (filenames comma separated)") },
    { __T("-attachi")       , CGI_HIDDEN             , FALSE, 1, checkInlineAttach   ,         __T(" <file> : attach text file(s) as INLINE (filenames comma separated)") },
    { __T("-imaf")          ,              NULL      , FALSE, 0, checkMissingAttach  ,      __T("           : ignore missing attachment files.  Do not stop for missing") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  files.") },
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
    { __T("-force8bit")     ,              NULL      , FALSE, 0, checkForce8bit      ,           __T("      : force Blat to believe the SMTP server supports 8-bit MIME") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  when the server does not say that it supports 8BITMIME") },
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
    { __T("-log")           ,              NULL      , TRUE , 0, checkLogMessages    ,     __T(" <file>     : log everything but usage to <file>") },
    { __T("-timestamp")     ,              NULL      , FALSE, 0, checkTimestamp      ,           __T("      : when -log is used, a timestamp is added to each log line") },
    { __T("-overwritelog")  ,              NULL      , TRUE , 0, checkLogOverwrite   ,              __T("   : when -log is used, overwrite the log file") },
    { __T("-logcmds")       ,              NULL      , TRUE , 0, checkLogCommands    ,         __T("        : when -log is used, write command line options to log file") },
    { __T("-ti")            , __T("timeout")         , FALSE, 1, checkTimeout        ,    __T(" <n>         : set timeout to 'n' seconds.  Blat will wait 'n' seconds for") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  server responses") },
    { strTry                ,              NULL      , FALSE, 1, checkAttempts       ,     __T(" <n times>  : how many times blat should try to send (1 to 'INFINITE')") },
    {                  NULL ,              NULL      , 0    , 0, NULL                , __T("                  The default is 1.)") },
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
    { __T("-superDuperDebug"),             NULL      , TRUE , 0, checkSuperDuperDebug,                 __T(": log many more debugging messages about Blat's function calls") },
#endif
    { __T("-showCommandLineArguments"),    NULL      , TRUE , 0, NULL                , NULL },

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

void printTitleLine( COMMON_DATA & CommonData, int quiet )
{
    _TCHAR tmpstr[1024];


    if ( !CommonData.titleLinePrinted ) {
        _stprintf( tmpstr, __T("Blat v%s%s (build : %s %s)\n") BIT_SIZE __T("-bit Windows, ") FEATURES_TEXT UNICODE_TEXT __T("\n\n"), blatVersion, blatVersionSuf, blatBuildDate, blatBuildTime );
        if ( !quiet )
            pMyPrintDLL( tmpstr );

        if ( CommonData.logOut )
            printMsg( CommonData, __T("%s"), tmpstr );
    }

    CommonData.titleLinePrinted = TRUE;
}

void print_usage_line( COMMON_DATA & CommonData, LPTSTR pUsageLine )
{
#if SUPPORT_GSSAPI
    LPTSTR match;
    _TCHAR beginning[USAGE_LINE_SIZE];

    //Make sure all lines about GSSAPI options contain "GSSAPI"
    if ( CommonData.bSuppressGssOptionsAtRuntime && _tcsstr(pUsageLine,__T("GSSAPI")) )
        return;

    if ( _tcsstr( pUsageLine, __T("GSSAPI") ) ) {
        if ( !CommonData.have_mechtype ) {
            try {
                if (!CommonData.pGss) {
                    CommonData.pGss = &CommonData.TheSession;
                }
                CommonData.pGss->MechtypeText(CommonData.mechtype.Get());
            }
            catch (GssSession::GssException &e) {
                printMsg(CommonData, __T("GssSession Error: %s\n"), e.message());
                CommonData.pGss = NULL;
                CommonData.mechtype = __T("UNAVAILABLE");
            }

            CommonData.have_mechtype = TRUE;
        }

        match = _tcsstr(pUsageLine,MECHTYPE);
        if ( match ) {
            memcpy( beginning, pUsageLine, (match - pUsageLine)*sizeof(_TCHAR) );
            beginning[match - pUsageLine] = __T('\0');

            pMyPrintDLL(beginning);
            pMyPrintDLL(CommonData.mechtype.Get());
            pMyPrintDLL(match + _tcslen(MECHTYPE));
            fflush(stdout);
            return;
        }
    }
#else
    CommonData = CommonData;
#endif
    pMyPrintDLL(pUsageLine);
    fflush(stdout);
}


int printUsage( COMMON_DATA & CommonData, int optionPtr )
{
    int  i;
    _TCHAR printLine[USAGE_LINE_SIZE];

    printTitleLine( CommonData, FALSE );

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
                print_usage_line( CommonData, printLine );
            }

            if ( blatOptionsList[++i].usageText ) {
                if ( (blatOptionsList[i].usageText[0] == __T('-')) && (blatOptionsList[i].usageText[1] == __T('-')) )
                    break;
            }
        }
    } else {
        CommonData.usagePrinted = TRUE;
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
                        print_usage_line( CommonData, printLine );
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
                print_usage_line( CommonData, printLine );

                if ( blatOptionsList[optionPtr+1].optionString && blatOptionsList[optionPtr+1].optionString[0] == __T('-') )
                    break;
            }
        }
    }

    return 1;
}


int processOptions( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int startargv, int preprocessing )
{
    FUNCTION_ENTRY();
    int i, this_arg, retval;

#if SUPPORT_GSSAPI
    // If the GSSAPI library is unavailable, make the GSSAPI options simply
    // unavailable at runtime.  It's less efficient to do it this way but
    // it doesn't intrude into the rest of the code below.
    if (CommonData.bSuppressGssOptionsAtRuntime) {
         for (i = 0; blatOptionsList[i].optionString; i++) {
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
    CommonData.processedOptions.Free();
    for ( this_arg = startargv; this_arg < argc; this_arg++ ) {
        if ( !argv[this_arg][0] )   // If we already dealt with a given option,
            continue;               // it will have been removed from the list.

        if ( (argv[this_arg][0] == __T('/'))
#if defined(_UNICODE) || defined(UNICODE)
          || (argv[this_arg][0] == 0x2013  )
#endif
           ) {
            argv[this_arg][0] = __T('-');
        }
    }
    for ( this_arg = startargv; this_arg < argc; this_arg++ ) {
        if ( !argv[this_arg][0] )   // If we already dealt with a given option,
            continue;               // it will have been removed from the list.

#if INCLUDE_SUPERDEBUG
        if ( CommonData.superDebug ) {
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

//#if INCLUDE_SUPERDEBUG
//            if ( CommonData.superDuperDebug == TRUE ) {
//                _TCHAR savedQuiet = CommonData.quiet;
//                CommonData.quiet = FALSE;
//                printMsg( CommonData, __T("superDuperDebug: comparing >>%s<< to >>%s<<\n"), blatOptionsList[i].optionString, argv[this_arg] );
//                CommonData.quiet = savedQuiet;
//            }
//#endif

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
                        if ( blatOptionsList[i].initFunction == NULL )
                            retval = 0;
                        else
                            retval = (*blatOptionsList[i].initFunction)(CommonData, next_arg - (this_arg + 1), argv, this_arg, startargv);

                        if ( CommonData.exitRequired || (retval < 0) ) {
                            CommonData.processedOptions.Free();
                            FUNCTION_EXIT();
                            if ( retval < 0 )
                                retval = 0 - retval;

                            return( retval );
                        }

                        CommonData.processedOptions.Add( __T("    ") );
                        CommonData.processedOptions.Add( argv[this_arg] );

                        // Remove the options we processed, so we do not attempt
                        // to process them a second time.
                        argv[this_arg][0] = __T('\0');
                        for ( ; retval; retval-- ) {
                            CommonData.processedOptions.Add( __T(' ') );
                            CommonData.processedOptions.Add( argv[++this_arg] );
                            argv[this_arg][0] = __T('\0');
                        }
                        CommonData.processedOptions.Add( __T("\r\n") );
                    } else
                        this_arg = next_arg - 1;
                } else {
                    if ( !preprocessing ) {
                        CommonData.processedOptions.Add( __T("    ") );
                        CommonData.processedOptions.Add( argv[this_arg] );
                        printMsg( CommonData, __T("Blat saw and processed these options, and %s the last one...\n%s\n\n"),
                                              __T("found fault with"), CommonData.processedOptions.Get() );
                        printMsg( CommonData, __T("Not enough arguments supplied for option: %s\n"),
                                              argv[this_arg] );

                        for ( ; ; ) {
                            if ( printUsage( CommonData, i ) )
                                break;

                            i++;    // Attempt to locate the main option name for the abbreviation we found.
                        }

                        CommonData.processedOptions.Free();
                        FUNCTION_EXIT();
                        return (1);
                    }
                }

                break;
            }
        }

        if ( !blatOptionsList[i].optionString && !preprocessing ) {
            CommonData.processedOptions.Add( __T("    ") );
            CommonData.processedOptions.Add( argv[this_arg] );
            printMsg( CommonData, __T("Blat saw and processed these options, and %s the last one...\n%s\n\n"),
                                  __T("was confused by"), CommonData.processedOptions.Get() );
            printMsg( CommonData, __T("Do not understand argument: %s\n"),
                                  argv[this_arg] );
            printUsage( CommonData, 0 );
            CommonData.processedOptions.Free();
            FUNCTION_EXIT();
            return(1);
        }
    }

    if ( !CommonData.logOut ) {
        if ( CommonData.debug ) {
            CommonData.quiet = FALSE;
        }
    }

    CommonData.processedOptions.Free();
    FUNCTION_EXIT();
    return(0);
}
