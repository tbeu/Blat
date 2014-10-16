/*
    reg.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blat.h"


extern void base64_encode(const unsigned char *in, int length, char *out, int inclCrLf);
extern int  base64_decode(const unsigned char *in, char *out);
extern void printMsg(const char *p, ... );              // Added 23 Aug 2000 Craig Morrison

extern char         SMTPHost[];
extern char         SMTPPort[];
extern const char * defaultSMTPPort;

#if INCLUDE_NNTP
extern char         NNTPHost[];
extern char         NNTPPort[];
extern const char * defaultNNTPPort;
#endif

#if INCLUDE_POP3
extern char         POP3Host[];
extern char         POP3Port[];
extern const char * defaultPOP3Port;
#endif
#if INCLUDE_IMAP
extern char         IMAPHost[];
extern char         IMAPPort[];
extern const char * defaultIMAPPort;
#endif

extern char         AUTHLogin[];
extern char         AUTHPassword[];
extern char         POP3Login[];
extern char         POP3Password[];
extern char         IMAPLogin[];
extern char         IMAPPassword[];
extern char         Try[];
extern char         Sender[];
extern char         loginname[];    // RFC 821 MAIL From. <loginname>. There are some inconsistencies in usage
extern char         quiet;

char                Profile[TRY_SIZE+1];

static const char   MainRegKey[]         = "SOFTWARE\\Public Domain\\Blat";

static const char   RegKeySMTPHost[]     = "SMTP server";
static const char   RegKeySMTPPort[]     = "SMTP Port";
#if INCLUDE_NNTP
static const char   RegKeyNNTPHost[]     = "NNTP server";
static const char   RegKeyNNTPPort[]     = "NNTP Port";
#endif
#if INCLUDE_POP3
static const char   RegKeyPOP3Host[]     = "POP3 server";
static const char   RegKeyPOP3Port[]     = "POP3 Port";
static const char   RegKeyPOP3Login[]    = "POP3 Login";
static const char   RegKeyPOP3Password[] = "POP3 Pwd";
#endif
#if INCLUDE_IMAP
static const char   RegKeyIMAPHost[]     = "IMAP server";
static const char   RegKeyIMAPPort[]     = "IMAP Port";
static const char   RegKeyIMAPLogin[]    = "IMAP Login";
static const char   RegKeyIMAPPassword[] = "IMAP Pwd";
#endif
static const char   RegKeyLogin[]        = "Login";
static const char   RegKeyPassword[]     = "Pwd";
static const char   RegKeySender[]       = "Sender";
static const char   RegKeyTry[]          = "Try";

// create registry entries for this program
static int CreateRegEntry( HKEY rootKeyLevel )
{
    HKEY   hKey1;
    DWORD  dwDisposition;
    LONG   lRetCode;
    char   strRegisterKey[256];
    char   tmpstr[SENDER_SIZE * 2];

    strcpy(strRegisterKey,MainRegKey);
    if ( Profile[0] != '\0' ) {
        strcat(strRegisterKey, "\\");
        strcat(strRegisterKey, Profile);
    }

    /* try to create the .INI file key */
    lRetCode = RegCreateKeyEx ( rootKeyLevel,
                                strRegisterKey,
                                0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey1, &dwDisposition
                              );

    /* if we failed, note it, and leave */
    if ( lRetCode != ERROR_SUCCESS ) {
        if ( ! quiet ) {
            if ( (lRetCode == ERROR_CANTWRITE) || (lRetCode == ERROR_ACCESS_DENIED) )
                printf ("Write access to the registry was denied.\n");
            else
                printf ("Error in creating blat key in the registry\n");
        }

        return(10);
    }

    if ( SMTPHost[0] ) {
        if ( !strcmp(SMTPHost,"-") ) {
            SMTPHost[0] = '\0';
            SMTPPort[0] = '\0';
        }

        /* try to set a section value */
        lRetCode = RegSetValueEx( hKey1, RegKeySMTPHost, 0, REG_SZ, (BYTE *) &SMTPHost[0], (DWORD)(strlen(SMTPHost)+1));

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ( "Error in setting SMTP server value in the registry\n");
            return(11);
        }

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeySMTPPort, 0, REG_SZ, (BYTE *) &SMTPPort[0], (DWORD)(strlen(SMTPPort)+1));

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ( "Error in setting port value in the registry\n");
            return(11);
        }

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyTry, 0, REG_SZ, (BYTE *) &Try[0], (DWORD)(strlen(Try)+1));

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ( "Error in setting number of try value in the registry\n");
            return(11);
        }

        /* try to set another section value */
        base64_encode((const unsigned char *)AUTHLogin, (int)strlen(AUTHLogin), tmpstr, FALSE);
        lRetCode = RegSetValueEx( hKey1, RegKeyLogin, 0, REG_SZ, (BYTE *) tmpstr, (DWORD)(strlen(tmpstr)+1));

        /* try to set another section value */
        base64_encode((const unsigned char *)AUTHPassword, (int)strlen(AUTHPassword), tmpstr, FALSE);
        lRetCode = RegSetValueEx( hKey1, RegKeyPassword, 0, REG_SZ, (BYTE *) tmpstr, (DWORD)(strlen(tmpstr)+1));

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeySender, 0, REG_SZ, (BYTE *) &Sender[0], (DWORD)(strlen(Sender)+1));

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ( "Error in setting sender address value in the registry\n");
            return(11);
        }
    }

