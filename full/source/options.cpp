/*
    options.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blat.h"
#include "winfile.h"
#include "gensock.h"

#ifdef __WATCOMC__
#define _strcmpi strcmpi
#endif

#if SUPPORT_GSSAPI
#include "gssfuncs.h" // Please read the comments here for information about how to use GssSession
  extern BOOL    authgssapi;
  extern BOOL    mutualauth;
  extern char    servicename[SERVICENAME_SIZE];
  extern protLev gss_protection_level;
  extern BOOL    bSuppressGssOptionsAtRuntime;
#endif

extern int  CreateRegEntry( int useHKCU );
extern int  DeleteRegEntry( char * pstrProfile, int useHKCU );
extern void ShowRegHelp( void );
extern void ListProfiles( char * pstrProfile );
extern void parseCommaDelimitString ( char * source, Buf & parsed_addys, int pathNames );

extern void printMsg(const char *p, ... );              // Added 23 Aug 2000 Craig Morrison

extern char         blatVersion[];
extern char         blatVersionSuf[];
extern char         blatBuildDate[];
extern char         blatBuildTime[];

extern char         Profile[TRY_SIZE+1];
extern char         priority[2];
extern char         sensitivity[2];

extern char         commentChar;
extern char         impersonating;
extern char         returnreceipt;
extern char         disposition;
extern char         ssubject;
extern char         includeUserAgent;
extern char         sendUndisclosed;
extern char         clearLogFirst;
static char         logFile[_MAX_PATH];

#if BLAT_LITE
#else
extern char         noheader;
extern unsigned int uuencodeBytesLine;
extern char         uuencode;
extern char         base64;
extern char         yEnc;
extern int          utf;
extern char         deliveryStatusRequested;
extern char         eightBitMimeRequested;
extern char         forcedHeaderEncoding;

extern char         organization[ORG_SIZE+1];
extern char         xheaders[DEST_SIZE+1];
extern char         aheaders1[DEST_SIZE+1];
extern char         aheaders2[DEST_SIZE+1];
extern char         optionsFile[_MAX_PATH];
extern Buf          userContentType;
extern char         ByPassCRAM_MD5;
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

extern char         SMTPHost[SERVER_SIZE+1];
extern char         SMTPPort[SERVER_SIZE+1];
extern const char * defaultSMTPPort;
#if SUPPORT_GSSAPI
  #if SUBMISSION_PORT
extern const char * SubmissionPort;
  #endif
#endif

#if INCLUDE_NNTP
extern char         NNTPHost[SERVER_SIZE+1];
extern char         NNTPPort[SERVER_SIZE+1];
extern const char * defaultNNTPPort;
extern Buf          groups;
#endif

#if INCLUDE_POP3
extern char         POP3Host[SERVER_SIZE+1];
extern char         POP3Port[SERVER_SIZE+1];
extern const char * defaultPOP3Port;
extern char         POP3Login[SERVER_SIZE+1];
extern char         POP3Password[SERVER_SIZE+1];
extern char         xtnd_xmit_wanted;
#endif
#if INCLUDE_IMAP
extern char         IMAPHost[SERVER_SIZE+1];
extern char         IMAPPort[SERVER_SIZE+1];
extern const char * defaultIMAPPort;
extern char         IMAPLogin[SERVER_SIZE+1];
extern char         IMAPPassword[SERVER_SIZE+1];
#endif

extern char         AUTHLogin[SERVER_SIZE+1];
extern char         AUTHPassword[SERVER_SIZE+1];
extern char         Try[TRY_SIZE+1];
extern char         Sender[SENDER_SIZE+1];
extern Buf          destination;
extern Buf          cc_list;
extern Buf          bcc_list;
extern char         loginname[SENDER_SIZE+1];       // RFC 821 MAIL From. <loginname>. There are some inconsistencies in usage
extern char         senderid[SENDER_SIZE+1];        // Inconsistent use in Blat for some RFC 822 Field definitions
extern char         sendername[SENDER_SIZE+1];      // RFC 822 Sender: <sendername>
extern char         fromid[SENDER_SIZE+1];          // RFC 822 From: <fromid>
extern char         replytoid[SENDER_SIZE+1];       // RFC 822 Reply-To: <replytoid>
extern char         returnpathid[SENDER_SIZE+1];    // RFC 822 Return-Path: <returnpath>
extern char         subject[SUBJECT_SIZE+1];
extern char         textmode[TEXTMODE_SIZE+1];      // added 15 June 1999 by James Greene "greene@gucc.org"
extern char         bodyFilename[_MAX_PATH];
extern Buf          bodyparameter;
extern char         formattedContent;
extern char         mime;
extern char         quiet;
extern char         debug;
extern int          attach;
extern int          regerr;
extern char         bodyconvert;
extern char         haveEmbedded;
extern char         haveAttachments;
extern int          maxNames;

static Buf          processedOptions;

extern char         attachfile[64][MAX_PATH+1];
extern char         my_hostname_wanted[1024];
extern FILE *       logOut;
extern char         charset[40];            // Added 25 Apr 2001 Tim Charron (default ISO-8859-1)

extern char         attachtype[64];
extern char         timestamp;
extern Buf          alternateText;
extern int          delayBetweenMsgs;

extern int          exitRequired;

char strInstall[]       = "-install";
char strSaveSettings[]  = "-SaveSettings";
#if BLAT_LITE
#else
char strInstallSMTP[]   = "-installSMTP";
  #if INCLUDE_NNTP
char strInstallNNTP[]   = "-installNNTP";
  #endif
  #if INCLUDE_POP3
char strInstallPOP3[]   = "-installPOP3";
  #endif
  #if INCLUDE_IMAP
char strInstallIMAP[]   = "-installIMAP";
  #endif
#endif

char strP[]             = "-p";
char strProfile[]       = "-profile";
char strServer[]        = "-server";
#if BLAT_LITE
#else
char strServerSMTP[]    = "-serverSMTP";
  #if INCLUDE_NNTP
char strServerNNTP[]    = "-serverNNTP";
  #endif
  #if INCLUDE_POP3
char strServerPOP3[]    = "-serverPOP3";
  #endif
  #if INCLUDE_IMAP
char strServerIMAP[]    = "-serverIMAP";
  #endif
#endif
char strF[]             = "-f";
char strI[]             = "-i";
char strPort[]          = "-port";
#if BLAT_LITE
#else
char strPortSMTP[]      = "-portSMTP";
  #if INCLUDE_NNTP
char strPortNNTP[]      = "-portNNTP";
  #endif
  #if INCLUDE_POP3
char strPortPOP3[]      = "-portPOP3";
  #endif
  #if INCLUDE_IMAP
char strPortIMAP[]      = "-portIMAP";
  #endif
#endif
char strUsername[]      = "-username";
char strU[]             = "-u";
char strPassword[]      = "-password";
char strPwd[]           = "-pwd";
char strPw[]            = "-pw";
#if INCLUDE_POP3
char strPop3Username[]  = "-pop3username";
char strPop3U[]         = "-pu";
char strPop3Password[]  = "-pop3password";
char strPop3Pw[]        = "-ppw";
#endif
#if INCLUDE_IMAP
char strImapUsername[]  = "-imapusername";
char strImapU[]         = "-iu";
char strImapPassword[]  = "-imappassword";
char strImapPw[]        = "-ipw";
#endif
char strTry[]           = "-try";
char strMailFrom[]      = "-mailfrom";
char strFrom[]          = "-from";
char strSender[]        = "-sender";


#if INCLUDE_SUPERDEBUG
char superDebug = FALSE;
#endif

int  printUsage( int optionPtr );

#if BLAT_LITE
#else

static int ReadNamesFromFile(char * type, char * namesfilename, Buf &listofnames) {
    WinFile fileh;
    int     found;
    DWORD   filesize;
    DWORD   dummy;
    Buf     p;
    char  * tmpstr;

    if ( !fileh.OpenThisFile(namesfilename)) {
        printMsg("error reading %s, aborting\n",namesfilename);
        return(3);
    }
    filesize = fileh.GetSize();
    tmpstr = (char *)malloc( filesize + 1 );
    if ( !tmpstr ) {
        fileh.Close();
        printMsg("error reading %s, aborting\n",namesfilename);
        return(5);
    }

    if ( !fileh.ReadThisFile(tmpstr, filesize, &dummy, NULL) ) {
        fileh.Close();
        free(tmpstr);
        printMsg("error reading %s, aborting\n",namesfilename);
        return(5);
    }
    fileh.Close();

    tmpstr[filesize] = 0;
    parseCommaDelimitString( tmpstr, p, FALSE );
    free(tmpstr);
    tmpstr = p.Get();

    if ( !tmpstr ) {
        printMsg("error reading %s, aborting\n",namesfilename);
        return(5);
    }

    // Count and consolidate addresses.
    found = 0;
    for ( ; *tmpstr; tmpstr += strlen(tmpstr) + 1 ) {
        if ( found )
            listofnames.Add( ',' );

        listofnames.Add( tmpstr );
        found++;
    }

    printMsg("Read %d %s addresses from %s\n",found,type,namesfilename);

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


static int checkInstallOption ( int argc, char ** argv, int this_arg, int startargv )
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

    SMTPHost[0]     =
    Sender[0]       =
    Profile[0]      =
    AUTHLogin[0]    =
    AUTHPassword[0] = 0;

    strcpy(SMTPPort, defaultSMTPPort);
    strcpy( Try, "1" );

    if ( !argc && !_strcmpi( argv[this_arg], strSaveSettings ) )
        argc = (INSTALL_STATE_DONE * 2) + 1;

    for ( ; argc && (argState < INSTALL_STATE_DONE) && argv[this_arg+1]; ) {
        argc--;
        this_arg++;
        startargv++;

        if ( !_stricmp(argv[this_arg], "-hkcu") ) {
            useHKCU = TRUE;
            continue;
        }

        if ( !_stricmp(argv[this_arg], strServer    )
#if BLAT_LITE
#else
          || !_stricmp(argv[this_arg], strServerSMTP)
#endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( SMTPHost, argv[this_arg], sizeof(SMTPHost)-1 );
                hostFound = TRUE;
                argState = INSTALL_STATE_SERVER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strF       )
          || !_stricmp(argv[this_arg], strI       )
          || !_stricmp(argv[this_arg], strSender  )
          || !_stricmp(argv[this_arg], strMailFrom)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( Sender, argv[this_arg], sizeof(Sender)-1 );
                senderFound = TRUE;
                argState = INSTALL_STATE_SENDER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strTry) ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( Try, argv[this_arg], sizeof(Try)-1 );
                tryFound = TRUE;
                argState = INSTALL_STATE_TRY+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strPort    )
#if BLAT_LITE
#else
          || !_stricmp(argv[this_arg], strPortSMTP)
#endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( SMTPPort, argv[this_arg], sizeof(SMTPPort)-1 );
                portFound = TRUE;
                argState = INSTALL_STATE_PORT+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strP      )
          || !_stricmp(argv[this_arg], strProfile)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the profile.
                    strncpy( Profile, argv[this_arg], sizeof(Profile)-1 );      // Keep the profile if not "-"

                profileFound = TRUE;
                argState = INSTALL_STATE_PROFILE+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strU       )
          || !_stricmp(argv[this_arg], strUsername)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the login name.
                    strncpy( AUTHLogin, argv[this_arg], sizeof(AUTHLogin)-1 );  // Keep the login name if not "-"

                loginFound = TRUE;
                argState = INSTALL_STATE_LOGIN_ID+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strPw      )
          || !_stricmp(argv[this_arg], strPwd     )
          || !_stricmp(argv[this_arg], strPassword)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the password.
                    strncpy( AUTHPassword, argv[this_arg], sizeof(AUTHPassword)-1 ); // Keep the password if not "-"

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
                strncpy( SMTPHost, argv[this_arg], sizeof(SMTPHost)-1 );
                continue;
            }

        case INSTALL_STATE_SENDER:
            argState++;
            if ( !senderFound ) {
                senderFound = TRUE;
                strncpy( Sender, argv[this_arg], sizeof(Sender)-1 );
                continue;
            }

        case INSTALL_STATE_TRY:
            argState++;
            if ( !tryFound ) {
                tryFound = TRUE;
                strncpy( Try, argv[this_arg], sizeof(Try)-1 );
                continue;
            }

        case INSTALL_STATE_PORT:
            argState++;
            if ( !portFound ) {
                portFound = TRUE;
                strncpy( SMTPPort, argv[this_arg], sizeof(SMTPPort)-1 );
                continue;
            }

        case INSTALL_STATE_PROFILE:
            argState++;
            if ( !profileFound ) {
                profileFound = TRUE;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the profile.
                    strncpy( Profile, argv[this_arg], sizeof(Profile)-1 );      // Keep the profile if not "-"
                continue;
            }

        case INSTALL_STATE_LOGIN_ID:
            argState++;
            if ( !loginFound ) {
                loginFound = TRUE;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the login name.
                    strncpy( AUTHLogin, argv[this_arg], sizeof(AUTHLogin)-1 );  // Keep the login name if not "-"
                continue;
            }

        case INSTALL_STATE_PASSWORD:
            argState++;
            if ( !pwdFound ) {
                pwdFound = TRUE;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then delete the password.
                    strncpy( AUTHPassword, argv[this_arg], sizeof(AUTHPassword)-1 ); // Keep the password if not "-"
                continue;
            }
        }
    }

    if ( !argState ) {
        if ( hostFound && senderFound )
            argState = INSTALL_STATE_DONE;
    }

    if ( argc ) {
        if ( argv[this_arg+1] && !_stricmp(argv[this_arg+1], "-hkcu") ) {
            useHKCU = TRUE;
            argc--;
            this_arg++;
            startargv++;
        }
    }

    if ( argState ) {
        if ( !strcmp(Try,"-") ) strcpy(Try,"1");
        if ( !strcmp(Try,"0") ) strcpy(Try,"1");

        if ( !strcmp(SMTPPort,"-") ) strcpy(SMTPPort,defaultSMTPPort);
        if ( !strcmp(SMTPPort,"0") ) strcpy(SMTPPort,defaultSMTPPort);

#if INCLUDE_NNTP
        NNTPHost[0] = 0;
#endif
#if INCLUDE_POP3
        POP3Host[0] = 0;
#endif
#if INCLUDE_IMAP
        IMAPHost[0] = 0;
#endif
        if ( CreateRegEntry( useHKCU ) == 0 ) {
            printMsg("SMTP server set to %s on port %s with user %s, retry %s time(s)\n", SMTPHost, SMTPPort, Sender, Try );
            regerr = 0;
            printMsg( NULL );
            if ( logOut )
                fclose( logOut );

            exitRequired = TRUE;
            return(0);
        }
    } else {
        printMsg( "To set the SMTP server's name/address and your username/email address for that\n" \
                  "server machine do:\n" \
                  "blat -install  server_name  your_email_address\n" \
                  "or use '-server <server_name>' and '-f <your_email_address>'\n");
        return(-6);
    }

    return(startargv);
}

#if INCLUDE_NNTP

static int checkNNTPInstall ( int argc, char ** argv, int this_arg, int startargv )
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

    NNTPHost[0]     =
    Sender[0]       =
    Profile[0]      =
    AUTHLogin[0]    =
    AUTHPassword[0] = 0;

    strcpy(NNTPPort, defaultNNTPPort);
    strcpy( Try, "1" );

    for ( ; argc && argState < INSTALL_STATE_DONE; ) {
        argc--;
        this_arg++;
        startargv++;

        if ( !_stricmp(argv[this_arg], "-hkcu") ) {
            useHKCU = TRUE;
            continue;
        }

        if ( !_stricmp(argv[this_arg], strServer    )
          || !_stricmp(argv[this_arg], strServerNNTP)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( NNTPHost, argv[this_arg], sizeof(NNTPHost)-1 );
                hostFound = TRUE;
                argState = INSTALL_STATE_SERVER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strF       )
          || !_stricmp(argv[this_arg], strI       )
          || !_stricmp(argv[this_arg], strSender  )
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( Sender, argv[this_arg], sizeof(Sender)-1 );
                senderFound = TRUE;
                argState = INSTALL_STATE_SENDER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strTry) ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( Try, argv[this_arg], sizeof(Try)-1 );
                tryFound = TRUE;
                argState = INSTALL_STATE_TRY+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strPort    )
#if BLAT_LITE
#else
          || !_stricmp(argv[this_arg], strPortNNTP)
#endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( NNTPPort, argv[this_arg], sizeof(NNTPPort)-1 );
                portFound = TRUE;
                argState = INSTALL_STATE_PORT+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strP      )
          || !_stricmp(argv[this_arg], strProfile)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the profile.
                    strncpy( Profile, argv[this_arg], sizeof(Profile)-1 );      // Keep the profile if not "-"

                profileFound = TRUE;
                argState = INSTALL_STATE_PROFILE+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strU       )
          || !_stricmp(argv[this_arg], strUsername)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the login name.
                    strncpy( AUTHLogin, argv[this_arg], sizeof(AUTHLogin)-1 );    // Keep the login name if not "-"

                loginFound = TRUE;
                argState = INSTALL_STATE_LOGIN_ID+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strPw      )
          || !_stricmp(argv[this_arg], strPwd     )
          || !_stricmp(argv[this_arg], strPassword)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the password.
                    strncpy( AUTHPassword, argv[this_arg], sizeof(AUTHPassword)-1 ); // Keep the password if not "-"

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
                strncpy( NNTPHost, argv[this_arg], sizeof(NNTPHost)-1 );
                continue;
            }

        case INSTALL_STATE_SENDER:
            argState++;
            if ( !senderFound ) {
                senderFound = TRUE;
                strncpy( Sender, argv[this_arg], sizeof(Sender)-1 );
                continue;
            }

        case INSTALL_STATE_TRY:
            argState++;
            if ( !tryFound ) {
                tryFound = TRUE;
                strncpy( Try, argv[this_arg], sizeof(Try)-1 );
                continue;
            }

        case INSTALL_STATE_PORT:
            argState++;
            if ( !portFound ) {
                portFound = TRUE;
                strncpy( NNTPPort, argv[this_arg], sizeof(NNTPPort)-1 );
                continue;
            }

        case INSTALL_STATE_PROFILE:
            argState++;
            if ( !profileFound ) {
                profileFound = TRUE;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the profile.
                    strncpy( Profile, argv[this_arg], sizeof(Profile)-1 );      // Keep the profile if not "-"
                continue;
            }

        case INSTALL_STATE_LOGIN_ID:
            argState++;
            if ( !loginFound ) {
                loginFound = TRUE;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the login name.
                    strncpy( AUTHLogin, argv[this_arg], sizeof(AUTHLogin)-1 );    // Keep the login name if not "-"
                continue;
            }

        case INSTALL_STATE_PASSWORD:
            argState++;
            if ( !pwdFound ) {
                pwdFound = TRUE;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then delete the password.
                    strncpy( AUTHPassword, argv[this_arg], sizeof(AUTHPassword)-1 ); // Keep the password if not "-"
                continue;
            }
        }
    }

    if ( !argState ) {
        if ( hostFound && senderFound )
            argState = INSTALL_STATE_DONE;
    }

    if ( argc ) {
        if ( !_stricmp(argv[++this_arg], "-hkcu") ) {
            useHKCU = TRUE;
            startargv++;
        }
    }

    if ( argState ) {
        if ( !strcmp(Try,"-") ) strcpy(Try,"1");
        if ( !strcmp(Try,"0") ) strcpy(Try,"1");

        if ( !strcmp(NNTPPort,"-") ) strcpy(NNTPPort,defaultNNTPPort);
        if ( !strcmp(NNTPPort,"0") ) strcpy(NNTPPort,defaultNNTPPort);

#if INCLUDE_POP3
        POP3Host[0] = 0;
#endif
#if INCLUDE_IMAP
        IMAPHost[0] = 0;
#endif
        SMTPHost[0] = 0;
        if ( CreateRegEntry( useHKCU ) == 0 ) {
            printMsg("NNTP server set to %s on port %s with user %s, retry %s time(s)\n", NNTPHost, NNTPPort, Sender, Try );
            regerr = 0;
            printMsg( NULL );
            if ( logOut )
                fclose( logOut );

            exitRequired = TRUE;
            return(0);
        }
    } else {
        printMsg( "To set the NNTP server's address and the user name at that address do:\nblat -installNNTP server username\n");
        return(-6);
    }

    return(startargv);
}

static int checkNNTPSrvr ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( NNTPHost, argv[this_arg+1], sizeof(NNTPHost)-1 );

    return(1);
}

static int checkNNTPPort ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( NNTPPort, argv[this_arg+1], sizeof(NNTPPort)-1 );

    return(1);
}

static int checkNNTPGroups ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( groups.Length() ) {
        printMsg("Only one -groups can be used at a time.  Aborting.\n");
        return(-1);
    }
    groups.Add( argv[this_arg+1] );

    return(1);
}
#endif

#if INCLUDE_POP3

static int checkXtndXmit ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    xtnd_xmit_wanted = TRUE;

    return(0);
}

static int checkPOP3Install ( int argc, char ** argv, int this_arg, int startargv )
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

    POP3Host[0]     =
    Sender[0]       =
    Profile[0]      =
    POP3Login[0]    =
    POP3Password[0] = 0;

    strcpy(POP3Port, defaultPOP3Port);

    for ( ; argc && argState < INSTALL_STATE_DONE; ) {
        argc--;
        this_arg++;
        startargv++;

        if ( !_stricmp(argv[this_arg], "-hkcu") ) {
            useHKCU = TRUE;
            continue;
        }

        if ( !_stricmp(argv[this_arg], strServer    )
          || !_stricmp(argv[this_arg], strServerPOP3)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( POP3Host, argv[this_arg], sizeof(POP3Host)-1 );
                hostFound = TRUE;
                argState = INSTALL_STATE_SERVER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strF       )
          || !_stricmp(argv[this_arg], strI       )
          || !_stricmp(argv[this_arg], strSender  )
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( Sender, argv[this_arg], sizeof(Sender)-1 );
                senderFound = TRUE;
                argState = INSTALL_STATE_SENDER+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strTry) ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( Try, argv[this_arg], sizeof(Try)-1 );
                tryFound = TRUE;
                argState = INSTALL_STATE_TRY+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strPort    )
  #if BLAT_LITE
  #else
          || !_stricmp(argv[this_arg], strPortPOP3)
  #endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( POP3Port, argv[this_arg], sizeof(POP3Port)-1 );
                portFound = TRUE;
                argState = INSTALL_STATE_PORT+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strP      )
          || !_stricmp(argv[this_arg], strProfile)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the profile.
                    strncpy( Profile, argv[this_arg], sizeof(Profile)-1 );      // Keep the profile if not "-"

                profileFound = TRUE;
                argState = INSTALL_STATE_PROFILE+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strU           )
          || !_stricmp(argv[this_arg], strUsername    )
  #if BLAT_LITE
  #else
          || !_stricmp(argv[this_arg], strPop3U       )
          || !_stricmp(argv[this_arg], strPop3Username)
  #endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the login name.
                    strncpy( POP3Login, argv[this_arg], sizeof(POP3Login)-1 );    // Keep the login name if not "-"

                loginFound = TRUE;
                argState = INSTALL_STATE_LOGIN_ID+1;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strPw          )
          || !_stricmp(argv[this_arg], strPwd     )
          || !_stricmp(argv[this_arg], strPassword    )
  #if BLAT_LITE
  #else
          || !_stricmp(argv[this_arg], strPop3Pw      )
          || !_stricmp(argv[this_arg], strPop3Password)
  #endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the password.
                    strncpy( POP3Password, argv[this_arg], sizeof(POP3Password)-1 ); // Keep the password if not "-"

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
                strncpy( POP3Host, argv[this_arg], sizeof(POP3Host)-1 );
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
                strncpy( POP3Port, argv[this_arg], sizeof(POP3Port)-1 );
                continue;
            }

        case INSTALL_STATE_PROFILE:
            argState++;
            if ( !profileFound ) {
                profileFound = TRUE;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the profile.
                    strncpy( Profile, argv[this_arg], sizeof(Profile)-1 );      // Keep the profile if not "-"
                continue;
            }

        case INSTALL_STATE_LOGIN_ID:
            argState++;
            if ( !loginFound ) {
                loginFound = TRUE;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the login name.
                    strncpy( POP3Login, argv[this_arg], sizeof(POP3Login)-1 );    // Keep the login name if not "-"
                continue;
            }

        case INSTALL_STATE_PASSWORD:
            argState++;
            if ( !pwdFound ) {
                pwdFound = TRUE;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then delete the password.
                    strncpy( POP3Password, argv[this_arg], sizeof(POP3Password)-1 ); // Keep the password if not "-"
                continue;
            }
        }
    }

    if ( !argState ) {
        if ( hostFound && senderFound )
            argState = INSTALL_STATE_DONE;
    }

    if ( argc ) {
        if ( !_stricmp(argv[++this_arg], "-hkcu") ) {
            useHKCU = TRUE;
            startargv++;
        }
    }

    if ( argState ) {
        if ( !strcmp(POP3Port,"-") ) strcpy(POP3Port,defaultPOP3Port);
        if ( !strcmp(POP3Port,"0") ) strcpy(POP3Port,defaultPOP3Port);

  #if INCLUDE_NNTP
        NNTPHost[0] = 0;
  #endif
        SMTPHost[0] = 0;
        if ( CreateRegEntry( useHKCU ) == 0 ) {
            printMsg("POP3 server set to %s on port %s\n", POP3Host, POP3Port );
            regerr = 0;
            printMsg( NULL );
            if ( logOut )
                fclose( logOut );

            exitRequired = TRUE;
            return(0);
        }
    } else {
        printMsg( "To set the POP3 server's address and the login name at that address do:\nblat -installPOP3 server - - - - loginname loginpwd\n");
        return(-6);
    }

    return(startargv);
}

static int checkPOP3Srvr ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( POP3Host, argv[this_arg+1], sizeof(POP3Host)-1 );

    return(1);
}

static int checkPOP3Port ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( POP3Port, argv[this_arg+1], sizeof(POP3Port)-1 );

    return(1);
}
#endif

#if INCLUDE_IMAP

static int checkIMAPInstall ( int argc, char ** argv, int this_arg, int startargv )
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

    IMAPHost[0]     =
    Sender[0]       =
    Profile[0]      =
    IMAPLogin[0]    =
    IMAPPassword[0] = 0;

    strcpy(IMAPPort, defaultIMAPPort);

    for ( ; argc && argState < INSTALL_STATE_DONE; ) {
        argc--;
        this_arg++;
        startargv++;

        if ( !_stricmp(argv[this_arg], "-hkcu") ) {
            useHKCU = TRUE;
            continue;
        }

        if ( !_stricmp(argv[this_arg], strServer    )
          || !_stricmp(argv[this_arg], strServerIMAP)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( IMAPHost, argv[this_arg], sizeof(IMAPHost)-1 );
                hostFound = TRUE;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strF       )
          || !_stricmp(argv[this_arg], strI       )
          || !_stricmp(argv[this_arg], strSender  )
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( Sender, argv[this_arg], sizeof(Sender)-1 );
                senderFound = TRUE;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strTry) ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( Try, argv[this_arg], sizeof(Try)-1 );
                tryFound = TRUE;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strPort    )
  #if BLAT_LITE
  #else
          || !_stricmp(argv[this_arg], strPortIMAP)
  #endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                strncpy( IMAPPort, argv[this_arg], sizeof(IMAPPort)-1 );
                portFound = TRUE;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strP      )
          || !_stricmp(argv[this_arg], strProfile)
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the profile.
                    strncpy( Profile, argv[this_arg], sizeof(Profile)-1 );      // Keep the profile if not "-"

                profileFound = TRUE;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strU           )
          || !_stricmp(argv[this_arg], strUsername    )
  #if BLAT_LITE
  #else
          || !_stricmp(argv[this_arg], strImapU       )
          || !_stricmp(argv[this_arg], strImapUsername)
  #endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the login name.
                    strncpy( IMAPLogin, argv[this_arg], sizeof(IMAPLogin)-1 );    // Keep the login name if not "-"

                loginFound = TRUE;
                continue;
            }

            argState = INSTALL_STATE_SERVER;
            break;
        }

        if ( !_stricmp(argv[this_arg], strPw          )
          || !_stricmp(argv[this_arg], strPwd     )
          || !_stricmp(argv[this_arg], strPassword    )
  #if BLAT_LITE
  #else
          || !_stricmp(argv[this_arg], strImapPw      )
          || !_stricmp(argv[this_arg], strImapPassword)
  #endif
           ) {
            if ( argc ) {
                argc--;
                this_arg++;
                startargv++;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the password.
                    strncpy( IMAPPassword, argv[this_arg], sizeof(IMAPPassword)-1 ); // Keep the password if not "-"

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
                strncpy( IMAPHost, argv[this_arg], sizeof(IMAPHost)-1 );
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
                strncpy( IMAPPort, argv[this_arg], sizeof(IMAPPort)-1 );
                continue;
            }

        case INSTALL_STATE_PROFILE:
            argState++;
            if ( !profileFound ) {
                profileFound = TRUE;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the profile.
                    strncpy( Profile, argv[this_arg], sizeof(Profile)-1 );      // Keep the profile if not "-"
                continue;
            }

        case INSTALL_STATE_LOGIN_ID:
            argState++;
            if ( !loginFound ) {
                loginFound = TRUE;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then skip the login name.
                    strncpy( IMAPLogin, argv[this_arg], sizeof(IMAPLogin)-1 );    // Keep the login name if not "-"
                continue;
            }

        case INSTALL_STATE_PASSWORD:
            argState++;
            if ( !pwdFound ) {
                pwdFound = TRUE;
                if ( strcmp(argv[this_arg], "-") )          // If "-" found, then delete the password.
                    strncpy( IMAPPassword, argv[this_arg], sizeof(IMAPPassword)-1 ); // Keep the password if not "-"
                continue;
            }
        }
    }

    if ( !argState ) {
        if ( hostFound && senderFound )
            argState = INSTALL_STATE_DONE;
    }

    if ( argc ) {
        if ( !_stricmp(argv[++this_arg], "-hkcu") ) {
            useHKCU = TRUE;
            startargv++;
        }
    }

    if ( argState ) {
        if ( !strcmp(IMAPPort,"-") ) strcpy(IMAPPort,defaultIMAPPort);
        if ( !strcmp(IMAPPort,"0") ) strcpy(IMAPPort,defaultIMAPPort);

  #if INCLUDE_NNTP
        NNTPHost[0] = 0;
  #endif
        SMTPHost[0] = 0;
        if ( CreateRegEntry( useHKCU ) == 0 ) {
            printMsg("IMAP server set to %s on port %s\n", IMAPHost, IMAPPort );
            regerr = 0;
            printMsg( NULL );
            if ( logOut )
                fclose( logOut );

            exitRequired = TRUE;
            return(0);
        }
    } else {
        printMsg( "To set the IMAP server's address and the login name at that address do:\nblat -installIMAP server - - - - loginname loginpwd\n");
        return(-6);
    }

    return(startargv);
}

static int checkIMAPSrvr ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( IMAPHost, argv[this_arg+1], sizeof(IMAPHost)-1 );

    return(1);
}

static int checkIMAPPort ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( IMAPPort, argv[this_arg+1], sizeof(IMAPPort)-1 );

    return(1);
}
#endif

#if BLAT_LITE
#else

static int checkOptionFile ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( !optionsFile[0] )
        strncpy( optionsFile, argv[this_arg+1], _MAX_PATH - 1);

    return(1);
}
#endif

static int checkToOption ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( destination.Length() ) {
        printMsg("Warning: -t/-to is being used with -tf, or another -t/to.\n\t Previous values will be ignored.\n");
        destination.Free();
    }

    destination.Add( argv[this_arg+1] );

    return(1);
}

#if BLAT_LITE
#else

static int checkToFileOption ( int argc, char ** argv, int this_arg, int startargv )
{
    int ret;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( destination.Length() ) {
        printMsg("Warning: -tf is being used with  -t/-to, or another -tf.\n\t Previous values will be ignored.\n");
        destination.Free();
    }

    ret = ReadNamesFromFile("TO", argv[this_arg+1], destination);
    if ( ret )
        return(0 - ret);

    return(1);
}
#endif

static int checkCcOption ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( cc_list.Length() ) {
        printMsg("Warning: -c/-cc is being used with  -cf, or another -c/-cc.\n\t Previous values will be ignored.\n");
        cc_list.Free();
    }
    cc_list.Add( argv[this_arg+1] );

    return(1);
}

#if BLAT_LITE
#else

static int checkCcFileOption ( int argc, char ** argv, int this_arg, int startargv )
{
    int ret;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( cc_list.Length() ) {
        printMsg("Warning: -cf is being used with  -c/-cc, or another -cf.\n\t Previous values will be ignored.\n");
        cc_list.Free();
    }

    ret = ReadNamesFromFile("CC", argv[this_arg+1], cc_list);
    if ( ret )
        return(0 - ret);

    return(1);
}
#endif

static int checkBccOption ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( bcc_list.Length() ) {
        printMsg("Warning: -b/-bcc is being used with -bf, or another -b/-bcc.\n\t Previous values will be ignored.\n");
        bcc_list.Free();
    }
    bcc_list.Add( argv[this_arg+1] );

    return(1);
}

#if BLAT_LITE
#else

static int checkBccFileOption ( int argc, char ** argv, int this_arg, int startargv )
{
    int ret;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( bcc_list.Length() ) {
        printMsg("Warning: -bf is being used with  -b/-bcc, or another -bf.\n\t Previous values will be ignored.\n");
        bcc_list.Free();
    }

    ret = ReadNamesFromFile("BCC", argv[this_arg+1], bcc_list);
    if ( ret )
        return(0 - ret);

    return(1);
}
#endif

static int checkSubjectOption ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    strncpy( subject, argv[this_arg+1], sizeof(subject)-1 );

    return(1);
}

#if BLAT_LITE
#else

static int checkSubjectFile ( int argc, char ** argv, int this_arg, int startargv )
{
    FILE * infile;
    int    x;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    infile = fopen(argv[this_arg+1], "r");
    if ( infile ) {
        memset(subject, 0x00, SUBJECT_SIZE);
        fgets(subject, SUBJECT_SIZE, infile);    //lint !e534 ignore return
        fclose(infile);
        for ( x = 0; subject[x]; x++ ) {
            if ( (subject[x] == '\n') || (subject[x] == '\t') )
                subject[x] = ' ';   // convert LF and tabs to spaces
            else
                if ( subject[x] == '\r' ) {
                    strcpy( &subject[x], &subject[x+1] );
                    x--;            // Remove CR bytes.
                }
        }
        for ( ; x; ) {
            if ( subject[--x] != ' ' )
                break;

            strcpy( &subject[x], &subject[x+1] );   // Strip off trailing spaces.
        }
    } else {
        strncpy(subject, argv[this_arg+1], SUBJECT_SIZE-1);
        subject[SUBJECT_SIZE-1] = 0x00;
    }

    return(1);
}
#endif

static int checkSkipSubject ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    ssubject = TRUE;                /*$$ASD*/

    return(0);
}

