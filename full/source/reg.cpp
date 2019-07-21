/*
    reg.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "blat.h"
#include "common_data.h"
#include "blatext.hpp"
#include "macros.h"
#include "reg.hpp"
#include "base64.hpp"
#include "sendsmtp.hpp"
#include "sendnntp.hpp"
#include "unicode.hpp"

static const _TCHAR SoftwareRegKey[]     = __T("SOFTWARE\\");
static const _TCHAR VirtualStoreKey[]    = __T("Classes\\VirtualStore\\MACHINE\\");

#if defined(_WIN64)
static const _TCHAR Wow6432NodeKey[]     = __T("Wow6432Node\\");
#endif
static const _TCHAR MainRegKey[]         = __T("Public Domain\\Blat");

static const _TCHAR RegKeySMTPHost[]     = __T("SMTP Server");
static const _TCHAR RegKeySMTPPort[]     = __T("SMTP Port");
#if INCLUDE_NNTP
static const _TCHAR RegKeyNNTPHost[]     = __T("NNTP Server");
static const _TCHAR RegKeyNNTPPort[]     = __T("NNTP Port");
#endif
#if INCLUDE_POP3
static const _TCHAR RegKeyPOP3Host[]     = __T("POP3 Server");
static const _TCHAR RegKeyPOP3Port[]     = __T("POP3 Port");
static const _TCHAR RegKeyPOP3Login[]    = __T("POP3 Login");
static const _TCHAR RegKeyPOP3Password[] = __T("POP3 Pwd");
#endif
#if INCLUDE_IMAP
static const _TCHAR RegKeyIMAPHost[]     = __T("IMAP Server");
static const _TCHAR RegKeyIMAPPort[]     = __T("IMAP Port");
static const _TCHAR RegKeyIMAPLogin[]    = __T("IMAP Login");
static const _TCHAR RegKeyIMAPPassword[] = __T("IMAP Pwd");
#endif
static const _TCHAR RegKeyLogin[]        = __T("Login");
static const _TCHAR RegKeyPassword[]     = __T("Pwd");
static const _TCHAR RegKeySender[]       = __T("Sender");
static const _TCHAR RegKeyTry[]          = __T("Try");

#if defined(_WIN64)
#else
#define LSTATUS LONG
#endif

#if defined(_UNICODE) || defined(UNICODE)

static void encodeThis( _TUCHAR * in, LPTSTR out, int inclCrLf )
{
    size_t byteCount;
    Buf    tmpStr;
    Buf    msg;
    size_t len;
    Buf    inputString;
    int    tempUTF;


    if ( in && out ) {
        tmpStr.Clear();
        *out = __T('\0');

        for ( len = 0; ; len++ ) {
            if ( in[len] == __T('\0') )
                break;
            if ( in[len] > 0x00FF ) {
                tempUTF = sizeof(_TCHAR);
                inputString = (LPTSTR)in;
                convertPackedUnicodeToUTF( inputString, tmpStr, &tempUTF, NULL, 8 );
                break;
            }
            tmpStr.Add( (_TCHAR)in[len] );
        }

        byteCount = tmpStr.Length();
        if ( byteCount ) {
            msg.AllocExact( (byteCount+1) * 3 );
            base64_encode( (_TUCHAR *)tmpStr.Get(), (size_t)byteCount, msg.Get(), inclCrLf );
            _tcscpy( out, msg.Get() );
        }

        inputString.Free();
        msg.Free();
        tmpStr.Free();
    }
}

//static void decodeThis(LPTSTR in, LPTSTR out)
//{
//    Buf    tmpStr;
//    Buf    msg;
//    size_t len;
//    size_t ix;
//    LPTSTR pTcharString;
//    DWORD  tlc;
//    int    count;
//
//    *out = __T('\0');
//    len = _tcslen(in);
//    if ( len ) {
//        len++;
//        for ( ix = 0; ix < len; ix++ ) {
//            if ( in[ix] == __T('\0') ) {
//               tmpStr.Add( __T('\0') );
//               ix++;
//               break;
//            }
//            if ( in[ix] & ~0x00FF ) {
//                tmpStr.Free();
//                return;
//            }
//            tmpStr.Add( (char)in[ix] );
//        }
//        len = ix;
//        msg.AllocExact( len );
//        pTcharString = msg.Get();
//        base64_decode( (_TUCHAR *)tmpStr.Get(), pTcharString );
//        tmpStr.Free();
//        for ( len = 0; ; len++ ) {
//            if ( pTcharString[len] == __T('\0') ) {
//                tmpStr.Add( __T('\0') );
//                break;
//            }
//            if ( (_TUCHAR)pTcharString[len] >= 0xFE ) {
//                msg.Free();
//                tmpStr.Free();
//                return;
//            }
//            if ( (_TUCHAR)pTcharString[len] < 128 ) {
//                tmpStr.Add( pTcharString[len] );
//                continue;
//            }
//
//            count = 0;
//            if ( (((_TUCHAR)pTcharString[len] & 0xE0) == 0xC0) &&
//                 ( (_TUCHAR)pTcharString[len]         >= 0xC1) ) {
//                tlc = (_TCHAR)(pTcharString[len] & 0x1Fl);
//                count = 1;
//            } else
//            if ( ((_TUCHAR)pTcharString[len] & 0xF0) == 0xE0 ) {
//                tlc = (_TCHAR)(pTcharString[len] & 0x0Fl);
//                count = 2;
//            } else
//            if ( ((_TUCHAR)pTcharString[len] & 0xF8) == 0xF0 ) {
//                tlc = (_TCHAR)(pTcharString[len] & 0x07l);
//                count = 3;
//            } else
//            if ( ((_TUCHAR)pTcharString[len] & 0xFC) == 0xF8 ) {
//                tlc = (_TCHAR)(pTcharString[len] & 0x03l);
//                count = 4;
//            } else
//            if ( ((_TUCHAR)pTcharString[len] & 0xFE) == 0xFC ) {
//                tlc = (_TCHAR)(pTcharString[len] & 0x01l);
//                count = 5;
//            } else {
//                msg.Free();
//                tmpStr.Free();
//                return;
//            }
//            while ( count ) {
//                if ( (pTcharString[++len] & 0xC0) != 0x80 ) {
//                    msg.Free();
//                    tmpStr.Free();
//                    return;
//                }
//                tlc <<= 6;
//                tlc |= (_TCHAR)(pTcharString[++len] & 0x3F);
//                count--;
//            }
//            tmpStr.Add( (_TCHAR)tlc );
//        }
//        _tcscpy( out, tmpStr.Get() );
//    }
//    msg.Free();
//    tmpStr.Free();
//}
//
#else
#define encodeThis(s,o,crlf) base64_encode((s), _tcslen((char *)(s)), (LPTSTR)(o), (crlf))
#endif
#define decodeThis(s,o)      base64_decode((_TUCHAR *)(s), (o))

// create registry entries for this program
static int CreateRegEntry( COMMON_DATA & CommonData, HKEY rootKeyLevel )
{
    FUNCTION_ENTRY();
    HKEY    hKey1;
    DWORD   dwDisposition;
    LSTATUS lRetCode;
    Buf     strRegisterKey;
    Buf     tmpstr;

#if defined(_WIN64)
    strRegisterKey = SoftwareRegKey;
    strRegisterKey.Add( Wow6432NodeKey );
    strRegisterKey.Add( MainRegKey );
    if ( CommonData.Profile.Get()[0] != __T('\0') ) {
        strRegisterKey.Add(__T("\\"));
        strRegisterKey.Add(CommonData.Profile);
    }

    /* try to create the .INI file key */
    lRetCode = RegCreateKeyEx ( rootKeyLevel,
                                strRegisterKey.Get(),
                                0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey1, &dwDisposition
                              );

    /* if we failed the normal location, try under VirtualStore */
    if ( lRetCode != ERROR_SUCCESS )
    {
        strRegisterKey = SoftwareRegKey;
        strRegisterKey.Add( VirtualStoreKey );
        strRegisterKey.Add( SoftwareRegKey );
        strRegisterKey.Add( Wow6432NodeKey );
        strRegisterKey.Add( MainRegKey );
        if ( CommonData.Profile.Get()[0] != __T('\0') ) {
            strRegisterKey.Add(__T("\\"));
            strRegisterKey.Add(CommonData.Profile);
        }
        /* try to create the registry key */
        lRetCode = RegCreateKeyEx ( rootKeyLevel,
                                    strRegisterKey.Get(),
                                    0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey1, &dwDisposition
                                  );
    }

    /* if we failed, note it, and leave */
    if ( lRetCode != ERROR_SUCCESS )