#if INCLUDE_NNTP
    if ( NNTPHost[0] ) {
        if ( !strcmp(NNTPHost,"-") ) {
            NNTPHost[0] = '\0';
            NNTPPort[0] = '\0';
        }

        /* try to set a section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyNNTPHost, 0, REG_SZ, (BYTE *) &NNTPHost[0], (DWORD)(strlen(NNTPHost)+1));

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ( "Error in setting NNTP server value in the registry\n");
            return(11);
        }

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyNNTPPort, 0, REG_SZ, (BYTE *) &NNTPPort[0], (DWORD)(strlen(NNTPPort)+1));

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ( "Error in setting port value in the registry\n");
            return(11);
        }

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyTry, 0, REG_SZ, (BYTE *) &Try[0], (DWORD)(strlen(Try)+1));

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ( "Error in setting number of try value in the registry\n");
            return(11);
        }

        /* try to set another section value */
        base64_encode((const unsigned char *)AUTHLogin, (int)strlen(AUTHLogin), tmpstr, FALSE);
        lRetCode = RegSetValueEx( hKey1, RegKeyLogin, 0, REG_SZ, (BYTE *) tmpstr, (DWORD)(strlen(tmpstr)+1));

        /* try to set another section value */
        base64_encode((const unsigned char *)AUTHPassword, (int)strlen(AUTHPassword), tmpstr, FALSE);
        lRetCode = RegSetValueEx( hKey1, RegKeyPassword, 0, REG_SZ, (BYTE *) tmpstr, (DWORD)(strlen(tmpstr)+1));

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeySender, 0, REG_SZ, (BYTE *) &Sender[0], (DWORD)(strlen(Sender)+1));

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ( "Error in setting sender address value in the registry\n");
            return(11);
        }
    }
#endif