static int checkBodyText ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    bodyparameter.Add( argv[this_arg+1] );
    strcpy(bodyFilename, "-");

    return(1);
}

static int checkBodyFile ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( !argv[this_arg+1] )
        return(-1);

    strncpy(bodyFilename, argv[this_arg+1], sizeof(bodyFilename)-1 );
    bodyFilename[sizeof(bodyFilename)-1] = 0;

    return(1);
}

static int checkProfileEdit ( int argc, char ** argv, int this_arg, int startargv )
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
    ListProfiles("<all>");
#else
/*    if ( argc == 0 ) {
        ListProfiles("<all>");
    } else
 */
    if ( !argv[this_arg] ) {    // argc = 0
        ListProfiles("<all>");
    } else {
        if ( !strcmp(argv[this_arg], "-delete") ) {
            this_arg++;
            if ( argc )
                argc--;

            for ( ; argc; argc-- ) {
                if ( !strcmp( argv[this_arg], "-hkcu" ) ) {
                    useHKCU = TRUE;
                    this_arg++;
                    continue;
                }
                DeleteRegEntry( argv[this_arg], useHKCU );
                this_arg++;
            }
        } else {
            if ( !strcmp(argv[this_arg], "-h") ) {
                if ( argc )
                    argc--;

                this_arg++;
                ShowRegHelp();
            }
            if ( argc == 0 ) {
                ListProfiles("<all>");
            } else {
                for ( ; argc; argc-- ) {
                    if ( !strcmp( argv[this_arg], "-hkcu" ) ) {
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

static int checkProfileToUse ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( Profile, argv[this_arg+1], sizeof(Profile)-1 );

    return(1);
}
#endif

static int checkServerOption ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( SMTPHost, argv[this_arg+1], sizeof(SMTPHost)-1 );

    return(1);
}

static int checkFromOption ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( loginname, argv[this_arg+1], sizeof(loginname)-1 );

    return(1);
}

static int checkImpersonate ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( senderid, argv[this_arg+1], sizeof(senderid)-1 );
    impersonating = TRUE;

    return(1);
}

static int checkPortOption ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( SMTPPort, argv[this_arg+1], sizeof(SMTPPort)-1 );

    return(1);
}