#endif
    {
        strRegisterKey = SoftwareRegKey;
        strRegisterKey.Add( MainRegKey );
        if ( CommonData.Profile.Get()[0] != __T('\0') ) {
            strRegisterKey.Add(__T("\\"));
            strRegisterKey.Add(CommonData.Profile);
        }
        /* try to create the .INI file key */
        lRetCode = RegCreateKeyEx ( rootKeyLevel,
                                    strRegisterKey.Get(),
                                    0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey1, &dwDisposition
                                  );
    }

    /* if we failed the normal location, try under VirtualStore */
    if ( lRetCode != ERROR_SUCCESS )
    {
        strRegisterKey = SoftwareRegKey;
        strRegisterKey.Add( VirtualStoreKey );
        strRegisterKey.Add( SoftwareRegKey );
        strRegisterKey.Add( MainRegKey );
        if ( CommonData.Profile.Get()[0] != __T('\0') ) {
            strRegisterKey.Add(__T("\\"));
            strRegisterKey.Add(CommonData.Profile);
        }
        /* try to create the registry key */
        lRetCode = RegCreateKeyEx ( rootKeyLevel,
                                    strRegisterKey.Get(),
                                    0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey1, &dwDisposition
                                  );
    }
    strRegisterKey.Free();

    /* if we failed, note it, and leave */
    if ( lRetCode != ERROR_SUCCESS ) {
        if ( !CommonData.quiet ) {
            if ( (lRetCode == ERROR_CANTWRITE) || (lRetCode == ERROR_ACCESS_DENIED) )
                pMyPrintDLL( __T("Write access to the registry was denied.\n") );
            else
                pMyPrintDLL( __T("Error in creating blat key in the registry\n") );
        }

        FUNCTION_EXIT();
        return(10);
    }

    if ( CommonData.SMTPHost.Length() > SERVER_SIZE ) {
        CommonData.SMTPHost.Get()[SERVER_SIZE] = __T('\0');
        CommonData.SMTPHost.SetLength();
    }
    if ( CommonData.SMTPPort.Length() > SERVER_SIZE ) {
        CommonData.SMTPPort.Get()[SERVER_SIZE] = __T('\0');
        CommonData.SMTPPort.SetLength();
    }
#if INCLUDE_NNTP
    if ( CommonData.NNTPHost.Length() > SERVER_SIZE ) {
        CommonData.NNTPHost.Get()[SERVER_SIZE] = __T('\0');
        CommonData.NNTPHost.SetLength();
    }
    if ( CommonData.NNTPPort.Length() > SERVER_SIZE ) {
        CommonData.NNTPPort.Get()[SERVER_SIZE] = __T('\0');
        CommonData.NNTPPort.SetLength();
    }
#endif
#if INCLUDE_POP3
    if ( CommonData.POP3Host.Length() > SERVER_SIZE ) {
        CommonData.POP3Host.Get()[SERVER_SIZE] = __T('\0');
        CommonData.POP3Host.SetLength();
    }
    if ( CommonData.POP3Port.Length() > SERVER_SIZE ) {
        CommonData.POP3Port.Get()[SERVER_SIZE] = __T('\0');
        CommonData.POP3Port.SetLength();
    }
#endif
#if INCLUDE_IMAP
    if ( CommonData.IMAPHost.Length() > SERVER_SIZE ) {
        CommonData.IMAPHost.Get()[SERVER_SIZE] = __T('\0');
        CommonData.IMAPHost.SetLength();
    }
    if ( CommonData.IMAPPort.Length() > SERVER_SIZE ) {
        CommonData.IMAPPort.Get()[SERVER_SIZE] = __T('\0');
        CommonData.IMAPPort.SetLength();
    }
#endif
    if ( CommonData.Try.Length() > TRY_SIZE ) {
        CommonData.Try.Get()[TRY_SIZE] = __T('\0');
        CommonData.Try.SetLength();
    }
    if ( CommonData.Sender.Length() > SENDER_SIZE ) {
        CommonData.Sender.Get()[SENDER_SIZE] = __T('\0');
        CommonData.Sender.SetLength();
    }

    if ( CommonData.SMTPHost.Get()[0] ) {
        if ( !_tcscmp(CommonData.SMTPHost.Get(), __T("-")) ) {
            CommonData.SMTPHost.Clear();
            CommonData.SMTPPort.Clear();
        }

        /* try to set a section value */
        lRetCode = RegSetValueEx( hKey1, RegKeySMTPHost, 0, REG_SZ, (LPBYTE)CommonData.SMTPHost.Get(), (DWORD)((CommonData.SMTPHost.Length()+1)*sizeof(_TCHAR)) );

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( !CommonData.quiet )  pMyPrintDLL( __T("Error in setting SMTP server value in the registry\n"));
            FUNCTION_EXIT();
            return(11);
        }

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeySMTPPort, 0, REG_SZ, (LPBYTE)CommonData.SMTPPort.Get(), (DWORD)((CommonData.SMTPPort.Length()+1)*sizeof(_TCHAR)) );

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( !CommonData.quiet )  pMyPrintDLL( __T("Error in setting port value in the registry\n") );
            FUNCTION_EXIT();
            return(11);
        }

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyTry, 0, REG_SZ, (LPBYTE)CommonData.Try.Get(), (DWORD)((CommonData.Try.Length()+1)*sizeof(_TCHAR)) );

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( !CommonData.quiet )  pMyPrintDLL( __T("Error in setting number of try value in the registry\n") );
            FUNCTION_EXIT();
            return(11);
        }

        /* try to set another section value */
        tmpstr.Clear();
        tmpstr.Alloc( CommonData.AUTHLogin.Length() * 2 );
        encodeThis( (_TUCHAR *)CommonData.AUTHLogin.Get(), tmpstr.Get(), FALSE );
        tmpstr.SetLength();
        lRetCode = RegSetValueEx( hKey1, RegKeyLogin, 0, REG_SZ, (LPBYTE)tmpstr.Get(), (DWORD)((tmpstr.Length()+1)*sizeof(_TCHAR)) );
        tmpstr.Free();

        /* try to set another section value */
        tmpstr.Clear();
        tmpstr.Alloc( CommonData.AUTHPassword.Length() * 2 );
        encodeThis( (_TUCHAR *)CommonData.AUTHPassword.Get(), tmpstr.Get(), FALSE );
        tmpstr.SetLength();
        lRetCode = RegSetValueEx( hKey1, RegKeyPassword, 0, REG_SZ, (LPBYTE)tmpstr.Get(), (DWORD)((tmpstr.Length()+1)*sizeof(_TCHAR)) );
        tmpstr.Free();

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeySender, 0, REG_SZ, (LPBYTE)CommonData.Sender.Get(), (DWORD)((CommonData.Sender.Length()+1)*sizeof(_TCHAR)) );

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( !CommonData.quiet )  pMyPrintDLL( __T("Error in setting sender address value in the registry\n") );
            FUNCTION_EXIT();
            return(11);
        }
    }