#if INCLUDE_POP3
    if ( POP3Host[0] ) {
        if ( !strcmp(POP3Host,"-") ) {
            POP3Host[0] = '\0';
            POP3Port[0] = '\0';
        }

        /* try to set a section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyPOP3Host, 0, REG_SZ, (BYTE *) &POP3Host[0], (DWORD)(strlen(POP3Host)+1));

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ( "Error in setting POP3 server value in the registry\n");
            return(11);
        }

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyPOP3Port, 0, REG_SZ, (BYTE *) &POP3Port[0], (DWORD)(strlen(POP3Port)+1));

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ( "Error in setting port value in the registry\n");
            return(11);
        }

        base64_encode((const unsigned char *)POP3Login, (int)strlen(POP3Login), tmpstr, FALSE);
        (void)RegSetValueEx( hKey1, RegKeyPOP3Login, 0, REG_SZ, (BYTE *) tmpstr, (DWORD)(strlen(tmpstr)+1));

        /* try to set another section value */
        base64_encode((const unsigned char *)POP3Password, (int)strlen(POP3Password), tmpstr, FALSE);
        (void)RegSetValueEx( hKey1, RegKeyPOP3Password, 0, REG_SZ, (BYTE *) tmpstr, (DWORD)(strlen(tmpstr)+1));
    }
#endif

#if INCLUDE_IMAP
    if ( IMAPHost[0] ) {
        if ( !strcmp(IMAPHost,"-") ) {
            IMAPHost[0] = '\0';
            IMAPPort[0] = '\0';
        }

        /* try to set a section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyIMAPHost, 0, REG_SZ, (BYTE *) &IMAPHost[0], (DWORD)(strlen(IMAPHost)+1));

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ( "Error in setting IMAP server value in the registry\n");
            return(11);
        }

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyIMAPPort, 0, REG_SZ, (BYTE *) &IMAPPort[0], (DWORD)(strlen(IMAPPort)+1));

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ( "Error in setting port value in the registry\n");
            return(11);
        }

        base64_encode((const unsigned char *)IMAPLogin, (int)strlen(IMAPLogin), tmpstr, FALSE);
        (void)RegSetValueEx( hKey1, RegKeyIMAPLogin, 0, REG_SZ, (BYTE *) tmpstr, (DWORD)(strlen(tmpstr)+1));

        /* try to set another section value */
        base64_encode((const unsigned char *)IMAPPassword, (int)strlen(IMAPPassword), tmpstr, FALSE);
        (void)RegSetValueEx( hKey1, RegKeyIMAPPassword, 0, REG_SZ, (BYTE *) tmpstr, (DWORD)(strlen(tmpstr)+1));
    }
#endif

    return(0);
}

// create registry entries for this program
int CreateRegEntry( int useHKCU )
{
    int retval;

    retval = 1;     // assume useHKCU set, so the 2nd if() works.
    if ( !useHKCU )
        retval = CreateRegEntry( HKEY_LOCAL_MACHINE );

    if ( retval )
        retval = CreateRegEntry( HKEY_CURRENT_USER );

    return retval;
}

/**************************************************************
 **************************************************************/

void ShowRegHelp( void )
{
    printMsg( "BLAT PROFILE EDITOR\n");
    printMsg( "To modify SMTP: blat -install SMTPHost Sender [Try [Port [Profile [Login name\n");
    printMsg( "                     [Password]]]]]\n");
    printMsg( "            or: blat -installSMTP SMTPHost Sender [Try [Port [Profile\n");
    printMsg( "                     [Login name [Password]]]]]\n");
#if INCLUDE_NNTP
    printMsg( "To modify NNTP: blat -installNNTP NNTPHost Sender [Try [Port [Profile\n");
    printMsg( "                     [Login name [Password]]]]]\n");
#endif
#if INCLUDE_POP3
    printMsg( "To modify POP3: blat -installPOP3 POP3Host - - [Port [Profile\n");
    printMsg( "                     [Login name [Password]]]]\n");
#endif
#if INCLUDE_IMAP
    printMsg( "To modify IMAP: blat -installIMAP IMAPHost - - [Port [Profile\n");
    printMsg( "                     [Login name [Password]]]]\n");
#endif
    printMsg( "To delete:      blat -profile -delete Profile\n");
    printMsg( "Profiles are listed as in the -install option:\n");
    printMsg( "Host Sender Try Port Profile login_name password\n");
    printMsg( "\n");
}