static int checkUserIDOption ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( AUTHLogin, argv[this_arg+1], sizeof(AUTHLogin)-1 );

    return(1);
}

static int checkPwdOption ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( AUTHPassword, argv[this_arg+1], sizeof(AUTHPassword)-1 );

    return(1);
}

#if INCLUDE_POP3
static int checkPop3UIDOption ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( POP3Login, argv[this_arg+1], sizeof(POP3Login)-1 );

    return(1);
}

static int checkPop3PwdOption ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( POP3Password, argv[this_arg+1], sizeof(POP3Password)-1 );

    return(1);
}
#endif

#if INCLUDE_IMAP
static int checkImapUIDOption ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( IMAPLogin, argv[this_arg+1], sizeof(IMAPLogin)-1 );

    return(1);
}

static int checkImapPwdOption ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( IMAPPassword, argv[this_arg+1], sizeof(IMAPPassword)-1 );

    return(1);
}
#endif

#if SUPPORT_GSSAPI
static int checkGssapiMutual (int argc, char **argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

#if SUBMISSION_PORT
    strncpy(SMTPPort, SubmissionPort, sizeof(SMTPPort)-1);
#endif
    authgssapi = TRUE;
    mutualauth = TRUE;
    return(0);
}

static int checkGssapiClient (int argc, char **argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

#if SUBMISSION_PORT
    strncpy(SMTPPort, SubmissionPort, sizeof(SMTPPort)-1 );
#endif
    authgssapi = TRUE;
    mutualauth = FALSE;
    return(0);
}