#if INCLUDE_NNTP
    if ( CommonData.NNTPHost.Get()[0] ) {
        if ( !_tcscmp(CommonData.NNTPHost.Get(), __T("-")) ) {
            CommonData.NNTPHost.Clear();
            CommonData.NNTPPort.Clear();
        }

        /* try to set a section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyNNTPHost, 0, REG_SZ, (LPBYTE)CommonData.NNTPHost.Get(), (DWORD)((CommonData.NNTPHost.Length()+1)*sizeof(_TCHAR)) );

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( !CommonData.quiet )  pMyPrintDLL( __T("Error in setting NNTP server value in the registry\n") );
            FUNCTION_EXIT();
            return(11);
        }

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyNNTPPort, 0, REG_SZ, (LPBYTE)CommonData.NNTPPort.Get(), (DWORD)((CommonData.NNTPPort.Length()+1)*sizeof(_TCHAR)) );

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( !CommonData.quiet )  pMyPrintDLL( __T("Error in setting port value in the registry\n") );
            FUNCTION_EXIT();
            return(11);
        }

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyTry, 0, REG_SZ, (LPBYTE)CommonData.Try.Get(), (DWORD)((CommonData.Try.Length()+1)*sizeof(_TCHAR)) );

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( !CommonData.quiet )  pMyPrintDLL( __T("Error in setting number of try value in the registry\n") );
            FUNCTION_EXIT();
            return(11);
        }

        /* try to set another section value */
        tmpstr.Clear();
        tmpstr.Alloc( CommonData.AUTHLogin.Length() * 2 );
        encodeThis( (_TUCHAR *)CommonData.AUTHLogin.Get(), tmpstr.Get(), FALSE );
        tmpstr.SetLength();
        lRetCode = RegSetValueEx( hKey1, RegKeyLogin, 0, REG_SZ, (LPBYTE)tmpstr.Get(), (DWORD)((tmpstr.Length()+1)*sizeof(_TCHAR)) );
        tmpstr.Free();

        /* try to set another section value */
        tmpstr.Clear();
        tmpstr.Alloc( CommonData.AUTHPassword.Length() * 2 );
        encodeThis( (_TUCHAR *)CommonData.AUTHPassword.Get(), tmpstr.Get(), FALSE );
        tmpstr.SetLength();
        lRetCode = RegSetValueEx( hKey1, RegKeyPassword, 0, REG_SZ, (LPBYTE)tmpstr.Get(), (DWORD)((tmpstr.Length()+1)*sizeof(_TCHAR)) );
        tmpstr.Free();

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeySender, 0, REG_SZ, (LPBYTE)CommonData.Sender.Get(), (DWORD)((CommonData.Sender.Length()+1)*sizeof(_TCHAR)) );

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( !CommonData.quiet )  pMyPrintDLL( __T("Error in setting sender address value in the registry\n") );
            FUNCTION_EXIT();
            return(11);
        }
    }
#endif

#if INCLUDE_POP3
    if ( CommonData.POP3Host.Get()[0] ) {
        if ( !_tcscmp(CommonData.POP3Host.Get(), __T("-")) ) {
            CommonData.POP3Host.Clear();
            CommonData.POP3Port.Clear();
        }

        /* try to set a section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyPOP3Host, 0, REG_SZ, (LPBYTE)CommonData.POP3Host.Get(), (DWORD)((CommonData.POP3Host.Length()+1)*sizeof(_TCHAR)) );

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( !CommonData.quiet )  pMyPrintDLL( __T("Error in setting POP3 server value in the registry\n") );
            FUNCTION_EXIT();
            return(11);
        }

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyPOP3Port, 0, REG_SZ, (LPBYTE)CommonData.POP3Port.Get(), (DWORD)((CommonData.POP3Port.Length()+1)*sizeof(_TCHAR)) );

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( !CommonData.quiet )  pMyPrintDLL( __T("Error in setting port value in the registry\n") );
            FUNCTION_EXIT();
            return(11);
        }

        tmpstr.Clear();
        tmpstr.Alloc( CommonData.POP3Login.Length() * 2 );
        encodeThis( (_TUCHAR *)CommonData.POP3Login.Get(), tmpstr.Get(), FALSE );
        tmpstr.SetLength();
        (void)RegSetValueEx( hKey1, RegKeyPOP3Login, 0, REG_SZ, (LPBYTE)tmpstr.Get(), (DWORD)((_tcslen(tmpstr)+1)*sizeof(_TCHAR)) );
        tmpstr.Free();

        /* try to set another section value */
        tmpstr.Clear();
        tmpstr.Alloc( CommonData.POP3Password.Length() * 2 );
        encodeThis( (_TUCHAR *)CommonData.POP3Password.Get(), tmpstr.Get(), FALSE );
        tmpstr.SetLength();
        (void)RegSetValueEx( hKey1, RegKeyPOP3Password, 0, REG_SZ, (LPBYTE)tmpstr.Get(), (DWORD)((_tcslen(tmpstr)+1)*sizeof(_TCHAR)) );
        tmpstr.Free();
    }
#endif

#if INCLUDE_IMAP
    if ( CommonData.IMAPHost.Get()[0] ) {
        if ( !_tcscmp(CommonData.IMAPHost.Get(), __T("-")) ) {
            CommonData.IMAPHost.Clear();
            CommonData.IMAPPort.Clear();
        }

        /* try to set a section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyIMAPHost, 0, REG_SZ, (LPBYTE)CommonData.IMAPHost.Get(), (DWORD)((CommonData.IMAPHost.Length()+1)*sizeof(_TCHAR)) );

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( !CommonData.quiet )  pMyPrintDLL( __T("Error in setting IMAP server value in the registry\n") );
            FUNCTION_EXIT();
            return(11);
        }

        /* try to set another section value */
        lRetCode = RegSetValueEx( hKey1, RegKeyIMAPPort, 0, REG_SZ, (LPBYTE)CommonData.IMAPPort.Get(), (DWORD)((CommonData.IMAPPort.Length()+1)*sizeof(_TCHAR)) );

        /* if we failed, note it, and leave */
        if ( lRetCode != ERROR_SUCCESS ) {
            if ( !CommonData.quiet )  pMyPrintDLL( __T("Error in setting port value in the registry\n") );
            FUNCTION_EXIT();
            return(11);
        }

        tmpstr.Clear();
        tmpstr.Alloc( CommonData.IMAPLogin.Length() * 2 );
        encodeThis( (_TUCHAR *)CommonData.IMAPLogin.Get(), tmpstr.Get(), FALSE );
        tmpstr.SetLength();
        (void)RegSetValueEx( hKey1, RegKeyIMAPLogin, 0, REG_SZ, (LPBYTE)tmpstr.Get(), (DWORD)((tmpstr.Length()+1)*sizeof(_TCHAR)) );
        tmpstr.Free();

        /* try to set another section value */
        tmpstr.Clear();
        tmpstr.Alloc( CommonData.IMAPPassword.Length() * 2 );
        encodeThis( (_TUCHAR *)CommonData.IMAPPassword.Get(), tmpstr.Get(), FALSE );
        tmpstr.SetLength();
        (void)RegSetValueEx( hKey1, RegKeyIMAPPassword, 0, REG_SZ, (LPBYTE)tmpstr.Get(), (DWORD)((tmpstr.Length()+1)*sizeof(_TCHAR)) );
        tmpstr.Free();
    }
#endif

    FUNCTION_EXIT();
    return(0);
}

// create registry entries for this program
int CreateRegEntry( COMMON_DATA & CommonData, int useHKCU )
{
    FUNCTION_ENTRY();
    int retval;

    retval = 1;     // assume useHKCU set, so the 2nd if() works.
    if ( !useHKCU )
        retval = CreateRegEntry( CommonData, HKEY_LOCAL_MACHINE );

    if ( retval )
        retval = CreateRegEntry( CommonData, HKEY_CURRENT_USER );

    FUNCTION_EXIT();
    return retval;
}