static int DeleteRegTree( HKEY rootKeyLevel, char * pstrProfile )
{
    HKEY     hKey1=NULL;
    DWORD    dwBytesRead;
    LONG     lRetCode;
    DWORD    dwIndex;                               // index of subkey to enumerate
    FILETIME lftLastWriteTime;
    char   * newProfile;

    // open the registry key in read mode
    lRetCode = RegOpenKeyEx( rootKeyLevel, pstrProfile, 0, KEY_ALL_ACCESS, &hKey1 );
    dwIndex = 0;
    for ( ; lRetCode == 0; ) {
        dwBytesRead = sizeof(Profile);
        lRetCode = RegEnumKeyEx(  hKey1,     // handle of key to enumerate
                                  dwIndex++, // index of subkey to enumerate
                                  Profile,   // address of buffer for subkey name
                                  &dwBytesRead,    // address for size of subkey buffer
                                  NULL,      // reserved
                                  NULL,      // address of buffer for class string
                                  NULL,      // address for size of class buffer
                                  &lftLastWriteTime
                                  // address for time key last written to);
                               );
        if ( lRetCode == ERROR_NO_MORE_ITEMS ) {
            lRetCode = 0;
            break;
        }

        if ( lRetCode == 0 ) {
            newProfile = new char[strlen(pstrProfile)+strlen(Profile)+2];
            sprintf(newProfile,"%s\\%s",pstrProfile,Profile);
            lRetCode = DeleteRegTree( rootKeyLevel, newProfile );
            delete [] newProfile;
        }
        if ( lRetCode == 0 ) {
            lRetCode = RegDeleteKey( hKey1, Profile );
            if ( lRetCode != ERROR_SUCCESS ) {
                if ( ! quiet )  printf ( "Error in deleting profile %s in the registry\n", pstrProfile);
                return(11);
            }
            dwIndex--;
        }
    }

    RegCloseKey(hKey1);
    return lRetCode;
}

// Delete registry entries for this program
int DeleteRegEntry( char * pstrProfile, int useHKCU )
{
    HKEY   rootKeyLevel;
    HKEY   hKey1=NULL;
    LONG   lRetCode;
    char   strRegisterKey[256];


    if ( useHKCU )
        rootKeyLevel = HKEY_CURRENT_USER;
    else
        rootKeyLevel = HKEY_LOCAL_MACHINE;

    strcpy(strRegisterKey,MainRegKey);

    if ( !strcmp(pstrProfile,"<default>") ) {
        DWORD dwBytesRead;
        DWORD dwIndex;      // index of subkey to enumerate

        lRetCode = RegOpenKeyEx ( rootKeyLevel, strRegisterKey, 0, KEY_ALL_ACCESS, &hKey1 );

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ("Error in finding blat default profile in the registry\n");
            return(10);
        }

        dwIndex = 0;
        for ( ; lRetCode == 0; ) {
            dwBytesRead = sizeof(Profile);
            lRetCode = RegEnumValue(  hKey1,     // handle of value to enumerate
                                      dwIndex++, // index of subkey to enumerate
                                      Profile,   // address of buffer for subkey name
                                      &dwBytesRead,    // address for size of subkey buffer
                                      NULL,      // reserved
                                      NULL,      // address of buffer for key type
                                      NULL,      // address of buffer for value data
                                      NULL       // address for length of value data );
                                   );
            if ( lRetCode == ERROR_NO_MORE_ITEMS ) {
                lRetCode = 0;
                break;
            }

            if ( lRetCode == 0 ) {
                lRetCode = RegDeleteValue( hKey1, Profile );
                if ( lRetCode != ERROR_SUCCESS ) {
                    if ( ! quiet )  printf ( "Error in deleting profile %s in the registry\n", pstrProfile);
                    return(11);
                }
                dwIndex--;
            }
        }

        RegCloseKey(hKey1);
    } else
    if ( !strcmp(pstrProfile,"<all>") ) {
        DeleteRegTree( rootKeyLevel, strRegisterKey );

        // Attempt to delete the main Blat key.
        lRetCode = RegOpenKeyEx ( rootKeyLevel, strRegisterKey, 0, KEY_ALL_ACCESS, &hKey1 );
        if ( lRetCode == ERROR_SUCCESS ) {
            lRetCode = RegDeleteKey( hKey1, "" );
            RegCloseKey(hKey1);
            *strrchr(strRegisterKey,'\\') = 0;

            // Attempt to delete the Public Domain key
            lRetCode = RegOpenKeyEx ( rootKeyLevel, strRegisterKey, 0, KEY_ALL_ACCESS, &hKey1 );
            if ( lRetCode == ERROR_SUCCESS ) {
                lRetCode = RegDeleteKey( hKey1, "" );
                RegCloseKey(hKey1);
            }
        }
    } else
    if ( pstrProfile[0] != '\0' ) {
        strcat(strRegisterKey, "\\");
        strcat(strRegisterKey, pstrProfile);

        lRetCode = DeleteRegTree( rootKeyLevel, strRegisterKey );

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ( "Error in deleting profile %s in the registry\n", pstrProfile);
            return(11);
        }

        lRetCode = RegOpenKeyEx ( rootKeyLevel, strRegisterKey, 0, KEY_ALL_ACCESS, &hKey1 );
        if ( lRetCode == ERROR_SUCCESS ) {
            lRetCode = RegDeleteKey( hKey1, "" );
            RegCloseKey(hKey1);
        }
        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( ! quiet )  printf ("Error in finding blat profile %s in the registry\n", pstrProfile);
            return(10);
        }

    }

    return(0);
}