static int checkServiceName (int argc, char **argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( servicename, argv[this_arg+1], sizeof(servicename)-1 );
    return(1);
}

static int checkProtectionLevel (int argc, char **argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    switch (argv[this_arg+1][0]) {
    case 'N':
    case 'n':
    case '0':
        gss_protection_level = GSSAUTH_P_NONE;
        break;
    case 'I':
    case 'i':
    case '1':
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

static int checkBypassCramMD5 (int argc, char **argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    ByPassCRAM_MD5 = TRUE;
    return(0);
}

static int checkOrgOption ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( organization, argv[this_arg+1], sizeof(organization)-1 );

    return(1);
}

static int checkXHeaders ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    // is argv[] "-x"? If so, argv[3] is an X-Header
    // Header MUST start with X-
    if ( (argv[this_arg+1][0] == 'X') && (argv[this_arg+1][1] == '-') ) {
        if ( xheaders[0] ) {
            strcat(xheaders, "\r\n");
        }

        strcat( xheaders, argv[this_arg+1] );
    }

    return(1);
}

static int checkDisposition ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    disposition = TRUE;

    return(0);
}

static int checkReadReceipt ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    returnreceipt = TRUE;

    return(0);
}

static int checkNoHeaders ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    noheader = 1;

    return(0);
}

static int checkNoHeaders2 ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    noheader = 2;

    return(0);
}
#endif

static int checkPriority ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    // Toby Korn 8/4/1999
    // Check for priority. 0 is Low, 1 is High - you don't need
    // normal, since omission of a priority is normal.

    priority[0] = argv[this_arg+1][0];
    priority[1] = 0;

    return(1);
}

static int checkSensitivity ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    // Check for sensitivity. 0 is personal, 1 is private, 2 is company-confidential
    // normal, since omission of a sensitivity is normal.

    sensitivity[0] = argv[this_arg+1][0];
    sensitivity[1] = 0;

    return(1);
}

static int checkCharset ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( charset, argv[this_arg+1], sizeof(charset)-1 );

    return(1);
}

#if BLAT_LITE
#else

static int checkDeliveryStat ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    _strlwr( argv[this_arg+1] );
    if ( strchr(argv[this_arg+1], 'n') )
        deliveryStatusRequested = DSN_NEVER;
    else {
        if ( strchr(argv[this_arg+1], 's') )
            deliveryStatusRequested |= DSN_SUCCESS;

        if ( strchr(argv[this_arg+1], 'f') )
            deliveryStatusRequested |= DSN_FAILURE;

        if ( strchr(argv[this_arg+1], 'd') )
            deliveryStatusRequested |= DSN_DELAYED;
    }

    return(1);
}

static int checkHdrEncB ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    forcedHeaderEncoding = 'b';

    return(0);
}

static int checkHdrEncQ ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    forcedHeaderEncoding = 'q';

    return(0);
}
#endif

#if SUPPORT_YENC
static int check_yEnc ( int argc, char ** argv, int this_arg, int startargv )
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

static int checkMime ( int argc, char ** argv, int this_arg, int startargv )
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

static int checkUUEncode ( int argc, char ** argv, int this_arg, int startargv )
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

static int checkLongUUEncode ( int argc, char ** argv, int this_arg, int startargv )
{
    uuencodeBytesLine = 63;     // must be a multiple of three and less than 64.

    return checkUUEncode( argc, argv, this_arg, startargv );
}

static int checkBase64Enc ( int argc, char ** argv, int this_arg, int startargv )
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

static int checkEnriched ( int argc, char ** argv, int this_arg, int startargv )
{
    strcpy( textmode, "enriched" );

    return checkMime( argc, argv, this_arg, startargv );
}
#endif

static int checkHTML ( int argc, char ** argv, int this_arg, int startargv )
{
    strcpy( textmode, "html" );

    return checkMime( argc, argv, this_arg, startargv );
}

#if BLAT_LITE
#else

static int checkUnicode ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    utf = UTF_REQUESTED;
    return 0;
}
#endif

static int addToAttachments ( char ** argv, int this_arg, char aType )
{
    Buf    tmpstr;
    char * srcptr;
    int    i;

    parseCommaDelimitString( argv[this_arg+1], tmpstr, TRUE );
    srcptr = tmpstr.Get();
    if ( srcptr ) {
        for ( ; *srcptr; ) {

            if ( attach == 64 ) {
                printMsg("Max of 64 files allowed!  Others are being ignored.\n");
                break;
            }

            for ( i = 0; i < attach; i++ ) {
                if ( aType < attachtype[i] )
                    break;
            }

            if ( i == attach ) {
                attachtype[attach] = aType;
                strncpy( (char *)attachfile[attach++], srcptr, sizeof(attachfile[0])-1 );
            } else {
                for ( int j = 63; j > i; j-- ) {
                    attachtype[j] = attachtype[j-1];
                    memcpy( attachfile[j], attachfile[j-1], (size_t)attachfile[0] );
                }
                attachtype[i] = aType;
                strncpy( (char *)attachfile[i], srcptr, sizeof(attachfile[0])-1 );
                attach++;
            }

            srcptr += strlen(srcptr) + 1;
#if BLAT_LITE
#else
            base64 = FALSE;
#endif
        }

        tmpstr.Free();
    }

    return(1);
}

static int checkInlineAttach ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    // -attachI added to v2.4.1
    haveAttachments = TRUE;

    return addToAttachments( argv, this_arg, INLINE_ATTACHMENT );
}

static int checkTxtFileAttach ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    // -attacht added 98.4.16 v1.7.2 tcharron@interlog.com
    haveAttachments = TRUE;

    return addToAttachments( argv, this_arg, TEXT_ATTACHMENT );
}

static int checkBinFileAttach ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    // -attach added 98.2.24 v1.6.3 tcharron@interlog.com
    haveAttachments = TRUE;

    return addToAttachments( argv, this_arg, BINARY_ATTACHMENT );
}

#if BLAT_LITE
#else

static int checkBinFileEmbed ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    haveEmbedded = TRUE;
    checkMime( argc, argv, this_arg, startargv );   // Force MIME encoding.

    return addToAttachments( argv, this_arg, EMBED_ATTACHMENT );
}

static int ReadFilenamesFromFile(char * namesfilename, char aType) {
    WinFile fileh;
    DWORD   filesize;
    DWORD   dummy;
    char  * tmpstr;

    if ( !fileh.OpenThisFile(namesfilename)) {
        printMsg("error reading %s, aborting\n", namesfilename);
        return(-3);
    }
    filesize = fileh.GetSize();
    tmpstr = (char *)malloc( filesize + 1 );
    if ( !tmpstr ) {
        fileh.Close();
        printMsg("error allocating memory for reading %s, aborting\n", namesfilename);
        return(-5);
    }

    if ( !fileh.ReadThisFile(tmpstr, filesize, &dummy, NULL) ) {
        fileh.Close();
        free(tmpstr);
        printMsg("error reading %s, aborting\n", namesfilename);
        return(-5);
    }
    fileh.Close();

    tmpstr[filesize] = 0;
    addToAttachments( &tmpstr, -1, aType );
    free(tmpstr);
    return(1);                                   // indicates no error.
}