/**************************************************************
 **************************************************************/

void ShowRegHelp( COMMON_DATA & CommonData )
{
    printMsg( CommonData, __T("BLAT PROFILE EDITOR\n") );
    printMsg( CommonData, __T("To modify SMTP: blat -install SMTPHost Sender [Try [Port [Profile [Login name\n") );
    printMsg( CommonData, __T("                     [Password]]]]]\n") );
    printMsg( CommonData, __T("            or: blat -installSMTP SMTPHost Sender [Try [Port [Profile\n") );
    printMsg( CommonData, __T("                     [Login name [Password]]]]]\n") );
#if INCLUDE_NNTP
    printMsg( CommonData, __T("To modify NNTP: blat -installNNTP NNTPHost Sender [Try [Port [Profile\n") );
    printMsg( CommonData, __T("                     [Login name [Password]]]]]\n") );
#endif
#if INCLUDE_POP3
    printMsg( CommonData, __T("To modify POP3: blat -installPOP3 POP3Host - - [Port [Profile\n") );
    printMsg( CommonData, __T("                     [Login name [Password]]]]\n") );
#endif
#if INCLUDE_IMAP
    printMsg( CommonData, __T("To modify IMAP: blat -installIMAP IMAPHost - - [Port [Profile\n") );
    printMsg( CommonData, __T("                     [Login name [Password]]]]\n") );
#endif
    printMsg( CommonData, __T("To delete:      blat -profile -delete Profile\n") );
    printMsg( CommonData, __T("Profiles are listed as in the -install option:\n") );
    printMsg( CommonData, __T("Host Sender Try Port Profile login_name password\n") );
    printMsg( CommonData, __T("\n") );
}

/*
 * The following paths are currently supported:
 *
 * Computer\HKEY_LOCAL_MACHINE\Software\Public Domain\Blat
 * Computer\HKEY_LOCAL_MACHINE\Software\Wow6432Node\Public Domain\Blat
 * Computer\HKEY_LOCAL_MACHINE\Software\Classes\VirtualStore\MACHINE\SOFTWARE\Public Domain\Blat
 * Computer\HKEY_LOCAL_MACHINE\Software\Classes\VirtualStore\MACHINE\SOFTWARE\Wow6432Node\Public Domain\Blat
 *
 * Computer\HKEY_CURRENT_USER\Software\Public Domain\Blat
 * Computer\HKEY_CURRENT_USER\Software\Wow6432Node\Public Domain\Blat
 * Computer\HKEY_CURRENT_USER\Software\Classes\VirtualStore\MACHINE\SOFTWARE\Public Domain\Blat
 * Computer\HKEY_CURRENT_USER\Software\Classes\VirtualStore\MACHINE\SOFTWARE\Wow6432Node\Public Domain\Blat
 *
 * Add support for searching these areas:
 *
 * Computer\HKEY_USERS\S-???????\Software\Classes\VirtualStore\MACHINE\SOFTWARE\Public Domain\Blat
 * Computer\HKEY_USERS\S-???????\Software\Classes\VirtualStore\MACHINE\SOFTWARE\Wow6432Node\Public Domain\Blat
 * Computer\HKEY_USERS\S-???????_Classes\VirtualStore\MACHINE\SOFTWARE\Public Domain\Blat
 * Computer\HKEY_USERS\S-???????_Classes\VirtualStore\MACHINE\SOFTWARE\Wow6432Node\Public Domain\Blat
 *
 * The ? does not matter, normally numerics and hyphens.
 */
typedef enum {
    VirtualStore_PublicDomain = 0,
    Software_VirtualStore_PublicDomain,
    Software_PublicDomain,
#if defined(_WIN64)
    VirtualStore_Wow6432_PublicDomain,
    Software_VirtualStore_Wow6432_PublicDomain,
    Software_Wow6432_PublicDomain,
#endif
    LAST_REG_LEVEL
} REGISTRY_LEVEL;


static LSTATUS DeleteRegTree( COMMON_DATA & CommonData, HKEY rootKeyLevel, Buf & pstrProfile )
{
    FUNCTION_ENTRY();
    HKEY     hKey1;
    DWORD    dwBytesRead;
    LSTATUS  lRetCode;
    DWORD    dwIndex;                                                       // index of subkey to enumerate
    FILETIME lftLastWriteTime;
    Buf      newProfile;
    Buf      tempBuffer;


    tempBuffer.Alloc(256);

    // open the registry key in read mode
    lRetCode = RegOpenKeyEx( rootKeyLevel, pstrProfile.Get(), 0, KEY_ALL_ACCESS, &hKey1 );
    dwIndex = 0;
    while ( lRetCode == ERROR_SUCCESS ) {
        dwBytesRead = 256;
        tempBuffer.Clear();
        lRetCode = RegEnumValue( hKey1,                                     // handle of value to enumerate
                                 dwIndex++,                                 // index of subkey to enumerate
                                 tempBuffer.Get(),                          // address of buffer for subkey name
                                 &dwBytesRead,                              // address for size of subkey buffer
                                 NULL,                                      // reserved
                                 NULL,                                      // address of buffer for key type
                                 NULL,                                      // address of buffer for value data
                                 NULL                                       // address for length of value data );
                               );
        if ( lRetCode == ERROR_SUCCESS ) {
            lRetCode = RegDeleteValue( hKey1, tempBuffer.Get() );
            dwIndex--;
        }
    }
    RegCloseKey(hKey1);

    lRetCode = RegOpenKeyEx( rootKeyLevel, pstrProfile.Get(), 0, KEY_ALL_ACCESS, &hKey1 );
    dwIndex = 0;
    while ( lRetCode == ERROR_SUCCESS ) {
        dwBytesRead = 256;
        tempBuffer.Clear();
        tempBuffer.Alloc(dwBytesRead * 4);
        lRetCode = RegEnumKeyEx( hKey1,                                     // handle of key to enumerate
                                 dwIndex++,                                 // index of subkey to enumerate
                                 tempBuffer.Get(),                          // address of buffer for subkey name
                                 &dwBytesRead,                              // address for size of subkey buffer
                                 NULL,                                      // reserved
                                 NULL,                                      // address of buffer for class string
                                 NULL,                                      // address for size of class buffer
                                 &lftLastWriteTime                          // address for time key last written to;
                               );
        if ( lRetCode == ERROR_SUCCESS ) {
            newProfile = pstrProfile;
            newProfile.Add(__T("\\"));
            newProfile.Add(tempBuffer.Get());
            lRetCode = DeleteRegTree( CommonData, rootKeyLevel, newProfile );
        }
        if ( lRetCode == ERROR_SUCCESS ) {
            lRetCode = RegDeleteKey( hKey1, tempBuffer.Get() );
            if ( lRetCode != ERROR_SUCCESS ) {
                if ( !CommonData.quiet ) {
                    pMyPrintDLL( __T("Error in deleting profile ") );
                    pMyPrintDLL( pstrProfile.Get() );
                    pMyPrintDLL( __T(" in the registry\n") );
                }
                FUNCTION_EXIT();
                return(11);
            }
            dwIndex--;
        }
    }
    if ( lRetCode == ERROR_NO_MORE_ITEMS )
        lRetCode = ERROR_SUCCESS;

    RegCloseKey(hKey1);

    tempBuffer.Free();
    FUNCTION_EXIT();
    return lRetCode;
}