/**************************************************************
 **************************************************************/

// get the registry entries for this program
static int GetRegEntryKeyed( HKEY rootKeyLevel, char *pstrProfile )
{
    HKEY   hKey1=NULL;
    DWORD  dwType;
    DWORD  dwBytesRead;
    LONG   lRetCode;
    char * register_key;
    char   tmpstr[(SENDER_SIZE * 2) + 4];
    int    retval;

    hKey1 = NULL;

    if ( pstrProfile && *pstrProfile ) {
        register_key = new char[ strlen(MainRegKey) + strlen(pstrProfile) + 2 ];
        sprintf( register_key, "%s\\%s", MainRegKey, pstrProfile );
    } else {
        register_key = new char[ strlen(MainRegKey) + 1 ];
        strcpy(register_key, MainRegKey);
    }

    // open the registry key in read mode
    lRetCode = RegOpenKeyEx( rootKeyLevel, register_key, 0, KEY_READ, &hKey1 );
    delete [] register_key;

    if ( lRetCode != ERROR_SUCCESS ) {
//       printMsg( "Failed to open registry key for Blat\n" );
        if ( pstrProfile && *pstrProfile )
            return GetRegEntryKeyed( rootKeyLevel, NULL );

        return(12);
    }

    // set the size of the buffer to contain the data returned from the registry
    // thanks to Beverly Brown "beverly@datacube.com" and "chick@cyberspace.com" for spotting it...
    dwBytesRead = SENDER_SIZE;
    // read the value of the SMTP server entry
    (void) RegQueryValueEx( hKey1, RegKeySender, NULL , &dwType, (BYTE *) Sender, &dwBytesRead);    //lint !e545

    dwBytesRead = TRY_SIZE;
    // read the value of the number of try entry
    lRetCode = RegQueryValueEx( hKey1, RegKeyTry, NULL , &dwType, (BYTE *) Try, &dwBytesRead);    //lint !e545
    // if we failed, assign a default value
    if ( lRetCode != ERROR_SUCCESS ) {
        strcpy(Try,"1");
    }

    dwBytesRead = sizeof(tmpstr) - 1;
    // read the value of the SMTP server entry
    lRetCode = RegQueryValueEx( hKey1, RegKeyLogin, NULL , &dwType, (BYTE *) tmpstr, &dwBytesRead);
    if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) )
        base64_decode((const unsigned char *)tmpstr, AUTHLogin);
    else
        AUTHLogin[0] = '\0';

    dwBytesRead = sizeof(tmpstr) - 1;
    // read the value of the SMTP server entry
    lRetCode = RegQueryValueEx( hKey1, RegKeyPassword, NULL , &dwType, (BYTE *) tmpstr, &dwBytesRead);
    if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) )
        base64_decode((const unsigned char *)tmpstr, AUTHPassword);
    else
        AUTHPassword[0] = '\0';

    dwBytesRead = SERVER_SIZE;
    // read the value of the SMTP server entry
    lRetCode = RegQueryValueEx( hKey1, RegKeySMTPHost, NULL , &dwType, (BYTE *) SMTPHost, &dwBytesRead);
    // if we got it, then get the smtp port.
    if ( lRetCode == ERROR_SUCCESS ) {
        dwBytesRead = SERVER_SIZE;
        // read the value of the SMTP port entry
        lRetCode = RegQueryValueEx( hKey1, RegKeySMTPPort, NULL , &dwType, (BYTE *) SMTPPort, &dwBytesRead);
        // if we failed, assign a default value
        if ( lRetCode != ERROR_SUCCESS )    strcpy( SMTPPort, defaultSMTPPort );
    }
    else {
        SMTPHost[0] = 0;
        SMTPPort[0] = 0;
    }