static int checkTxtFileAttFil ( int argc, char ** argv, int this_arg, int startargv )
{
    int retVal;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    retVal = ReadFilenamesFromFile(argv[this_arg+1], TEXT_ATTACHMENT );

    if ( retVal == 1 )
        haveAttachments = TRUE;

    return retVal;
}

static int checkBinFileAttFil ( int argc, char ** argv, int this_arg, int startargv )
{
    int retVal;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    retVal = ReadFilenamesFromFile(argv[this_arg+1], BINARY_ATTACHMENT );

    if ( retVal == 1 )
        haveAttachments = TRUE;

    return retVal;
}

static int checkEmbFileAttFil ( int argc, char ** argv, int this_arg, int startargv )
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

static int checkHelp ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    printUsage( TRUE );

    return(-1);
}

static int checkQuietMode ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    quiet = TRUE;

    return(0);
}

static int checkDebugMode ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    debug = TRUE;

    return(0);
}

static int checkLogMessages ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( logOut ) {
        fclose( logOut );
        logOut = 0;
    }
    if ( !argv[this_arg+1] || !argv[this_arg+1][0] )
        return(-1);

    strncpy( logFile, argv[this_arg+1], sizeof(logFile) - 1 );
    logFile[sizeof(logFile) - 1] = 0;

    if ( logFile[0] ) {
        if ( clearLogFirst )
            logOut = fopen(logFile, "w");
        else
            logOut = fopen(logFile, "a");
    }

    // if all goes well the file is closed normally
    // if we return before closing the library SHOULD
    // close it for us..

    return(1);
}

static int checkLogOverwrite ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    clearLogFirst = TRUE;
    if ( logOut )
    {
        fclose( logOut );
        logOut = fopen(logFile, "w");
    }

    return(0);
}

static int checkTimestamp ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    timestamp = TRUE;

    return(0);
}

static int checkTimeout ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    globaltimeout = atoi(argv[this_arg+1]);

    return(1);
}

static int checkAttempts ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( Try, argv[this_arg+1], sizeof(Try)-1 );

    return(1);
}

static int checkFixPipe ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    bodyconvert = FALSE;

    return(0);
}

static int checkHostname ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( my_hostname_wanted, argv[this_arg+1], sizeof(my_hostname_wanted)-1 );

    return(1);
}

static int checkMailFrom ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( loginname, argv[this_arg+1], sizeof(loginname)-1 );

    return(1);
}

static int checkWhoFrom ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( fromid, argv[this_arg+1], sizeof(fromid)-1 );

    return(1);
}

static int checkReplyTo ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( replytoid, argv[this_arg+1], sizeof(replytoid)-1 );

    return(1);
}

static int checkReturnPath ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( returnpathid, argv[this_arg+1], sizeof(returnpathid)-1 );

    return(1);
}

static int checkSender ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( sendername, argv[this_arg+1], sizeof(sendername)-1 );

    return(1);
}

static int checkRaw ( int argc, char ** argv, int this_arg, int startargv )
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

static int checkA1Headers ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( aheaders1, argv[this_arg+1], DEST_SIZE );

    return(1);
}

static int checkA2Headers ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    strncpy( aheaders2, argv[this_arg+1], DEST_SIZE );

    return(1);
}

static int check8bitMime ( int argc, char ** argv, int this_arg, int startargv )
{
    eightBitMimeRequested = TRUE;

    return checkMime( argc, argv, this_arg, startargv );
}

static int checkAltTextFile ( int argc, char ** argv, int this_arg, int startargv )
{
    Buf     tmpstr;
    char *  srcptr;
    char    alternateTextFile[_MAX_PATH];
    WinFile atf;
    DWORD   fileSize;
    DWORD   dummy;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    parseCommaDelimitString( argv[this_arg+1], tmpstr, TRUE );
    srcptr = tmpstr.Get();
    if ( srcptr )
        strncpy( alternateTextFile, srcptr, sizeof(alternateTextFile)-1 );  // Copy only the first filename.

    tmpstr.Free();

    if ( !atf.OpenThisFile(alternateTextFile) ) {
        printMsg( "Error reading %s, Alternate Text will not be added.\n", alternateTextFile );
    } else {
        fileSize = atf.GetSize();
        alternateText.Clear();

        if ( fileSize ) {
            alternateText.Alloc( fileSize + 1 );
            if ( !atf.ReadThisFile(alternateText.Get(), fileSize, &dummy, NULL) ) {
                printMsg( "Error reading %s, Alternate Text will not be added.\n", alternateTextFile );
            } else {
                *(alternateText.Get()+fileSize) = 0;
                alternateText.SetLength(fileSize);
            }
        }
        atf.Close();
    }
    return(1);
}

static int checkAltText ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    alternateText.Clear();
    alternateText.Add( argv[this_arg+1] );
    return(1);
}
#endif
#if SUPPORT_SIGNATURES

static int checkSigFile ( int argc, char ** argv, int this_arg, int startargv )
{
    WinFile fileh;
    DWORD   sigsize;
    DWORD   dummy;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    this_arg++;
    if ( !fileh.OpenThisFile(argv[this_arg]) ) {
        printMsg( "Error reading signature file %s, signature will not be added\n", argv[this_arg] );
        return(1);
    }

    sigsize = fileh.GetSize();
    signature.Alloc( sigsize + 1 );
    signature.SetLength( sigsize );

    if ( !fileh.ReadThisFile(signature.Get(), sigsize, &dummy, NULL) ) {
        printMsg( "Error reading signature file %s, signature will not be added\n", argv[this_arg] );
        signature.Free();
    }
    else
        *signature.GetTail() = 0;

    fileh.Close();
    return(1);
}
#endif
#if SUPPORT_TAGLINES

static int checkTagFile ( int argc, char ** argv, int this_arg, int startargv )
{
    WinFile fileh;
    DWORD   tagsize;
    DWORD   count;
    int     selectedTag;
    char *  p;
    Buf     tmpBuf;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    this_arg++;
    if ( !fileh.OpenThisFile(argv[this_arg]) ) {
        printMsg( "Error reading tagline file %s, a tagline will not be added.\n", argv[this_arg] );
        return(1);
    }

    tagsize = fileh.GetSize();
    if ( tagsize ) {
        tmpBuf.Alloc( tagsize + 1 );
        tmpBuf.SetLength( tagsize );

        if ( !fileh.ReadThisFile(tmpBuf.Get(), tagsize, &count, NULL) ) {
            printMsg( "Error reading tagline file %s, a tagline will not be added.\n", argv[this_arg] );
        }

        *tmpBuf.GetTail() = 0;
        p = tmpBuf.Get();
        for ( count = 0; p; ) {
            p = strchr( p, '\n' );
            if ( p ) {
                p++;
                count++;
            }
        }

        if ( count ) {
            char * p2;

            selectedTag = rand() % count;
            p = tmpBuf.Get();

            for ( ; selectedTag; selectedTag-- )
                p = strchr( p, '\n' ) + 1;

            p2 = strchr( p, '\n' );
            if ( p2 )
                p2[1] = 0;

            tagline = p;
        } else
            tagline.Add( tmpBuf );
    }
    fileh.Close();

    return(1);
}
#endif
#if SUPPORT_POSTSCRIPTS
static int checkPostscript ( int argc, char ** argv, int this_arg, int startargv )
{
    WinFile fileh;
    DWORD   sigsize;
    DWORD   dummy;

    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    this_arg++;
    if ( !fileh.OpenThisFile(argv[this_arg]) ) {
        printMsg( "Error reading P.S. file %s, postscript will not be added\n", argv[this_arg] );
        return(1);
    }

    sigsize = fileh.GetSize();
    postscript.Alloc( sigsize + 1 );
    postscript.SetLength( sigsize );

    if ( !fileh.ReadThisFile(postscript.Get(), sigsize, &dummy, NULL) ) {
        printMsg( "Error reading P.S. file %s, postscript will not be added\n", argv[this_arg] );
        postscript.Free();
    }
    else
        *postscript.GetTail() = 0;

    fileh.Close();
    return(1);
}
#endif

static int checkUserAgent ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    includeUserAgent = TRUE;

    return(0);
}


char          disableMPS    = FALSE;

#if SUPPORT_MULTIPART

unsigned long multipartSize = 0;

// The MultiPart message size value is per 1000 bytes.  e.g. 900 would be 900,000 bytes.

static int checkMultiPartSize ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( argc ) {
        long mps = atol( argv[this_arg+1] );
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


static int checkDisableMPS ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    this_arg  = this_arg;
    startargv = startargv;

    disableMPS = TRUE;

    return(0);
}
#endif


static int checkUnDisROption ( int argc, char ** argv, int this_arg, int startargv )
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
static int checkUserContType ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    argv      = argv;
    startargv = startargv;

    userContentType.Add( argv[this_arg+1] );
    return(1);
}


static int checkMaxNames ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    int maxn = atoi( argv[this_arg+1] );

    if ( maxn > 0 )
        maxNames = maxn;

    return(1);
}

static int checkCommentChar ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    if ( strlen( argv[++this_arg] ) == 1 ) {
        if ( argv[this_arg][0] != '-' )
            commentChar = argv[this_arg][0];

        return(1);
    }

    return(0);
}

static int checkDelayTime ( int argc, char ** argv, int this_arg, int startargv )
{
    argc      = argc;   // For eliminating compiler warnings.
    startargv = startargv;

    int timeDelay = atoi( argv[this_arg+1] );

    if ( timeDelay > 0 )
        delayBetweenMsgs = timeDelay;

    return(1);
}
#endif

#if INCLUDE_SUPERDEBUG
static int checkSuperDebug ( int argc, char ** argv, int this_arg, int startargv )
{
    superDebug = TRUE;
    return checkDebugMode( argc, argv, this_arg, startargv );
}

static int checkSuperDebugT ( int argc, char ** argv, int this_arg, int startargv )
{
    superDebug = 2;
    return checkDebugMode( argc, argv, this_arg, startargv );
}
#endif

#define CGI_HIDDEN  (char *)1