// Delete registry entries for this program
int DeleteRegEntry( COMMON_DATA & CommonData, HKEY rootKeyLevel, LPTSTR pstrSubDirectory, Buf & pstrProfile )
{
    FUNCTION_ENTRY();
    LSTATUS lRetCode;
    HKEY    hKey1;

    if ( pstrSubDirectory == NULL )
        pstrSubDirectory = __T("");

    if ( (rootKeyLevel == HKEY_USERS) && (pstrSubDirectory[0] == __T('\0')) ) {
        /*
         * Search first level below HKEY_USERS, calling ourselves with the first level entries we find.
         */
        lRetCode = RegOpenKeyEx ( rootKeyLevel, NULL, 0, KEY_ALL_ACCESS, &hKey1 );
        if ( lRetCode == ERROR_SUCCESS ) {
            DWORD  dwBytesRead;
            DWORD  dwIndex;     // index of subkey to enumerate
            Buf    subKey;
            _TCHAR quiet;

            quiet = CommonData.quiet;
            CommonData.quiet = TRUE;

            dwIndex = 0;
            for ( ; ; ) {
                LPTSTR pKeyName;

                dwBytesRead = 256;
                subKey.Clear();
                pKeyName = subKey.Get();
                lRetCode = RegEnumKeyEx( hKey1,                             // handle of value to enumerate
                                         dwIndex,                           // index of subkey to enumerate
                                         pKeyName,                          // address of buffer for subkey name
                                         &dwBytesRead,                      // address for size of subkey buffer
                                         NULL,                              // reserved
                                         NULL,                              // address of buffer for user defined class
                                         NULL,                              // address for length of user defined class
                                         NULL                               // address for FILETIME structure, time at which was last written to
                                       );
                if ( lRetCode == ERROR_SUCCESS ) {
                    subKey.SetLength();
                    subKey.Add( __T('\\') );
                    DeleteRegEntry( CommonData, rootKeyLevel, subKey.Get(), pstrProfile );
                    dwIndex++;
                } else
                    break;
            }
            RegCloseKey( hKey1 );
            subKey.Free();
            CommonData.quiet = quiet;
        }
    } else {
        Buf      strRegisterKey;
        unsigned regLevel;
        bool     foundSomething;

        foundSomething = FALSE;

        hKey1=NULL;
        regLevel = LAST_REG_LEVEL;
        do {
            regLevel--;
            strRegisterKey.Clear();
            strRegisterKey.Add( pstrSubDirectory );
            switch( (REGISTRY_LEVEL) regLevel )
            {
#if defined(_WIN64)
            case Software_VirtualStore_Wow6432_PublicDomain:
                strRegisterKey.Add( SoftwareRegKey );
                /* fall through */

            case VirtualStore_Wow6432_PublicDomain:
                strRegisterKey.Add( VirtualStoreKey );
                /* fall through */

            case Software_Wow6432_PublicDomain:
                strRegisterKey.Add( SoftwareRegKey );
                strRegisterKey.Add( Wow6432NodeKey );
                break;
#endif
            case Software_VirtualStore_PublicDomain:
                strRegisterKey.Add( SoftwareRegKey );
                /* fall through */

            case VirtualStore_PublicDomain:
                strRegisterKey.Add( VirtualStoreKey );
                /* fall through */

            default:
            case Software_PublicDomain:
                strRegisterKey.Add( SoftwareRegKey );
                break;
            }
            strRegisterKey.Add( MainRegKey );
            lRetCode = RegOpenKeyEx ( rootKeyLevel, strRegisterKey.Get(), 0, KEY_ALL_ACCESS, &hKey1 );
            if ( lRetCode == ERROR_SUCCESS ) {
                DWORD dwBytesRead;
                DWORD dwIndex;      // index of subkey to enumerate
                Buf   strMainRegKey;
                Buf   tempBuffer;

                tempBuffer.Alloc(256);
                strMainRegKey = strRegisterKey;
                RegCloseKey(hKey1);

                if ( !_tcscmp(pstrProfile.Get(), __T("<all>")) ) {
                    DeleteRegTree( CommonData, rootKeyLevel, strRegisterKey );
                } else
                if ( !_tcscmp(pstrProfile.Get(), __T("<default>")) ) {
                    lRetCode = RegOpenKeyEx ( rootKeyLevel, strRegisterKey.Get(), 0, KEY_ALL_ACCESS, &hKey1 );

                    dwIndex = 0;
                    while ( lRetCode == ERROR_SUCCESS ) {
                        dwBytesRead = 256;
                        tempBuffer.Clear();
                        lRetCode = RegEnumValue( hKey1,                     // handle of value to enumerate
                                                 dwIndex++,                 // index of subkey to enumerate
                                                 tempBuffer.Get(),          // address of buffer for subkey name
                                                 &dwBytesRead,              // address for size of subkey buffer
                                                 NULL,                      // reserved
                                                 NULL,                      // address of buffer for key type
                                                 NULL,                      // address of buffer for value data
                                                 NULL                       // address for length of value data );
                                               );
                        if ( lRetCode == ERROR_SUCCESS ) {
                            lRetCode = RegDeleteValue( hKey1, tempBuffer.Get() );
                            if ( lRetCode == ERROR_SUCCESS ) {
                                foundSomething = TRUE;
                            }
                            dwIndex--;
                        }
                    }
                    RegCloseKey(hKey1);
                } else
                if ( pstrProfile.Get()[0] != __T('\0') ) {
                    strRegisterKey.Add(__T("\\"));
                    strRegisterKey.Add(pstrProfile);

                    dwIndex = 0;
                    lRetCode = RegOpenKeyEx ( rootKeyLevel, strRegisterKey.Get(), 0, KEY_ALL_ACCESS, &hKey1 );
                    while ( lRetCode == ERROR_SUCCESS ) {
                        dwBytesRead = 256;
                        tempBuffer.Clear();
                        lRetCode = RegEnumValue( hKey1,                     // handle of value to enumerate
                                                 dwIndex++,                 // index of subkey to enumerate
                                                 tempBuffer.Get(),          // address of buffer for subkey name
                                                 &dwBytesRead,              // address for size of subkey buffer
                                                 NULL,                      // reserved
                                                 NULL,                      // address of buffer for key type
                                                 NULL,                      // address of buffer for value data
                                                 NULL                       // address for length of value data );
                                               );
                        if ( lRetCode == ERROR_SUCCESS ) {
                            lRetCode = RegDeleteValue( hKey1, tempBuffer.Get() );
                            if ( lRetCode == ERROR_SUCCESS ) {
                                foundSomething = TRUE;
                            }
                            dwIndex--;
                        }
                    }

                    RegCloseKey(hKey1);
                    RegDeleteKey( rootKeyLevel, strRegisterKey.Get() );
                }

                /*
                 * Check if there are more keys or data under \Public Domain\Blat.
                 * If nothing exists, then try to delete \Public Domain\Blat.
                 */
                lRetCode = RegOpenKeyEx ( rootKeyLevel, strMainRegKey.Get(), 0, KEY_ALL_ACCESS, &hKey1 );
                if ( lRetCode == ERROR_SUCCESS ) {
                    dwIndex = 0;
                    dwBytesRead = 256;
                    tempBuffer.Clear();
                    lRetCode = RegEnumValue( hKey1,                         // handle of value to enumerate
                                             dwIndex,                       // index of subkey to enumerate
                                             tempBuffer.Get(),              // address of buffer for subkey name
                                             &dwBytesRead,                  // address for size of subkey buffer
                                             NULL,                          // reserved
                                             NULL,                          // address of buffer for key type
                                             NULL,                          // address of buffer for value data
                                             NULL                           // address for length of value data );
                                           );
                    if ( lRetCode == ERROR_NO_MORE_ITEMS ) {                // No data items below \Blat ?
                        Buf subKey;

                        RegCloseKey( hKey1 );
                        lRetCode = RegOpenKeyEx ( rootKeyLevel, strMainRegKey.Get(), 0, KEY_ALL_ACCESS, &hKey1 );
                        subKey.Clear();
                        dwIndex = 0;
                        dwBytesRead = 256;
                        lRetCode = RegEnumKeyEx( hKey1,                     // handle of value to enumerate
                                                 dwIndex,                   // index of subkey to enumerate
                                                 subKey.Get(),              // address of buffer for subkey name
                                                 &dwBytesRead,              // address for size of subkey buffer
                                                 NULL,                      // reserved
                                                 NULL,                      // address of buffer for user defined class
                                                 NULL,                      // address for length of user defined class
                                                 NULL                       // address for FILETIME structure, time at which was last written to
                                               );
                    }
                    RegCloseKey( hKey1 );
                    if ( lRetCode == ERROR_NO_MORE_ITEMS ) {                // No data items or keys below \Blat ?

                        // Attempt to delete the Blat key
                        lRetCode = RegDeleteKey( rootKeyLevel, strMainRegKey.Get() );   // Delete "\Blat"
                        if ( lRetCode == ERROR_SUCCESS ) {
                            LPTSTR pKeyName;

                            dwIndex = (DWORD)strMainRegKey.Length();
                            pKeyName = strMainRegKey.Get();
                            pKeyName[dwIndex-5] = __T('\0');                // Remove "\Blat" from the key string.

                            // Attempt to delete the Public Domain key
                            RegDeleteKey( rootKeyLevel, pKeyName );         // Delete "\Public Domain"
                        }
                    }
                }
                tempBuffer.Free();
            }
        } while ( regLevel > 0 );

        if ( !foundSomething ) {
            if ( rootKeyLevel == HKEY_LOCAL_MACHINE ) {
                DeleteRegEntry( CommonData, HKEY_CURRENT_USER, NULL, pstrProfile );
            } else
            if ( rootKeyLevel == HKEY_CURRENT_USER ) {
                DeleteRegEntry( CommonData, HKEY_USERS, NULL, pstrProfile );
            } else
            if ( !CommonData.quiet )
                pMyPrintDLL( __T("Error in finding or accessing blat profile in the registry\n") );
        }
        strRegisterKey.Free();
    }
    FUNCTION_EXIT();
    return(0);
}