#if INCLUDE_NNTP
    dwBytesRead = SERVER_SIZE;
    // read the value of the NNTP server entry
    lRetCode = RegQueryValueEx( hKey1, RegKeyNNTPHost, NULL , &dwType, (BYTE *) NNTPHost, &dwBytesRead);
    // if we got it, then get the nntp port.
    if ( lRetCode == ERROR_SUCCESS ) {
        dwBytesRead = SERVER_SIZE;
        // read the value of the NNTP port entry
        lRetCode = RegQueryValueEx( hKey1, RegKeyNNTPPort, NULL , &dwType, (BYTE *) NNTPPort, &dwBytesRead);
        // if we failed, assign a default value
        if ( lRetCode != ERROR_SUCCESS )    strcpy( NNTPPort, defaultNNTPPort );
    }
    else {
        NNTPHost[0] = 0;
        NNTPPort[0] = 0;
    }
#endif

#if INCLUDE_POP3
    dwBytesRead = SERVER_SIZE;
    // read the value of the POP3 server entry
    lRetCode = RegQueryValueEx( hKey1, RegKeyPOP3Host, NULL , &dwType, (BYTE *) POP3Host, &dwBytesRead);
    // if we got it, then get the POP3 port.
    if ( lRetCode == ERROR_SUCCESS ) {
        dwBytesRead = SERVER_SIZE;
        // read the value of the POP3 port entry
        lRetCode = RegQueryValueEx( hKey1, RegKeyPOP3Port, NULL , &dwType, (BYTE *) POP3Port, &dwBytesRead);
        // if we failed, assign a default value
        if ( lRetCode != ERROR_SUCCESS )    strcpy( POP3Port, defaultPOP3Port );

        dwBytesRead = sizeof(tmpstr) - 1;
        // read the value of the SMTP server entry
        lRetCode = RegQueryValueEx( hKey1, RegKeyPOP3Login, NULL , &dwType, (BYTE *) tmpstr, &dwBytesRead);
        if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) )
            base64_decode((const unsigned char *)tmpstr, POP3Login);
        else
            strcpy( POP3Login, AUTHLogin );

        dwBytesRead = sizeof(tmpstr) - 1;
        // read the value of the SMTP server entry
        lRetCode = RegQueryValueEx( hKey1, RegKeyPOP3Password, NULL , &dwType, (BYTE *) tmpstr, &dwBytesRead);
        if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) )
            base64_decode((const unsigned char *)tmpstr, POP3Password);
        else
            strcpy( POP3Password, AUTHPassword );
    }
    else {
        POP3Host[0] = 0;
        POP3Port[0] = 0;
    }