/*    optionString     szCgiEntry       preProcess                       usageText
                                                additionArgC           
                                                   initFunction        
*/                                                                     
_BLATOPTIONS blatOptionsList[] = {                                     
#if BLAT_LITE                                                          
    {           NULL ,             NULL, FALSE, 0, NULL                , "Blat lite v%s%s (build : %s %s)\n" },
#else                                                                  
    {           NULL ,             NULL, FALSE, 0, NULL                , "Blat "   "v%s%s (build : %s %s)\n" },
#endif                                                                 
    {           NULL ,             NULL, FALSE, 0, NULL                , "" },
#if BLAT_LITE | !INCLUDE_NNTP                                          
    {           NULL ,             NULL, FALSE, 0, NULL                , WIN_32_STR " console utility to send mail via SMTP" },
#else                                                                  
    {           NULL ,             NULL, FALSE, 0, NULL                , WIN_32_STR "  console utility to send mail via SMTP or post to usenet via NNTP" },
#endif                                                                 
    {           NULL ,             NULL, FALSE, 0, NULL                , "by P.Mendes,M.Neal,G.Vollant,T.Charron,T.Musson,H.Pesonen,A.Donchey,C.Hyde" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "  http://www.blat.net" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "syntax:" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "  Blat <filename> -to <recipient> [optional switches (see below)]" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "" },
#if BLAT_LITE                                                          
    {           NULL ,             NULL, FALSE, 0, NULL                , "  Blat -SaveSettings -sender <sender email addy> -server <server addr>" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "       [-port <port>] [-try <try>] [-u <login id>] [-pw <password>]" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "  or" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "  Blat -install <server addr> <sender's email addr> [<try>[<port>]] [-q]" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "  Blat -profile [-q]" },
#else                                                                  
    {           NULL ,             NULL, FALSE, 0, NULL                , "  Blat -SaveSettings -f <sender email addy> -server <server addr>" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "       [-port <port>] [-try <try>] [-profile <profile>]" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "       [-u <login id>] [-pw <password>]" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "  or" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "  Blat -install <server addr> <sender's addr> [<try>[<port>[<profile>]]] [-q]" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "  Blat -profile [-delete | \"<default>\"] [profile1] [profileN] [-q]" },
#endif                                                                 
    {           NULL ,             NULL, FALSE, 0, NULL                , "  Blat -h" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "-------------------------------- Installation ---------------------------------" },
#if BLAT_LITE                                                          
#else                                                                  
    { strInstallSMTP , CGI_HIDDEN      , FALSE, 1, checkInstallOption  , NULL },
#endif                                                                 
#if INCLUDE_NNTP                                                       
    { strInstallNNTP , CGI_HIDDEN      , FALSE, 1, checkNNTPInstall    , NULL },
#endif                                                                 
#if INCLUDE_POP3                                                       
    { strInstallPOP3 , CGI_HIDDEN      , FALSE, 1, checkPOP3Install    , NULL },
#endif                                                                 
#if INCLUDE_IMAP                                                       
    { strInstallIMAP , CGI_HIDDEN      , FALSE, 1, checkIMAPInstall    , NULL },
#endif                                                                 
/*                                                                     
-install[SMTP|NNTP|POP3|IMAP] <server addr> <sender email addr> [<try n times>
 */                                                                    
    { strSaveSettings, CGI_HIDDEN      , FALSE, 0, checkInstallOption  ,              "   : store common settings to the Windows Registry.  Takes the" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "                  same parameters as -install, and is only for SMTP settings." },
    { strInstall     , CGI_HIDDEN      , FALSE, 1, checkInstallOption  ,
#if BLAT_LITE                                                          
#else                                                                  
                                                                                 "[SMTP"
  #if INCLUDE_NNTP                                                     
                                                                                      "|NNTP"
  #endif                                                               
  #if INCLUDE_POP3                                                     
                                                                                           "|POP3"
  #endif                                                               
  #if INCLUDE_IMAP                                                     
                                                                                                "|IMAP"
  #endif                                                               
                                                                                                     "]"
#endif                                                                 
                                                                                                      " <server addr> <sender email addr> [<try n times>"
#if BLAT_LITE                                                          
#else                                                                  
                                                                                                                                                       },
    {           NULL ,             NULL, FALSE, 0, NULL                , "                [<port> [<profile> [<username> [<password>]]]"
#endif                                                                 
                                                                                                 "]]" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "                : set server, sender, number of tries and port"
#if BLAT_LITE                                                          
#else                                                                  
                                                                                                                                       " for profile"
#endif                                                                 
                                                                                                                                                      },
    {           NULL ,             NULL, FALSE, 0, NULL                , "                  (<try n times> and <port> may be replaced by '-')" },
#if INCLUDE_NNTP | INCLUDE_POP3 | INCLUDE_IMAP                         
    {           NULL ,             NULL, FALSE, 0, NULL                , "                  port defaults are SMTP=25"
  #if INCLUDE_NNTP                                                     
                                                                                                                    ", NNTP=119"
  #endif                                                               
  #if INCLUDE_POP3                                                     
                                                                                                                              ", POP3=110"
  #endif                                                               
  #if INCLUDE_IMAP                                                     
                                                                                                                                        ", IMAP=143"
  #endif                                                               
                                                                                                                                           },
#else                                                                  
    {           NULL ,             NULL, FALSE, 0, NULL                , "                  port default is 25" },
#endif                                                                 
#if BLAT_LITE                                                          
#else                                                                  
    {           NULL ,             NULL, FALSE, 0, NULL                , "                  default profile can be specified with a '-'" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "                  username and/or password may be stored to the registry" },
#endif                                                                 
    {           NULL ,             NULL, FALSE, 0, NULL                , "                  order of options is specific" },
#if INCLUDE_NNTP                                                       
    {           NULL ,             NULL, FALSE, 0, NULL                , "                  use -installNNTP for storing NNTP information" },
#endif                                                                 
#if INCLUDE_POP3                                                       
    {           NULL ,             NULL, FALSE, 0, NULL                , "                  use -installPOP3 for storing POP3 information" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "                      (sender and try are ignored, use '-' in place of these)" },
#endif                                                                 
#if INCLUDE_IMAP                                                       
    {           NULL ,             NULL, FALSE, 0, NULL                , "                  use -installIMAP for storing IMAP information" },
    {           NULL ,             NULL, FALSE, 0, NULL                , "                      (sender and try are ignored, use '-' in place of these)" },
#endif                                                                 
    {           NULL ,             NULL, 0    , 0, NULL                , "" },
    {           NULL ,             NULL, 0    , 0, NULL                , "--------------------------------- The Basics ----------------------------------" },
    {           NULL ,             NULL, 0    , 0, NULL                , "<filename>      : file with the message body to be sent" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  if your message body is on the command line, use a hyphen (-)" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  as your first argument, and -body followed by your message" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  if your message will come from the console/keyboard, use the" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  hyphen as your first argument, but do not use -body option." },
#if BLAT_LITE                                                          
#else                                                                  
    { "-@"           ,             NULL, TRUE , 1, checkOptionFile     , NULL },
    { "-optionfile"  ,             NULL, TRUE , 1, checkOptionFile     , NULL },
    { "-of"          ,             NULL, TRUE , 1, checkOptionFile     ,    " <file>      : text file containing more options (also -optionfile)" },
#endif                                                                 
    { "-t"           ,             NULL, FALSE, 1, checkToOption       , NULL },
    { "-to"          ,             NULL, FALSE, 1, checkToOption       ,    " <recipient> : recipient list (also -t) (comma separated)" },
#if BLAT_LITE                                                          
#else                                                                  
    { "-tf"          ,             NULL, FALSE, 1, checkToFileOption   ,    " <file>      : recipient list filename" },
#endif                                                                 
    { "-c"           ,             NULL, FALSE, 1, checkCcOption       , NULL },
    { "-cc"          ,             NULL, FALSE, 1, checkCcOption       ,    " <recipient> : carbon copy recipient list (also -c) (comma separated)" },
#if BLAT_LITE                                                          
#else                                                                  
    { "-cf"          ,             NULL, FALSE, 1, checkCcFileOption   ,    " <file>      : cc recipient list filename" },
#endif                                                                 
    { "-b"           ,             NULL, FALSE, 1, checkBccOption      , NULL },
    { "-bcc"         ,             NULL, FALSE, 1, checkBccOption      ,     " <recipient>: blind carbon copy recipient list (also -b)" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  (comma separated)" },
#if BLAT_LITE                                                          
#else                                                                  
    { "-bf"          ,             NULL, FALSE, 1, checkBccFileOption  ,    " <file>      : bcc recipient list filename" },
    { "-maxNames"    ,             NULL, FALSE, 1, checkMaxNames       ,          " <x>   : send to groups of <x> number of recipients" },
#endif                                                                 
    { "-ur"          ,             NULL, FALSE, 0, checkUnDisROption   ,    "             : set To: header to Undisclosed Recipients if not using the" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  -to and -cc options" },
    { "-s"           ,             NULL, FALSE, 1, checkSubjectOption  , NULL },
    { "-subject"     ,             NULL, FALSE, 1, checkSubjectOption  ,         " <subj> : subject line, surround with quotes to include spaces(also -s)" },
    { "-ss"          ,             NULL, FALSE, 0, checkSkipSubject    ,    "             : suppress subject line if not defined" },
#if BLAT_LITE                                                          
#else                                                                  
    { "-sf"          ,             NULL, FALSE, 1, checkSubjectFile    ,    " <file>      : file containing subject line" },
#endif                                                                 
    { "-bodyF"       ,             NULL, FALSE, 1, checkBodyFile       ,       " <file>   : file containing the message body" },
    { "-body"        ,             NULL, FALSE, 1, checkBodyText       ,      " <text>    : message body, surround with quotes (\") to include spaces" },
#if SUPPORT_SIGNATURES                                                 
    { "-sigfile"     ,             NULL, FALSE, 1, checkSigFile        , NULL },
    { "-sig"         ,             NULL, FALSE, 1, checkSigFile        ,     " <file>     : text file containing your email signature" },
#endif                                                                 
#if SUPPORT_TAGLINES                                                   
    { "-tagfile"     ,             NULL, FALSE, 1, checkTagFile        , NULL },
    { "-tag"         ,             NULL, FALSE, 1, checkTagFile        ,     " <file>     : text file containing taglines, to be randomly chosen" },
#endif                                                                 
#if SUPPORT_POSTSCRIPTS                                                
    { "-ps"          ,             NULL, FALSE, 1, checkPostscript     ,    " <file>      : final message text, possibly for unsubscribe instructions" },
#endif                                                                 
    {           NULL ,             NULL, 0    , 0, NULL                , "" },
    {           NULL ,             NULL, 0    , 0, NULL                , "----------------------------- Registry overrides ------------------------------" },
#if BLAT_LITE                                                          
#else                                                                  
    { strP           ,             NULL, TRUE , 1, checkProfileToUse   ,   " <profile>    : send with server, user, and port defined in <profile>" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                : use username and password if defined in <profile>" },
#endif                                                                 
    { strProfile     , CGI_HIDDEN      , FALSE, 0, checkProfileEdit    ,         "        : list all profiles in the Registry" },
    { strServer      ,             NULL, FALSE, 1, checkServerOption   ,        " <addr>  : specify SMTP server to be used (optionally, addr:port)" },
#if BLAT_LITE                                                          
#else                                                                  
    { strServerSMTP  , CGI_HIDDEN      , FALSE, 1, checkServerOption   ,            " <addr>" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                : same as -server" },
  #if INCLUDE_NNTP                                                     
    { strServerNNTP  , CGI_HIDDEN      , FALSE, 1, checkNNTPSrvr       ,            " <addr>" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                : specify NNTP server to be used (optionally, addr:port)" },
  #endif                                                               
  #if INCLUDE_POP3                                                     
    { strServerPOP3  , CGI_HIDDEN      , FALSE, 1, checkPOP3Srvr       ,            " <addr>" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                : specify POP3 server to be used (optionally, addr:port)" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  when POP3 access is required before sending email" },
  #endif                                                               
  #if INCLUDE_IMAP                                                     
    { strServerIMAP  , CGI_HIDDEN      , FALSE, 1, checkIMAPSrvr       ,            " <addr>" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                : specify IMAP server to be used (optionally, addr:port)" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  when IMAP access is required before sending email" },
  #endif                                                               
#endif                                                                 
    { strF           , "sender"        , FALSE, 1, checkFromOption     ,   " <sender>     : override the default sender address (must be known to server)" },
    { strI           , "from"          , FALSE, 1, checkImpersonate    ,   " <addr>       : a 'From:' address, not necessarily known to the server" },
    { strPort        ,             NULL, FALSE, 1, checkPortOption     ,      " <port>    : port to be used on the SMTP server, defaults to SMTP (25)" },
#if BLAT_LITE                                                          
#else                                                                  
    { strPortSMTP    , CGI_HIDDEN      , FALSE, 1, checkPortOption     ,          " <port>: same as -port" },
  #if INCLUDE_NNTP                                                     
    { strPortNNTP    , CGI_HIDDEN      , FALSE, 1, checkNNTPPort       ,          " <port>: port to be used on the NNTP server, defaults to NNTP (119)" },
  #endif                                                               
  #if INCLUDE_POP3                                                     
    { strPortPOP3    , CGI_HIDDEN      , FALSE, 1, checkPOP3Port       ,          " <port>: port to be used on the POP3 server, defaults to POP3 (110)" },
  #endif                                                               
  #if INCLUDE_IMAP                                                     
    { strPortIMAP    , CGI_HIDDEN      , FALSE, 1, checkIMAPPort       ,          " <port>: port to be used on the IMAP server, defaults to IMAP (110)" },
  #endif                                                               
#endif                                                                 
    { strUsername    ,             NULL, FALSE, 1, checkUserIDOption   , NULL },
    { strU           ,             NULL, FALSE, 1, checkUserIDOption   ,   " <username>   : username for AUTH LOGIN (use with -pw)" },
#if SUPPORT_GSSAPI                                                     
    {           NULL ,             NULL, 0    , 0, NULL                , "                  or for AUTH GSSAPI with -k" },
#endif                                                                 
    { strPassword    ,             NULL, FALSE, 1, checkPwdOption      , NULL },
    { strPwd         ,             NULL, FALSE, 1, checkPwdOption      , NULL },
    { strPw          ,             NULL, FALSE, 1, checkPwdOption      ,    " <password>  : password for AUTH LOGIN (use with -u)" },
#if INCLUDE_POP3                                                       
    { strPop3Username,             NULL, FALSE, 1, checkPop3UIDOption  , NULL },
    { strPop3U       ,             NULL, FALSE, 1, checkPop3UIDOption  ,    " <username>  : username for POP3 LOGIN (use with -ppw)" },
    { strPop3Password,             NULL, FALSE, 1, checkPop3PwdOption  , NULL },
    { strPop3Pw      ,             NULL, FALSE, 1, checkPop3PwdOption  ,     " <password> : password for POP3 LOGIN (use with -pu)" },
#endif                                                                 
#if INCLUDE_IMAP                                                       
    { strImapUsername,             NULL, FALSE, 1, checkImapUIDOption  , NULL },
    { strImapU       ,             NULL, FALSE, 1, checkImapUIDOption  ,    " <username>  : username for IMAP LOGIN (use with -ppw)" },
    { strImapPassword,             NULL, FALSE, 1, checkImapPwdOption  , NULL },
    { strImapPw      ,             NULL, FALSE, 1, checkImapPwdOption  ,     " <password> : password for IMAP LOGIN (use with -pu)" },
#endif                                                                 
#if SUPPORT_GSSAPI                                                     
    { "-k"           ,             NULL, FALSE, 0, checkGssapiMutual   ,   "              : Use " MECHTYPE " mutual authentication and AUTH GSSAPI" },
    { "-kc"          ,             NULL, FALSE, 0, checkGssapiClient   ,    "             : Use " MECHTYPE " client-only authentication and AUTH GSSAPI" },
    { "-service"     ,             NULL, FALSE, 1, checkServiceName    ,         " <name> : Set GSSAPI service name (use with -k), default \"smtp@server\"" },
    { "-level"       ,             NULL, FALSE, 1, checkProtectionLevel,        "<lev>    : Set GSSAPI protection level to <lev>, which should be one of" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                : None, Integrity, or Privacy (default GSSAPI level is Privacy)"},
#endif                                                                 
#if BLAT_LITE                                                          
#else                                                                  
    { "-nomd5"       ,             NULL, FALSE, 0, checkBypassCramMD5  ,       "          : Do NOT use CRAM-MD5 authentication.  Use this in cases where"},
    {           NULL ,             NULL, 0    , 0, NULL                , "                  the server's CRAM-MD5 is broken, such as Network Solutions."},
#endif                                                                 
    {           NULL ,             NULL, 0    , 0, NULL                , "" },
    {           NULL ,             NULL, 0    , 0, NULL                , "---------------------- Miscellaneous RFC header switches ----------------------" },
#if BLAT_LITE                                                          
#else                                                                  
    { "-o"           ,             NULL, FALSE, 1, checkOrgOption      , NULL },
    { "-org"         ,             NULL, FALSE, 1, checkOrgOption      , NULL },
    { "-organization", "organisation"  , FALSE, 1, checkOrgOption      ,              " <organization>" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                : Organization field (also -o and -org)" },
#endif                                                                 
    { "-ua"          ,             NULL, FALSE, 0, checkUserAgent      ,    "             : include User-Agent header line instead of X-Mailer" },
#if BLAT_LITE                                                          
#else                                                                  
    { "-x"           , "xheader"       , FALSE, 1, checkXHeaders       ,   " <X-Header: detail>" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                : custom 'X-' header.  eg: -x \"X-INFO: Blat is Great!\"" },
    { "-noh"         ,             NULL, FALSE, 0, checkNoHeaders      ,     "            : prevent X-Mailer/User-Agent header from showing Blat homepage" },
    { "-noh2"        ,             NULL, FALSE, 0, checkNoHeaders2     ,      "           : prevent X-Mailer header entirely" },
    { "-d"           , "notify"        , FALSE, 0, checkDisposition    ,   "              : request disposition notification" },
    { "-r"           , "requestreceipt", FALSE, 0, checkReadReceipt    ,   "              : request return receipt" },
#endif                                                                 
    { "-charset"     ,             NULL, FALSE, 1, checkCharset        ,         " <cs>   : user defined charset.  The default is iso-8859-1" },
#if BLAT_LITE                                                          
#else                                                                  
    { "-a1"          ,             NULL, FALSE, 1, checkA1Headers      ,    " <header>    : add custom header line at the end of the regular headers" },
    { "-a2"          ,             NULL, FALSE, 1, checkA2Headers      ,    " <header>    : same as -a1, for a second custom header line" },
    { "-dsn"         ,             NULL, FALSE, 1, checkDeliveryStat   ,     " <nsfd>     : use Delivery Status Notifications (RFC 3461)" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  n = never, s = successful, f = failure, d = delayed" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  can be used together, however N takes precedence" },
    { "-hdrencb"     ,             NULL, FALSE, 0, checkHdrEncB        ,         "        : use base64 for encoding headers, if necessary" },
    { "-hdrencq"     ,             NULL, FALSE, 0, checkHdrEncQ        ,         "        : use quoted-printable for encoding headers, if necessary" },
#endif                                                                 
    { "-priority"    ,             NULL, FALSE, 1, checkPriority       ,          " <pr>  : set message priority 0 for low, 1 for high" },
    { "-sensitivity" ,             NULL, FALSE, 1, checkSensitivity    ,             " <s>: set message sensitivity 0 for personal, 1 for private," },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  2 for company-confidential" },
    {           NULL ,             NULL, 0    , 0, NULL                , "" },
    {           NULL ,             NULL, 0    , 0, NULL                , "----------------------- Attachment and encoding options -----------------------" },
    { "-attach"      , CGI_HIDDEN      , FALSE, 1, checkBinFileAttach  ,        " <file>  : attach binary file(s) to message (filenames comma separated)" },
    { "-attacht"     , CGI_HIDDEN      , FALSE, 1, checkTxtFileAttach  ,         " <file> : attach text file(s) to message (filenames comma separated)" },
    { "-attachi"     , CGI_HIDDEN      , FALSE, 1, checkInlineAttach   ,         " <file> : attach text file(s) as INLINE (filenames comma separated)" },
#if BLAT_LITE                                                          
#else                                                                  
    { "-embed"       , CGI_HIDDEN      , FALSE, 1, checkBinFileEmbed   ,       " <file>   : embed file(s) in HTML.  Object tag in HTML must specify" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  content-id using cid: tag.  eg: <img src=\"cid:image.jpg\">" },
    { "-af"          ,             NULL, FALSE, 1, checkBinFileAttFil  ,    " <file>      : file containing list of binary file(s) to attach (comma" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  separated)" },
    { "-atf"         ,             NULL, FALSE, 1, checkTxtFileAttFil  ,     " <file>     : file containing list of text file(s) to attach (comma" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  separated)" },
    { "-aef"         ,             NULL, FALSE, 1, checkEmbFileAttFil  ,     " <file>     : file containing list of embed file(s) to attach (comma" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  separated)" },
    { "-base64"      ,             NULL, FALSE, 0, checkBase64Enc      ,        "         : send binary files using base64 (binary MIME)" },
    { "-uuencodel"   ,             NULL, FALSE, 0, checkLongUUEncode   , NULL },
    { "-uuencode"    ,             NULL, FALSE, 0, checkUUEncode       ,          "       : send binary files UUEncoded" },
    { "-enriched"    ,             NULL, FALSE, 0, checkEnriched       ,          "       : send an enriched text message (Content-Type=text/enriched)" },
    { "-unicode"     ,             NULL, FALSE, 0, checkUnicode        ,         "        : message body is in 16- or 32-bit Unicode format" },
#endif                                                                 
    { "-html"        ,             NULL, FALSE, 0, checkHTML           ,      "           : send an HTML message (Content-Type=text/html)" },
#if BLAT_LITE                                                          
#else                                                                  
    { "-alttext"     ,             NULL, FALSE, 1, checkAltText        ,         " <text> : plain text for use as alternate text" },
    { "-althtml"     ,             NULL, FALSE, 1, checkAltTextFile    , NULL },
    { "-htmaltf"     ,             NULL, FALSE, 1, checkAltTextFile    , NULL },
    { "-alttextf"    ,             NULL, FALSE, 1, checkAltTextFile    ,          " <file>: plain text file for use as alternate text" },
#endif                                                                 
    { "-mime"        ,             NULL, FALSE, 0, checkMime           ,      "           : MIME Quoted-Printable Content-Transfer-Encoding" },
#if BLAT_LITE                                                          
#else                                                                  
    { "-8bitmime"    ,             NULL, FALSE, 0, check8bitMime       ,          "       : ask for 8bit data support when sending MIME" },
#endif                                                                 
#if SUPPORT_YENC                                                       
    { "-yenc"        ,             NULL, FALSE, 0, check_yEnc          ,      "           : send binary files encoded with yEnc" },
#endif                                                                 
#if SUPPORT_MULTIPART                                                  
    { "-mps"         ,             NULL, FALSE, 0, checkMultiPartSize  , NULL },
    { "-multipart"   ,             NULL, FALSE, 0, checkMultiPartSize  ,           " <size>" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                : send multipart messages, breaking attachments on <size>" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  KB boundaries, where <size> is per 1000 bytes" },
    { "-nomps"       ,             NULL, FALSE, 0, checkDisableMPS     ,       "          : do not allow multipart messages" },
#endif                                                                 
#if BLAT_LITE                                                          
#else                                                                  
    { "-contentType" ,             NULL, FALSE, 1, checkUserContType   ,             " <string>" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                : use <string> in the ContentType header for attachments that" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  do not have a registered content type for the extension" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  For example: -contenttype \"text/calendar\"" },
#endif                                                                 
    {           NULL ,             NULL, 0    , 0, NULL                , "" },
#if INCLUDE_NNTP                                                       
    {           NULL ,             NULL, 0    , 0, NULL                , "---------------------------- NNTP specific options ----------------------------" },
    { "-group"       , CGI_HIDDEN      , FALSE, 1, checkNNTPGroups     , NULL },
    { "-groups"      , CGI_HIDDEN      , FALSE, 1, checkNNTPGroups     ,        " <usenet groups>" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                : list of newsgroups (comma separated)" },
    {           NULL ,             NULL, 0    , 0, NULL                , "" },
#endif                                                                 
    {           NULL ,             NULL, 0    , 0, NULL                , "-------------------------------- Other options --------------------------------" },
#if INCLUDE_POP3                                                       
    { "-xtndxmit"    ,             NULL, 0    , 0, checkXtndXmit       ,          "       : Attempt to use POP3 to transmit when accessing POP3 first" },
#endif                                                                 
    { "/h"           , CGI_HIDDEN      , FALSE, 0, checkHelp           , NULL },
    { "/?"           , CGI_HIDDEN      , FALSE, 0, checkHelp           , NULL },
    { "-?"           , CGI_HIDDEN      , FALSE, 0, checkHelp           , NULL },
    { "-help"        , CGI_HIDDEN      , FALSE, 0, checkHelp           , NULL },
    { "-h"           , CGI_HIDDEN      , FALSE, 0, checkHelp           ,   "              : displays this help (also -?, /?, -help or /help)" },
    { "-q"           ,             NULL, TRUE , 0, checkQuietMode      ,   "              : suppresses all output to the screen" },
    { "-debug"       ,             NULL, TRUE , 0, checkDebugMode      ,       "          : echoes server communications to a log file or screen" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  (overrides -q if echoes to the screen)" },
    { "-log"         ,             NULL, TRUE , 1, checkLogMessages    ,     " <file>     : log everything but usage to <file>" },
    { "-timestamp"   ,             NULL, FALSE, 0, checkTimestamp      ,           "      : when -log is used, a timestamp is added to each log line" },
    { "-overwritelog",             NULL, TRUE , 0, checkLogOverwrite   ,              "   : when -log is used, overwrite the log file" },
    { "-ti"          , "timeout"       , FALSE, 1, checkTimeout        ,    " <n>         : set timeout to 'n' seconds.  Blat will wait 'n' seconds for" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  server responses" },
    { strTry         ,             NULL, FALSE, 1, checkAttempts       ,     " <n times>  : how many times blat should try to send (1 to 'INFINITE')" },
    { "-binary"      ,             NULL, FALSE, 0, checkFixPipe        ,        "         : do not convert ASCII | (pipe, 0x7c) to CrLf in the message" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  body" },
    { "-hostname"    ,             NULL, FALSE, 1, checkHostname       ,          " <hst> : select the hostname used to send the message via SMTP" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  this is typically your local machine name" },
    { "-penguin"     ,             NULL, FALSE, 0, checkRaw            , NULL },
    { "-raw"         ,             NULL, FALSE, 0, checkRaw            ,     "            : do not add CR/LF after headers" },
#if BLAT_LITE                                                          
#else                                                                  
    { "-delay"       ,             NULL, FALSE, 1, checkDelayTime      ,       " <x>      : wait x seconds between messages being sent when used with" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  -maxnames"
  #if SUPPORT_MULTIPART                                                
                                                                                  " or -multipart"
  #endif                                                               
                                                                                                   },
    { "-comment"     ,             NULL, TRUE , 1, checkCommentChar    ,         " <char> : use this character to mark the start of comments in" },
    {           NULL ,             NULL, 0    , 0, NULL                , "                  options files and recipient list files.  The default is ;" },
#endif                                                                 
#if INCLUDE_SUPERDEBUG                                                 
    { "-superdebug"  ,             NULL, TRUE , 0, checkSuperDebug     ,            "     : hex/ascii dump the data between Blat and the server" },
    { "-superdebugT" ,             NULL, TRUE , 0, checkSuperDebugT    ,             "    : ascii dump the data between Blat and the server" },
#endif                                                                 
    {           NULL ,             NULL, 0    , 0, NULL                , "-------------------------------------------------------------------------------" },
    {           NULL ,             NULL, 0    , 0, NULL                , "" },
    {           NULL ,             NULL, 0    , 0, NULL                , "Note that if the '-i' option is used, <sender> is included in 'Reply-to:'" },
    {           NULL ,             NULL, 0    , 0, NULL                , "and 'Sender:' fields in the header of the message." },
    {           NULL ,             NULL, 0    , 0, NULL                , "" },
    {           NULL ,             NULL, 0    , 0, NULL                , "Optionally, the following options can be used instead of the -f and -i" },
    {           NULL ,             NULL, 0    , 0, NULL                , "options:" },
    {           NULL ,             NULL, 0    , 0, NULL                , "" },
    { strMailFrom    ,             NULL, FALSE, 1, checkMailFrom       ,          " <addr>   The RFC 821 MAIL From: statement" },
    { strFrom        ,             NULL, FALSE, 1, checkWhoFrom        ,      " <addr>       The RFC 822 From: statement" },
    { "-replyto"     ,             NULL, FALSE, 1, checkReplyTo        ,         " <addr>    The RFC 822 Reply-To: statement" },
    { "-returnpath"  ,             NULL, FALSE, 1, checkReturnPath     ,            " <addr> The RFC 822 Return-Path: statement" },
    { strSender      ,             NULL, FALSE, 1, checkSender         ,        " <addr>     The RFC 822 Sender: statement" },
    {           NULL ,             NULL, 0    , 0, NULL                , "" },
    {           NULL ,             NULL, 0    , 0, NULL                , "For backward consistency, the -f and -i options have precedence over these" },
    {           NULL ,             NULL, 0    , 0, NULL                , "RFC 822 defined options.  If both -f and -i options are omitted then the" },
    {           NULL ,             NULL, 0    , 0, NULL                , "RFC 821 MAIL FROM statement will be defaulted to use the installation-defined" },
    {           NULL ,             NULL, 0    , 0, NULL                , "default sender address." },
    {           NULL ,             NULL, 0    , 0, NULL                , NULL } };


void printTitleLine( int quiet )
{
    static int titleLinePrinted = FALSE;
    char       tmpstr[1024];

    sprintf( tmpstr, blatOptionsList[0].usageText, blatVersion, blatVersionSuf, blatBuildDate, blatBuildTime );
    if ( !titleLinePrinted ) {
        if ( !quiet )
            printf( "%s\n", tmpstr );

        if ( logOut )
            printMsg( "%s", tmpstr );
    }

    titleLinePrinted = TRUE;
}

void print_usage_line( char * pUsageLine )
{
#if SUPPORT_GSSAPI
    char      * match;
    char        beginning[USAGE_LINE_SIZE];
    static BOOL have_mechtype = FALSE;
    static char mechtype[MECHTYPE_SIZE] = "UNKNOWN";

    //Make sure all lines about GSSAPI options contain "GSSAPI"
    if ( bSuppressGssOptionsAtRuntime && strstr(pUsageLine,"GSSAPI") )
        return;

    if ( strstr( pUsageLine, "GSSAPI" ) ) {
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
                printMsg("GssSession Error: %s\n",e.message());
                pGss = NULL;
                strcpy( mechtype,"UNAVAILABLE" );
            }

            have_mechtype = TRUE;
        }

        match = strstr(pUsageLine,MECHTYPE);
        if ( match ) {
            memcpy( beginning, pUsageLine, match - pUsageLine );
            beginning[match - pUsageLine] = 0;

            printf("%s%s%s", beginning, mechtype, match + strlen(MECHTYPE) );
            fflush(stdout);
            return;
        }
    }
#endif
    printf("%s", pUsageLine);
    fflush(stdout);
}


int printUsage( int optionPtr )
{
    int  i;
    char printLine[USAGE_LINE_SIZE];

    printTitleLine( FALSE );

    if ( !optionPtr ) {
        for ( i = 1; ;  ) {
            if ( !blatOptionsList[i].optionString &&
                 !blatOptionsList[i].preprocess   &&
                 !blatOptionsList[i].additionArgC &&
                 !blatOptionsList[i].initFunction &&
                 !blatOptionsList[i].usageText    )
                break;

            if ( blatOptionsList[i].usageText ) {
                sprintf( printLine, "%s%s\n",
                         blatOptionsList[i].optionString ? blatOptionsList[i].optionString : "",
                         blatOptionsList[i].usageText );
                print_usage_line( printLine );
            }

            if ( blatOptionsList[++i].usageText ) {
                if ( (blatOptionsList[i].usageText[0] == '-') && (blatOptionsList[i].usageText[1] == '-') )
                    break;
            }
        }
    } else
    if ( optionPtr == TRUE ) {
        for ( i = 1; ; i++ ) {
            if ( !blatOptionsList[i].optionString &&
                 !blatOptionsList[i].preprocess   &&
                 !blatOptionsList[i].additionArgC &&
                 !blatOptionsList[i].initFunction &&
                 !blatOptionsList[i].usageText    )
                break;

            if ( blatOptionsList[i].usageText ) {
                sprintf( printLine, "%s%s\n",
                         blatOptionsList[i].optionString ? blatOptionsList[i].optionString : "",
                         blatOptionsList[i].usageText );
                print_usage_line( printLine );
            }
        }
    } else {
        printf( "Blat found fault with: %s\n\n", blatOptionsList[optionPtr].optionString );
        for ( ; !blatOptionsList[optionPtr].usageText; optionPtr++ )
            ;

        for ( ; ; optionPtr++ ) {
            if ( !blatOptionsList[optionPtr].usageText    ||
                 !blatOptionsList[optionPtr].usageText[0] ||
                 (blatOptionsList[optionPtr].usageText[0] == '-') )
                break;

            sprintf( printLine, "%s%s\n",
                     blatOptionsList[optionPtr].optionString ? blatOptionsList[optionPtr].optionString : "",
                     blatOptionsList[optionPtr].usageText );
            print_usage_line( printLine );

            if ( blatOptionsList[optionPtr+1].optionString && blatOptionsList[optionPtr+1].optionString[0] == '-' )
                break;
        }
    }

    return 1;
}


int processOptions( int argc, char ** argv, int startargv, int preprocessing )
{
    int i, this_arg, retval;

    for ( i = 0; i < argc; i++ )
        if ( argv[i][0] == '/' )
            argv[i][0] = '-';

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
                 blatOptionsList[i].optionString = "";

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

#if INCLUDE_SUPERDEBUG
        if ( superDebug )
            printf( "Checking option %s\n", argv[this_arg] );
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

            if ( lstrcmpi(blatOptionsList[i].optionString, argv[this_arg]) == 0 ) {
                int next_arg;

                if ( _strnicmp(argv[this_arg], strInstall, 8 ) == 0 ) {
                    next_arg = argc;
                }
                else {
                    for ( next_arg = this_arg + 1; next_arg < argc; next_arg++ ) {
                        if ( next_arg == (this_arg + 1) )
                            if ( strcmp( argv[this_arg], "-profile" ) == 0 )
                                if ( strcmp( argv[next_arg], "-delete" ) == 0 )
                                    continue;

                        if ( argv[next_arg][0] == '-' )
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

                                    if ( lstrcmpi(blatOptionsList[check].optionString, argv[next_arg]) == 0 ) {
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

                        processedOptions.Add( "    " );
                        processedOptions.Add( argv[this_arg] );

                        // Remove the options we processed, so we do not attempt
                        // to process them a second time.
                        argv[this_arg][0] = '\0';
                        for ( ; retval; retval-- ) {
                            processedOptions.Add( ' ' );
                            processedOptions.Add( argv[++this_arg] );
                            argv[this_arg][0] = '\0';
                        }
                        processedOptions.Add( "\r\n" );
                    } else
                        this_arg = next_arg - 1;
                }
                else {
                    if ( !preprocessing ) {
                        processedOptions.Add( "    " );
                        processedOptions.Add( argv[this_arg] );
                        printMsg( "Blat saw and processed these options, and %s the last one...\n%s\n\n",
                                  "found fault with", processedOptions.Get() );
                        printMsg( "Not enough arguments supplied for option: %s\n",
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
            processedOptions.Add( "    " );
            processedOptions.Add( argv[this_arg] );
            printMsg( "Blat saw and processed these options, and %s the last one...\n%s\n\n",
                      "was confused by", processedOptions.Get() );
            printMsg( "Do not understand argument: %s\n",
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