/**************************************************************
 **************************************************************/

// get the registry entries for this program
static int GetRegEntryKeyed( COMMON_DATA & CommonData, HKEY rootKeyLevel, LPTSTR pstrRegistryKey, LPTSTR pstrProfile )
{
    FUNCTION_ENTRY();
    HKEY    hKey1;
    DWORD   dwType;
    DWORD   dwBytesRead;
    LSTATUS lRetCode;
    _TCHAR  tmpstr[(MAXOUTLINE + 2) * 2];
    int     retval;
    Buf     regKey;


    hKey1 = NULL;

    regKey = pstrRegistryKey;
    if ( pstrProfile && *pstrProfile ) {
        regKey.Add( "\\" );
        regKey.Add( pstrProfile );
    }

    // open the registry key in read mode
    lRetCode = RegOpenKeyEx( rootKeyLevel, regKey.Get(), 0, KEY_READ, &hKey1 );
    regKey.Free();

    if ( lRetCode != ERROR_SUCCESS ) {
//       printMsg( CommonData, __T("Failed to open registry key for Blat\n") );
         if ( hKey1 )
            RegCloseKey(hKey1);

        retval = 12;
        if ( pstrProfile && *pstrProfile ) {
            retval = GetRegEntryKeyed( CommonData, rootKeyLevel, pstrRegistryKey, NULL );
        }

        FUNCTION_EXIT();
        return retval;
    }

    // set the size of the buffer to contain the data returned from the registry
    // thanks to Beverly Brown "beverly@datacube.com" and "chick@cyberspace.com" for spotting it...
    dwBytesRead = SENDER_SIZE;
    // read the value of the SMTP server entry
    lRetCode = RegQueryValueEx( hKey1, RegKeySender, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead ); //lint !e545
    if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) )
        CommonData.Sender = tmpstr;
    else
        CommonData.Sender.Clear();

    dwBytesRead = TRY_SIZE;
    // read the value of the number of try entry
    lRetCode = RegQueryValueEx( hKey1, RegKeyTry, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead );    //lint !e545
    // if we failed, assign a default value
    if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) )
        CommonData.Try = tmpstr;
    else
        CommonData.Try = __T("1");

    dwBytesRead = (sizeof(tmpstr)/sizeof(tmpstr[0])) - 1;
    // read the value of the SMTP server entry
    lRetCode = RegQueryValueEx( hKey1, RegKeyLogin, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead );
    if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) ) {
        CommonData.AUTHLogin.Clear();
        CommonData.AUTHLogin.Alloc(dwBytesRead*4);
        decodeThis( tmpstr, CommonData.AUTHLogin.Get() );
        CommonData.AUTHLogin.SetLength();
    }

    dwBytesRead = (sizeof(tmpstr)/sizeof(tmpstr[0])) - 1;
    // read the value of the SMTP server entry
    lRetCode = RegQueryValueEx( hKey1, RegKeyPassword, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead );
    if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) ) {
        CommonData.AUTHPassword.Clear();
        CommonData.AUTHPassword.Alloc(dwBytesRead*4);
        decodeThis( tmpstr, CommonData.AUTHPassword.Get() );
        CommonData.AUTHPassword.SetLength();
    }

    dwBytesRead = SERVER_SIZE;
    // read the value of the SMTP server entry
    lRetCode = RegQueryValueEx( hKey1, RegKeySMTPHost, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead );
    // if we got it, then get the smtp port.
    if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) ) {
        CommonData.SMTPHost = tmpstr;

        dwBytesRead = SERVER_SIZE;
        // read the value of the SMTP port entry
        lRetCode = RegQueryValueEx( hKey1, RegKeySMTPPort, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead );
        // if we got it, then convert it back to Unicode
        if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) )
            CommonData.SMTPPort = tmpstr;
        else
            CommonData.SMTPPort = defaultSMTPPort;
    } else {
        CommonData.SMTPHost.Clear();
        CommonData.SMTPPort.Clear();
    }

#if INCLUDE_NNTP
    dwBytesRead = SERVER_SIZE;
    // read the value of the NNTP server entry
    lRetCode = RegQueryValueEx( hKey1, RegKeyNNTPHost, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead );
    // if we got it, then get the nntp port.
    if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) ) {
        CommonData.NNTPHost = tmpstr;

        dwBytesRead = SERVER_SIZE;
        // read the value of the NNTP port entry
        lRetCode = RegQueryValueEx( hKey1, RegKeyNNTPPort, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead );
        // if we got it,
        if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) )
            CommonData.NNTPPort = tmpstr;
        else
            CommonData.NNTPPort = defaultNNTPPort;
    } else {
        CommonData.NNTPHost.Clear();
        CommonData.NNTPPort.Clear();
    }
#endif

#if INCLUDE_POP3
    dwBytesRead = SERVER_SIZE;
    // read the value of the POP3 server entry
    lRetCode = RegQueryValueEx( hKey1, RegKeyPOP3Host, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead );
    // if we got it, then get the POP3 port.
    if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) ) {
        CommonData.POP3Host = tmpstr;

        dwBytesRead = SERVER_SIZE;
        // read the value of the POP3 port entry
        lRetCode = RegQueryValueEx( hKey1, RegKeyPOP3Port, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead );
        // if we got it,
        if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) )
            CommonData.POP3Port = tmpstr;
        else
            CommonData.POP3Port = defaultPOP3Port;

        dwBytesRead = (sizeof(tmpstr)/sizeof(tmpstr[0])) - 1;
        // read the value of the POP3 login id
        lRetCode = RegQueryValueEx( hKey1, RegKeyPOP3Login, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead );
        // if we got it,
        if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) ) {
            CommonData.POP3Login.Clear();
            CommonData.POP3Login.Alloc(dwBytesRead*4);
            decodeThis( tmpstr, CommonData.POP3Login.Get() );
            CommonData.POP3Login.SetLength();
        } else
            CommonData.POP3Login = CommonData.AUTHLogin;

        dwBytesRead = (sizeof(tmpstr)/sizeof(tmpstr[0])) - 1;
        // read the value of the POP3 password
        lRetCode = RegQueryValueEx( hKey1, RegKeyPOP3Password, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead );
        // if we got it,
        if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) ) {
            CommonData.POP3Password.Clear();
            CommonData.POP3Password.Alloc(dwBytesRead*4);
            decodeThis( tmpstr, CommonData.POP3Password.Get() );
            CommonData.POP3Password.SetLength();
        } else
            CommonData.POP3Password = CommonData.AUTHPassword;
    } else {
        CommonData.POP3Host.Clear();
        CommonData.POP3Port.Clear();
    }