#endif

#if INCLUDE_IMAP
    dwBytesRead = SERVER_SIZE;
    // read the value of the IMAP server entry
    lRetCode = RegQueryValueEx( hKey1, RegKeyIMAPHost, NULL , &dwType, (BYTE *) IMAPHost, &dwBytesRead);
    // if we got it, then get the IMAP port.
    if ( lRetCode == ERROR_SUCCESS ) {
        dwBytesRead = SERVER_SIZE;
        // read the value of the IMAP port entry
        lRetCode = RegQueryValueEx( hKey1, RegKeyIMAPPort, NULL , &dwType, (BYTE *) IMAPPort, &dwBytesRead);
        // if we failed, assign a default value
        if ( lRetCode != ERROR_SUCCESS )    strcpy( IMAPPort, defaultIMAPPort );

        dwBytesRead = sizeof(tmpstr) - 1;
        // read the value of the SMTP server entry
        lRetCode = RegQueryValueEx( hKey1, RegKeyIMAPLogin, NULL , &dwType, (BYTE *) tmpstr, &dwBytesRead);
        if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) )
            base64_decode((const unsigned char *)tmpstr, IMAPLogin);
        else
            strcpy( IMAPLogin, AUTHLogin );

        dwBytesRead = sizeof(tmpstr) - 1;
        // read the value of the SMTP server entry
        lRetCode = RegQueryValueEx( hKey1, RegKeyIMAPPassword, NULL , &dwType, (BYTE *) tmpstr, &dwBytesRead);
        if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) )
            base64_decode((const unsigned char *)tmpstr, IMAPPassword);
        else
            strcpy( IMAPPassword, AUTHPassword );
    }
    else {
        IMAPHost[0] = 0;
        IMAPPort[0] = 0;
    }
#endif

    if ( hKey1 )
        RegCloseKey(hKey1);

    // if the host is not specified, or if the sender is not specified,
    // leave the routine with error 12.
    if ( !Sender[0] )
        return (12);

    retval = 12;
    if ( SMTPHost[0] )
        retval = 0;

#if INCLUDE_POP3
    if ( POP3Host[0] )
        retval = 0;
#endif

#if INCLUDE_IMAP
    if ( IMAPHost[0] )
        retval = 0;
#endif

#if INCLUDE_NNTP
    if ( NNTPHost[0] )
        retval = 0;
#endif

    return retval;
}


int GetRegEntry( char *pstrProfile )
{
    int lRetVal;

    lRetVal = GetRegEntryKeyed( HKEY_CURRENT_USER, pstrProfile );
    if ( lRetVal )
        lRetVal = GetRegEntryKeyed( HKEY_LOCAL_MACHINE, pstrProfile );

    return lRetVal;
}