#endif

#if INCLUDE_IMAP
    dwBytesRead = SERVER_SIZE;
    // read the value of the IMAP server entry
    lRetCode = RegQueryValueEx( hKey1, RegKeyIMAPHost, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead );
    // if we got it, then get the IMAP port.
    if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) ) {
        CommonData.IMAPHost = tmpstr;

        dwBytesRead = SERVER_SIZE;
        // read the value of the IMAP port entry
        lRetCode = RegQueryValueEx( hKey1, RegKeyIMAPPort, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead );
        // if we got it,
        if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) )
            CommonData.IMAPPort = tmpstr;
        else
            CommonData.IMAPPort = defaultIMAPPort;

        dwBytesRead = (sizeof(tmpstr)/sizeof(tmpstr[0])) - 1;
        // read the value of the IMAP login id
        lRetCode = RegQueryValueEx( hKey1, RegKeyIMAPLogin, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead );
        // if we got it,
        if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) ) {
            CommonData.IMAPLogin.Clear();
            CommonData.IMAPLogin.Alloc(dwBytesRead*4);
            decodeThis( tmpstr, CommonData.IMAPLogin.Get() );
            CommonData.IMAPLogin.SetLength();
       } else
            CommonData.IMAPLogin = CommonData.AUTHLogin;

        dwBytesRead = (sizeof(tmpstr)/sizeof(tmpstr[0])) - 1;
        // read the value of the IMAP password
        lRetCode = RegQueryValueEx( hKey1, RegKeyIMAPPassword, NULL, &dwType, (LPBYTE)tmpstr, &dwBytesRead );
        // if we got it,
        if ( dwBytesRead && (lRetCode == ERROR_SUCCESS) ) {
            CommonData.IMAPPassword.Clear();
            CommonData.IMAPPassword.Alloc(dwBytesRead*4);
            decodeThis( tmpstr, CommonData.IMAPPassword.Get() );
            CommonData.IMAPPassword.SetLength();
       } else
            CommonData.IMAPPassword = CommonData.AUTHPassword;
    } else {
        CommonData.IMAPHost.Clear();
        CommonData.IMAPPort.Clear();
    }
#endif

    if ( hKey1 )
        RegCloseKey(hKey1);

    // if the host is not specified, or if the sender is not specified,
    // leave the routine with error 12.
    retval = 12;
    if ( CommonData.Sender.Get()[0] ) {
        if ( CommonData.SMTPHost.Get()[0] )
            retval = 0;

#if INCLUDE_POP3
        if ( CommonData.POP3Host.Get()[0] )
            retval = 0;
#endif

#if INCLUDE_IMAP
        if ( CommonData.IMAPHost.Get()[0] )
            retval = 0;
#endif

#if INCLUDE_NNTP
        if ( CommonData.NNTPHost.Get()[0] )
            retval = 0;
#endif
    }

    FUNCTION_EXIT();
    return retval;
}

static LSTATUS SearchForRegEntry( COMMON_DATA & CommonData, HKEY rootKeyLevel, LPTSTR pstrProfile )
{
    FUNCTION_ENTRY();
    HKEY    hKey1;
    LSTATUS lRetCode;
    Buf     register_key;
    Buf     register_key_saved;

    hKey1 = NULL;

#if defined(_WIN64)
    register_key = SoftwareRegKey;
    register_key.Add( Wow6432NodeKey );
    register_key.Add( MainRegKey );
    register_key_saved = register_key;

    if ( pstrProfile && *pstrProfile ) {
        register_key.Add( __T("\\") );
        register_key.Add( pstrProfile );
    }

    // open the registry key in read mode
    lRetCode = RegOpenKeyEx( rootKeyLevel, register_key.Get(), 0, KEY_READ, &hKey1 );

    /* if we failed the normal location, try under VirtualStore */
    if ( lRetCode != ERROR_SUCCESS )
    {
        register_key = SoftwareRegKey;
        register_key.Add( VirtualStoreKey );
        register_key.Add( register_key_saved.Get() );
        register_key_saved = register_key;

        if ( pstrProfile && *pstrProfile ) {
            register_key.Add( __T("\\") );
            register_key.Add( pstrProfile );
        }

        // open the registry key in read mode
        lRetCode = RegOpenKeyEx( rootKeyLevel, register_key.Get(), 0, KEY_READ, &hKey1 );
    }
    if ( lRetCode != ERROR_SUCCESS )
#endif
    {
        register_key = SoftwareRegKey;
        register_key.Add( MainRegKey );
        register_key_saved = register_key;

        if ( pstrProfile && *pstrProfile ) {
            register_key.Add( __T("\\") );
            register_key.Add( pstrProfile );
        }

        // open the registry key in read mode
        lRetCode = RegOpenKeyEx( rootKeyLevel, register_key.Get(), 0, KEY_READ, &hKey1 );

        /* if we failed the normal location, try under VirtualStore */
        if ( lRetCode != ERROR_SUCCESS )
        {
            register_key = SoftwareRegKey;
            register_key.Add( VirtualStoreKey );
            register_key.Add( register_key_saved.Get() );
            register_key_saved = register_key;

            if ( pstrProfile && *pstrProfile ) {
                register_key.Add( __T("\\") );
                register_key.Add( pstrProfile );
            }

            // open the registry key in read mode
            lRetCode = RegOpenKeyEx( rootKeyLevel, register_key.Get(), 0, KEY_READ, &hKey1 );
        }
    }

    if ( hKey1 )
        RegCloseKey(hKey1);

    if ( lRetCode == ERROR_SUCCESS )
        lRetCode = GetRegEntryKeyed( CommonData, rootKeyLevel, register_key_saved.Get(), pstrProfile );

    register_key_saved.Free();
    register_key.Free();
    FUNCTION_EXIT();
    return lRetCode;
}

int GetRegEntry( COMMON_DATA & CommonData, Buf & pstrProfile )
{
    FUNCTION_ENTRY();
    int lRetVal;
    Buf register_key;

    if (pstrProfile.Length() == 0)
        pstrProfile.Clear();

    lRetVal = SearchForRegEntry( CommonData, HKEY_CURRENT_USER, pstrProfile.Get() );
    if ( lRetVal )
        lRetVal = SearchForRegEntry( CommonData, HKEY_LOCAL_MACHINE, pstrProfile.Get() );

    FUNCTION_EXIT();
    return lRetVal;
}

static void DisplayThisProfile( COMMON_DATA & CommonData, HKEY rootKeyLevel, LPTSTR pstrRegistryKey, LPTSTR pstrProfile )
{
    FUNCTION_ENTRY();
    LPTSTR tmpstr;

    GetRegEntryKeyed( CommonData, rootKeyLevel, pstrRegistryKey, pstrProfile );

    tmpstr = __T("");
    if ( CommonData.AUTHLogin.Get()[0] || CommonData.AUTHPassword.Get()[0] )
        tmpstr = __T(" ***** *****");

    if ( CommonData.SMTPHost.Get()[0] ) {
        //printMsg(CommonData, __T("SMTP: %s %s %s %s %s %s %s\n"),CommonData.SMTPHost.Get(),CommonData.Sender.Get(),CommonData.Try.Get(),CommonData.SMTPPort.Get(),CommonData.Profile.Get(),CommonData.AUTHLogin.Get(),CommonData.AUTHPassword.Get());
        printMsg(CommonData, __T("%s: %s \"%s\" %s %s %s%s\n"),
                 __T("SMTP"), CommonData.SMTPHost.Get(), CommonData.Sender.Get(), CommonData.Try.Get(), CommonData.SMTPPort.Get(), CommonData.Profile.Get(), tmpstr);
    }

#if INCLUDE_NNTP
    if ( CommonData.NNTPHost.Get()[0] ) {
        //printMsg(CommonData, __T("NNTP: %s %s %s %s %s %s %s\n"),CommonData.NNTPHost.Get(),CommonData.Sender.Get(),CommonData.Try.Get(),CommonData.NNTPPort.Get(),CommonData.Profile.Get(),CommonData.AUTHLogin.Get(),CommonData.AUTHPassword.Get());
        printMsg(CommonData, __T("%s: %s \"%s\" %s %s %s%s\n"),
                 __T("NNTP"), CommonData.NNTPHost.Get(), CommonData.Sender.Get(), CommonData.Try.Get(), CommonData.NNTPPort.Get(), CommonData.Profile.Get(), tmpstr);
    }
#endif
#if INCLUDE_POP3
    tmpstr = __T("");
    if ( CommonData.POP3Login.Get()[0] || CommonData.POP3Password.Get()[0] )
        tmpstr = __T(" ***** *****");

    if ( CommonData.POP3Host.Get()[0] ) {
        //printMsg(CommonData, __T("POP3: %s - - %s %s %s %s\n"),CommonData.POP3Host.Get(),CommonData.POP3Port.Get(),CommonData.Profile.Get(),CommonData.POP3Login.Get(),CommonData.POP3Password.Get());
        printMsg(CommonData, __T("%s: %s - - %s %s%s\n"),
                 __T("POP3"), CommonData.POP3Host.Get(), CommonData.POP3Port.Get(), CommonData.Profile.Get(), tmpstr);
    }
#endif
#if INCLUDE_IMAP
    tmpstr = __T("");
    if ( CommonData.IMAPLogin.Get()[0] || CommonData.IMAPPassword.Get()[0] )
        tmpstr = __T(" ***** *****");

    if ( CommonData.IMAPHost.Get()[0] ) {
        //printMsg(CommonData, __T("IMAP: %s - - %s %s %s %s\n"),CommonData.IMAPHost.Get(),CommonData.IMAPPort.Get(),CommonData.Profile.Get(),CommonData.IMAPLogin.Get(),CommonData.IMAPPassword.Get());
        printMsg(CommonData, __T("%s: %s - - %s %s%s\n"),
                 __T("IMAP"), CommonData.IMAPHost.Get(), CommonData.IMAPPort.Get(), CommonData.Profile.Get(), tmpstr);
    }
#endif
    FUNCTION_EXIT();
}

static bool DumpProfiles( COMMON_DATA & CommonData, HKEY rootKeyLevel, LPTSTR pstrProfile, unsigned regLevel )
{
    FUNCTION_ENTRY();
    HKEY     hKey1;
    DWORD    dwBytesRead;
    LSTATUS  lRetCode;
    DWORD    dwIndex;                                                       // index of subkey to enumerate
    FILETIME lftLastWriteTime;                                              // address for time key last written to
    Buf      strRegisterKey;
    bool     foundSomething;

    hKey1 = NULL;
    foundSomething = FALSE;

    strRegisterKey.Clear();
    switch( (REGISTRY_LEVEL) regLevel )
    {
#if defined(_WIN64)
    case Software_VirtualStore_Wow6432_PublicDomain:
        strRegisterKey.Add( SoftwareRegKey );
        /* fall through */

    case VirtualStore_Wow6432_PublicDomain:
        strRegisterKey.Add( VirtualStoreKey );
        /* fall through */

    case Software_Wow6432_PublicDomain:
        strRegisterKey.Add( SoftwareRegKey );
        strRegisterKey.Add( Wow6432NodeKey );
        break;
#endif
    case Software_VirtualStore_PublicDomain:
        strRegisterKey.Add( SoftwareRegKey );
        /* fall through */

    case VirtualStore_PublicDomain:
        strRegisterKey.Add( VirtualStoreKey );
        /* fall through */

    default:
    case Software_PublicDomain:
        strRegisterKey.Add( SoftwareRegKey );
        break;
    }
    strRegisterKey.Add( MainRegKey );

    /* try to open the registry key */
    lRetCode = RegOpenKeyEx( rootKeyLevel,
                             strRegisterKey.Get(),
                             0, KEY_READ, &hKey1
                           );
    if ( lRetCode == ERROR_SUCCESS ) {
        foundSomething = TRUE;
        CommonData.quiet = FALSE;
        CommonData.Profile.Clear();
        CommonData.Profile.Alloc(SERVER_SIZE * 4);

        if ( !_tcscmp(pstrProfile, __T("<default>")) || !_tcscmp(pstrProfile, __T("<all>")) ) {
            DisplayThisProfile( CommonData, rootKeyLevel, strRegisterKey.Get(), __T("") );
        }

        dwIndex = 0;
        do {
            dwBytesRead = SERVER_SIZE;
            lRetCode = RegEnumKeyEx( hKey1,                                 // handle of key to enumerate
                                     dwIndex++,                             // index of subkey to enumerate
                                     CommonData.Profile.Get(),              // address of buffer for subkey name
                                     &dwBytesRead,                          // address for size of subkey buffer
                                     NULL,                                  // reserved
                                     NULL,                                  // address of buffer for class string
                                     NULL,                                  // address for size of class buffer
                                     &lftLastWriteTime                      // address for time key last written to
                                   );
            if ( lRetCode == ERROR_SUCCESS ) {
                CommonData.Profile.SetLength();
                if ( !_tcscmp(pstrProfile, CommonData.Profile.Get()) ||
                     !_tcscmp(pstrProfile, __T("<all>")) ) {
                    DisplayThisProfile( CommonData, rootKeyLevel, strRegisterKey.Get(), CommonData.Profile.Get() );
                }
            }
        } while ( lRetCode == ERROR_SUCCESS );

        RegCloseKey(hKey1);
    }
    strRegisterKey.Free();
    FUNCTION_EXIT();
    return foundSomething;
}

// List all profiles
void ListProfiles( COMMON_DATA & CommonData, LPTSTR pstrProfile )
{
    FUNCTION_ENTRY();
    bool     foundSomething;
    unsigned regLevel;

    foundSomething = 0;

    printMsg( CommonData, __T("Profile(s) for current user --\n") );
    regLevel = LAST_REG_LEVEL;
    do {
        regLevel--;
        foundSomething |= DumpProfiles( CommonData, HKEY_CURRENT_USER, pstrProfile, regLevel );
    } while ( regLevel > 0 );
    printMsg( CommonData, __T("\n") );

    printMsg( CommonData, __T("Profile(s) for all users of this computer --\n") );
    regLevel = LAST_REG_LEVEL;
    do {
        regLevel--;
        foundSomething |= DumpProfiles( CommonData, HKEY_LOCAL_MACHINE, pstrProfile, regLevel );
    } while ( regLevel > 0 );
    printMsg( CommonData, __T("\n") );

    if ( !foundSomething )
        printMsg( CommonData, __T("Failed to open registry key for Blat to list any profiles.\n") );

    FUNCTION_EXIT();
}