static void DisplayThisProfile( HKEY rootKeyLevel, char *pstrProfile )
{
    char * tmpstr;

    GetRegEntryKeyed( rootKeyLevel, pstrProfile );

    tmpstr = "";
    if ( AUTHLogin[0] || AUTHPassword[0] )
        tmpstr = " ***** *****";

    if ( SMTPHost[0] ) {
        //printMsg("SMTP: %s %s %s %s %s %s %s\n",SMTPHost,Sender,Try,SMTPPort,Profile,AUTHLogin,AUTHPassword);
        printMsg("%s: %s \"%s\" %s %s %s%s\n",
                 "SMTP", SMTPHost, Sender, Try, SMTPPort, Profile, tmpstr);
    }

#if INCLUDE_NNTP
    if ( NNTPHost[0] ) {
        //printMsg("NNTP: %s %s %s %s %s %s %s\n",NNTPHost,Sender,Try,NNTPPort,Profile,AUTHLogin,AUTHPassword);
        printMsg("%s: %s \"%s\" %s %s %s%s\n",
                 "NNTP", NNTPHost, Sender, Try, NNTPPort, Profile, tmpstr);
    }
#endif
#if INCLUDE_POP3
    tmpstr = "";
    if ( POP3Login[0] || POP3Password[0] )
        tmpstr = " ***** *****";

    if ( POP3Host[0] ) {
        //printMsg("POP3: %s - - %s %s %s %s\n",POP3Host,POP3Port,Profile,POP3Login,POP3Password);
        printMsg("%s: %s - - %s %s%s\n",
                 "POP3", POP3Host, POP3Port, Profile, tmpstr);
    }
#endif
#if INCLUDE_IMAP
    tmpstr = "";
    if ( IMAPLogin[0] || IMAPPassword[0] )
        tmpstr = " ***** *****";

    if ( IMAPHost[0] ) {
        //printMsg("IMAP: %s - - %s %s %s %s\n",IMAPHost,IMAPPort,Profile,IMAPLogin,IMAPPassword);
        printMsg("%s: %s - - %s %s%s\n",
                 "IMAP", IMAPHost, IMAPPort, Profile, tmpstr);
    }
#endif
}

static void DumpProfiles( HKEY rootKeyLevel, char *pstrProfile )
{
    HKEY     hKey1=NULL;
    DWORD    dwBytesRead;
    LONG     lRetCode;
    DWORD    dwIndex;                               // index of subkey to enumerate
    FILETIME lftLastWriteTime;
    // address for time key last written to);


    // open the registry key in read mode
    lRetCode = RegOpenKeyEx( rootKeyLevel,
                             MainRegKey,
                             0, KEY_READ, &hKey1
                           );
    if ( lRetCode != ERROR_SUCCESS ) {
        printMsg( "Failed to open registry key for Blat using default.\n" );
    } else {
        quiet      = FALSE;
        Profile[0] = '\0';

        if ( !strcmp(pstrProfile,"<default>") || !strcmp(pstrProfile,"<all>") ) {
            DisplayThisProfile( rootKeyLevel, "" );
        }

        dwIndex = 0;
        do {
            dwBytesRead = sizeof(Profile);
            lRetCode = RegEnumKeyEx(  hKey1,     // handle of key to enumerate
                                      dwIndex++, // index of subkey to enumerate
                                      Profile,   // address of buffer for subkey name
                                      &dwBytesRead,    // address for size of subkey buffer
                                      NULL,      // reserved
                                      NULL,      // address of buffer for class string
                                      NULL,      // address for size of class buffer
                                      &lftLastWriteTime
                                      // address for time key last written to);
                                   );
            if ( lRetCode == 0 ) {
                if ( !strcmp(pstrProfile,Profile) || !strcmp(pstrProfile,"<all>") ) {
                    DisplayThisProfile( rootKeyLevel, Profile );
                }
            }
        } while ( lRetCode == 0 );

        RegCloseKey(hKey1);
    }
}

// List all profiles
void ListProfiles( char *pstrProfile )
{
    HKEY     hKey1=NULL;
    LONG     lRetCode;


    // open the HKCU registry key in read mode
    lRetCode = RegOpenKeyEx( HKEY_CURRENT_USER,
                             MainRegKey,
                             0, KEY_READ, &hKey1
                           );
    if ( lRetCode == ERROR_SUCCESS ) {
        RegCloseKey(hKey1);
        printMsg("Profile(s) for current user --\n");
        DumpProfiles( HKEY_CURRENT_USER, pstrProfile );
        printMsg("\n");
    }

    printMsg("Profile(s) for all users of this computer --\n");
    DumpProfiles( HKEY_LOCAL_MACHINE, pstrProfile );
}
