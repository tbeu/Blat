// CGI Support by Gilles Vollant <info@winimage.com>, February 1999
// Added in Blat Version 1.8.3

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "blat.h"
#include "buf.h"
#include "winfile.h"

/***************************************************************************/
// begin of CGI stuff
/***************************************************************************/

/*
The BLAT CGI accept both GET and FORM CGI usage
Usage :

1)
  call from Web server
     http://server/scripts/blat.exe/linecommand.txt

     http://linecommand.txt must be a single line text file, with the BLAT command line, like (without the '"')
     "- -t myname@mydomain.com -server smtp.mydomain.com -f myname@mydomain.com"

2)
     http://server/scripts/blat.exe
     Your HTTP request must contain some vars with the command line.:
          TO :            the -t parameters of Blat
          CC :            the -c parameters of Blat
          BCC :           the -b parameters of Blat
          SENDER :        the -f parameters of Blat
          FROM :          the -i parameters of Blat
          ORGANISATION :  the -o parameters of Blat
          SERVER :        the -server parameters of Blat
          SUBJECT :       the -s parameters of Blat
          PORT :          the -port parameters of Blat
          HOSTNAME :      the -hostname parameters of Blat
          TIMEOUT :       the -ti parameter of Blat
          XHEADER :       the -xheader parameter of Blat
     These Variable are Boolean (if present and set TO "1" or "Y", added the option,
                                 if not present or set TO "N" or "0", remove the option)

          NOH :           the -noh parameters of Blat
          NOH2 :          the -noh2 parameters of Blat
          MIME :          the -mime parameters of Blat
          UUENCODE :      the -uuencode parameters of Blat
          BASE64 :        the -base64 parameters of Blat
          REQUESTRECEIPT :the -r parameter of Blat
          NOTIFY :        the -d parameter of Blat

You can prefix these var name bu "Blat_" (ie using BLAT_SUBJECT instead SUBJECT) if you don't want see
 the variable content in the message.

Blat_success and Blat_failure will contain the URL for success and failure message

Example of HTML page using blat

<HTML><BODY>
GET test:<br>

<FORM ACTION="/scripts/blat.exe" METHOD="GET">
Enter here your email
     <INPUT TYPE="text" SIZE="20" NAME="From" VALUE="myname@mydomain.com">
<br>
     <INPUT TYPE="text" SIZE="20" NAME="TO" VALUE="myname2@winimage.com">
<br>
     <INPUT TYPE="text" SIZE="20" NAME="Blat_Subject" VALUE="le sujet est 'quot' !">
<br>
     <INPUT TYPE="text" SIZE="20" NAME="Var2" VALUE="Var_2">
<br>
     <INPUT TYPE="text" SIZE="20" NAME="Var3" VALUE="Var_3">
<br>
     <INPUT TYPE="hidden" NAME="Blat_success" VALUE="http://localhost/good.htm">
     <INPUT TYPE="hidden" NAME="Blat_failure" VALUE="http://localhost/bad.htm">
     <INPUT TYPE="hidden" NAME="Blat_Mime" VALUE="Y">
     <INPUT TYPE="submit" VALUE="submit"><BR>
</form></BODY></HTML>

TODO :
 - improve documentation

*/

extern _BLATOPTIONS blatOptionsList[];


BOOL getFileSize(LPCTSTR lpFn, DWORD &dwSize)
{
    WIN32_FIND_DATA ffblk;
    HANDLE hFind;
    _TCHAR szName[MAX_PATH+1];


    _tcscpy(szName,lpFn);
    dwSize = 0;

    if ( (hFind = FindFirstFile(szName,&ffblk)) == INVALID_HANDLE_VALUE )
        return FALSE;

    dwSize = ffblk.nFileSizeLow;
    FindClose(hFind);
    return TRUE;
}

//
// Works like _getenv(), but uses win32 functions instead.
//
void GetEnv(LPTSTR lpszEnvVar, Buf &buf) {

    DWORD dwLen = 0;

    if ( lpszEnvVar ) {
        dwLen = GetEnvironmentVariable(lpszEnvVar, NULL, 0);
    }

    if ( dwLen == 0 )
        buf.Clear();
    else {
        buf.AllocExact(dwLen+1);
        GetEnvironmentVariable(lpszEnvVar, buf.Get(), dwLen);
        buf.SetLength();
    }
}

void ReadPostData(Buf &buf)
{
    DWORD dwStep=4096;
    DWORD dwReadThis;
    DWORD dwThisStep;
    DWORD dwTotalBytes;
    Buf   lpszContentLength;

    GetEnv(__T("CONTENT_LENGTH"), lpszContentLength);
    dwTotalBytes = (DWORD)_tstol(lpszContentLength.Get());
    buf.AllocExact(dwTotalBytes + 1);
    buf.Clear();

    do {
        dwThisStep = (DWORD) min(dwTotalBytes-buf.Length(),dwStep);
        dwReadThis = dwThisStep;
        if ( dwThisStep > 0 ) {
            dwReadThis = 0;
            if ( !ReadFile(GetStdHandle(STD_INPUT_HANDLE),buf.GetTail(),dwReadThis,
                                        &dwReadThis,NULL) )
                dwReadThis = 0;
        }
        buf.SetLength(buf.Length() + dwReadThis);

    } while ( (dwReadThis==dwThisStep) && (buf.Length()<dwTotalBytes) );

    *buf.GetTail() = __T('\0');
    lpszContentLength.Free();
}

static int hextoint( _TCHAR c )
{
    if ( (c >= __T('0')) && (c <= __T('9')) )
        return c-__T('0');

    return _totupper(c)-__T('A')+10;
}

static void url_decode( LPTSTR cp )
{
    for ( ; *cp; cp++ ) {
        if ( *cp == __T('+') )
            *cp = __T(' ');
        else
        if ( *cp == __T('%') ) {
            *cp = (_TCHAR)(hextoint(*(cp+1)) * 16 + hextoint(*(cp+2)));
            memmove( cp+1, cp+3, (_tcslen(cp+3)+1)*sizeof(_TCHAR) );
        }
    }
}

DWORD SearchNextPos(LPCTSTR lpszParamCgi, BOOL fSearchEqual)
{
    _TCHAR cSup;
    DWORD  dwNext;

    if ( fSearchEqual )
        cSup = __T('=');
    else
        cSup = __T('\0');

    dwNext = 0;
    while ( (*(lpszParamCgi+dwNext) != __T('&')) && (*(lpszParamCgi+dwNext) != __T('\0')) && (*(lpszParamCgi+dwNext) != cSup) )
        dwNext++;

    return dwNext;
}

DWORD SearchNextPercent(LPCTSTR lpszParamCgi)
{
    DWORD dwNext = 0;

    while ( (*(lpszParamCgi+dwNext) != __T('%')) && (*(lpszParamCgi+dwNext) != __T('\0')) )
        dwNext++;
    return dwNext;
}

void SearchVar(Buf &lpszParamCgi,LPCTSTR lpszVarName,BOOL fMimeDecode, Buf &ret)
{
    Buf    lpAlloc;
    LPTSTR lpszProvCmp,lpszVarForCmp;
    DWORD  dwVarNameLen=(DWORD)_tcslen(lpszVarName);
    DWORD  dwLineLen=(DWORD)lpszParamCgi.Length();
    DWORD  dwPos=0;

    ret.Clear();

    lpAlloc.AllocExact((dwVarNameLen+0x10)*2);
    lpszProvCmp=lpAlloc.Get();
    lpszVarForCmp=lpAlloc.Get()+(((dwVarNameLen+7)/4)*4);

    _tcscpy(lpszVarForCmp,lpszVarName);
    _tcscat(lpszVarForCmp,__T("="));
    *(lpszProvCmp+dwVarNameLen+1) = __T('\0');

    while ( dwPos < dwLineLen ) {
        DWORD dwNextPos = SearchNextPos(lpszParamCgi.Get()+dwPos,FALSE);
        if ( dwPos+dwVarNameLen >= dwLineLen )
            break;

        memcpy(lpszProvCmp,lpszParamCgi.Get()+dwPos,(dwVarNameLen+1)*sizeof(_TCHAR));
        if ( _tcsicmp(lpszProvCmp,lpszVarForCmp) == 0 ) {
            DWORD dwLenContent = dwNextPos-(dwVarNameLen+1);
            ret.Alloc(dwLenContent+0x10);
            ret.Add(lpszParamCgi.Get()+dwPos+dwVarNameLen+1,dwLenContent);
            if ( fMimeDecode ) {
                url_decode(ret.Get());
                ret.SetLength();
            }
            break;
        }
        dwPos += dwNextPos+1;
    }
    lpAlloc.Free();
}

BOOL BuildMessageAndCmdLine(Buf &lpszParamCgi, LPCTSTR lpszPathTranslated, Buf &lpszCmdBlat, Buf &lpszMessage)
{
    DWORD dwSize=0;
    Buf   lpszParamFile;
    Buf   lpszValue;

    lpszMessage.Clear();
    lpszParamFile.Clear();
    lpszValue.Clear();

    if ( _tcslen(lpszPathTranslated) > 0 )
        if ( getFileSize(lpszPathTranslated,dwSize) )
            if ( dwSize > 0 ) {
                WinFile hf;
                if ( hf.OpenThisFile(lpszPathTranslated) ) {
                    DWORD dwSizeRead = 0;
                    lpszParamFile.Alloc(dwSize+10);
                    hf.ReadThisFile(lpszParamFile.Get(),dwSize,&dwSizeRead,NULL);
                    *(lpszParamFile.Get()+dwSizeRead) = __T('\0');
                    lpszParamFile.SetLength(dwSizeRead);
                    hf.Close();
                }
            }

    if ( !lpszParamFile.Length() ) {
        Buf lpszNewParamFile;

        lpszNewParamFile.Add(__T("-"));
        _BLATOPTIONS * pbvo = blatOptionsList;

        for ( ; ; pbvo++ ) {
            if ( !pbvo->optionString &&
                 !pbvo->preprocess   &&
                 !pbvo->additionArgC &&
                 !pbvo->initFunction &&
                 !pbvo->usageText    )
                break;

/*
 * If .szCgiEntry == NULL (0), there is no alternate CGI option to match.
 * If .szCgiEntry == 1, then the option is blocked from CGI access.
 */
            if ( pbvo->szCgiEntry == (LPTSTR)1 )
                continue;

            _TCHAR szVarNamePrefixed[128];

            lpszValue.Clear();
            if ( pbvo->szCgiEntry ) {
                SearchVar(lpszParamCgi, pbvo->szCgiEntry, TRUE, lpszValue);
                if ( !lpszValue.Length() ) {
                    _stprintf(szVarNamePrefixed, __T("Blat_%s"), pbvo->szCgiEntry);
                    SearchVar(lpszParamCgi, szVarNamePrefixed, TRUE, lpszValue);
                }
            }

            if ( !lpszValue.Length() ) {
                if ( !pbvo->optionString )
                    continue;

                SearchVar(lpszParamCgi, &pbvo->optionString[1], TRUE, lpszValue);
                if ( !lpszValue.Length() ) {
                    _stprintf(szVarNamePrefixed, __T("Blat_%s"), &pbvo->optionString[1]);
                    SearchVar(lpszParamCgi, szVarNamePrefixed, TRUE, lpszValue);
                }
            }

            if ( lpszValue.Length() ) {
                DWORD dwLen = (DWORD)lpszValue.Length();
                DWORD i;

                for ( i = 0; i < dwLen; i++ )
                    if ( (*(lpszValue.Get()+i)) == __T('"') )
                        (*(lpszValue.Get()+i)) = __T('\'');  // to avoid security problem, like including other parameter like -attach

                if ( pbvo->additionArgC ) {
                    lpszNewParamFile.Add(__T(" "));
                    lpszNewParamFile.Add(pbvo->optionString);
                    lpszNewParamFile.Add(__T(" \""));
                    lpszNewParamFile.Add(lpszValue.Get());
                    lpszNewParamFile.Add(__T("\""));
                } else {
                    if ( dwLen > 0 )
                        /*
                         * Assume that by finding the boolean option specified, the site developer wants to use the option.
                         * Therefore, unless we find the developer specifically tells us 'N', we will presume the developer
                         * goofed and forgot to mention 'Y'.
                         */
                        if ( ((*lpszValue.Get()) != __T('N')) && ((*lpszValue.Get()) != __T('n')) && ((*lpszValue.Get()) != __T('0')) ) {
                            lpszNewParamFile.Add(__T(" "));
                            lpszNewParamFile.Add(pbvo->optionString);
                        }
                }
                lpszValue.Clear();
            }
        }
        lpszParamFile.Move(lpszNewParamFile);
        lpszNewParamFile.Free();
    }
    lpszCmdBlat.Move(lpszParamFile);

    //tprintf(__T("command : \"%s\"\n"),lpszCmdBlat);

    lpszMessage.Add(__T(""));

    DWORD dwPos     = 0;
    DWORD dwLineLen = (DWORD)lpszParamCgi.Length();

    while ( dwPos < dwLineLen ) {
        _TCHAR szNameForCmp[7];
        DWORD  dwEndVar;
        DWORD  dwEndVarName;
        BOOL   fCopyCurrentVar;

        dwEndVar        = SearchNextPos(lpszParamCgi.Get()+dwPos,FALSE);
        dwEndVarName    = SearchNextPos(lpszParamCgi.Get()+dwPos,TRUE);
        fCopyCurrentVar = TRUE;

        if ( dwEndVarName > 5 ) {
            memcpy(szNameForCmp,lpszParamCgi.Get()+dwPos,5*sizeof(_TCHAR));
            szNameForCmp[5] = __T('\0');
            fCopyCurrentVar = (lstrcmpi(szNameForCmp,__T("Blat_")) != 0);
        }
        if ( fCopyCurrentVar ) {
            Buf lpCurVar;

            lpCurVar.Alloc(dwEndVar+0x10);
            lpCurVar.Clear();
            memcpy(lpCurVar.Get(),lpszParamCgi.Get()+dwPos,dwEndVar*sizeof(_TCHAR));
            lpCurVar.SetLength(dwEndVar);
            *lpCurVar.GetTail() = __T('\0');
            url_decode(lpCurVar.Get());
            lpszMessage.Add(lpCurVar.Get());
            lpszMessage.Add(__T("\r\n"));
            lpCurVar.Free();
        }
        dwPos += dwEndVar+1;
    }

    lpszValue.Free();
    lpszParamFile.Free();
    return TRUE;
}

DWORD WINAPI ReadCommandLine(LPTSTR szParcLine, int & argc, LPTSTR* &argv)
{

    DWORD dwCurLigne,dwCurCol;
    BOOL fInQuote = FALSE;
    //DWORD nNb;
    dwCurLigne = 1;
    dwCurCol = 0;
    DWORD  dwCurLigAllocated=0x10;
    LPTSTR lpszOldArgv=*argv;                             // exe name

    argv  = (LPTSTR*)malloc(sizeof(LPTSTR)*3);
    *argv = (LPTSTR )malloc((_tcslen(lpszOldArgv)+10)*sizeof(_TCHAR));
    _tcscpy(*argv,lpszOldArgv);

    *(argv+1)  = (LPTSTR)malloc((dwCurLigAllocated+10)*sizeof(_TCHAR));
    *(argv+2)  = NULL;
    **(argv+1) = __T('\0');



    while ( ((*szParcLine) != __T('\0')) && ((*szParcLine) != __T('\r')) && ((*szParcLine) != __T('\n')) ) {
        _TCHAR c = (*szParcLine);
        if ( c == __T('"') )
            fInQuote = ! fInQuote;
        else
        if ( (c == __T(' ')) && (!fInQuote) ) {     // && (dwCurLigne+1 < MAXPARAM))
            argv = (LPTSTR*)realloc(argv,sizeof(LPTSTR)*(dwCurLigne+0x10));
            dwCurLigne++;
            dwCurLigAllocated    = 0x10;
            *(argv+dwCurLigne)   = (LPTSTR)malloc((dwCurLigAllocated+10)*sizeof(_TCHAR));
            *(argv+dwCurLigne+1) = NULL;

            dwCurCol = 0;
        } else {
            LPTSTR lpszCurLigne;
            if ( dwCurCol >= dwCurLigAllocated ) {
                dwCurLigAllocated += 0x20;
                *(argv+dwCurLigne) = (LPTSTR)realloc(*(argv+dwCurLigne),(dwCurLigAllocated+10)*sizeof(_TCHAR));
            }
            lpszCurLigne = *(argv+dwCurLigne);
            *(lpszCurLigne+dwCurCol) = c;
            dwCurCol++;
            *(lpszCurLigne+dwCurCol) = __T('\0');
        }
        szParcLine++;
    }

    if ( dwCurCol > 0 )
        dwCurLigne++;

    argc = (int)dwCurLigne;
    return dwCurLigne;
}


BOOL DoCgiWork(int & argc, LPTSTR*  &argv, Buf &lpszMessage,
                    Buf & lpszCgiSuccessUrl, Buf &lpszCgiFailureUrl,
                    Buf & lpszFirstReceivedData, Buf &lpszOtherHeader)
{
    Buf lpszMethod;
    Buf lpszPost;
    Buf lpszParamCgi;
    Buf lpszQueryString;
    Buf lpszPathTranslated;
    Buf lpszCmdBlat;

    lpszMessage.Clear();
    lpszFirstReceivedData.Clear();
    lpszOtherHeader.Clear();

    GetEnv(__T("REQUEST_METHOD"), lpszMethod);
    lpszPost=__T("");
    if ( lstrcmpi(lpszMethod.Get(),__T("POST")) == 0)
        ReadPostData(lpszPost);

    GetEnv(__T("QUERY_STRING"), lpszQueryString);
//    lpszParamCgi.AllocExact(lpszQueryString.Length()+lpszPost.Length()+10);

    lpszParamCgi.Add( lpszQueryString );
    if (lpszQueryString.Length() && lpszPost.Length())
        lpszParamCgi.Add( __T('&') );

    lpszParamCgi.Add( lpszPost );
    GetEnv(__T("PATH_TRANSLATED"), lpszPathTranslated);

    BuildMessageAndCmdLine(lpszParamCgi,lpszPathTranslated.Get(),lpszCmdBlat,lpszMessage);

    SearchVar(lpszParamCgi, __T("BLAT_SUCCESS"), TRUE, lpszCgiSuccessUrl);
    SearchVar(lpszParamCgi, __T("BLAT_FAILURE"), TRUE, lpszCgiFailureUrl);

    // now replace %__% by var

    DWORD dwPos     = 0;
    DWORD dwLineLen = (DWORD)lpszCmdBlat.Length();

    while ( dwPos < dwLineLen ) {
        if ( *(lpszCmdBlat.Get()+dwPos) == __T('%') ) {
            Buf   lpVarNameForSearch;
            Buf   lpContentVar;
            DWORD dwEnd;

            dwEnd = (SearchNextPercent(lpszCmdBlat.Get()+dwPos+1));
            if ( *(lpszCmdBlat.Get()+dwPos+1+dwEnd) == __T('\0') ) {
                lpContentVar.Free();
                break;
            }

            lpVarNameForSearch.Alloc(dwEnd+0x10);
            lpVarNameForSearch.Clear();
            memcpy(lpVarNameForSearch.Get(),lpszCmdBlat.Get()+dwPos+1,dwEnd*sizeof(_TCHAR));
            lpVarNameForSearch.SetLength(dwEnd);
            *lpVarNameForSearch.GetTail() = __T('\0');

            SearchVar(lpszParamCgi, lpVarNameForSearch.Get(), TRUE, lpContentVar);
            if ( lpContentVar.Length() ) {
                DWORD dwLenContentVar;

                dwLenContentVar = (DWORD)lpContentVar.Length();
                lpszCmdBlat.Alloc(dwLineLen+dwLenContentVar+0x10);
                memmove(lpszCmdBlat.Get()+dwPos+dwLenContentVar,lpszCmdBlat.Get()+dwPos+dwEnd+2,(dwLineLen-(dwPos+dwEnd+1))*sizeof(_TCHAR));
                memcpy(lpszCmdBlat.Get()+dwPos,lpContentVar.Get(),dwLenContentVar*sizeof(_TCHAR));
                lpszCmdBlat.SetLength();
                dwLineLen = (DWORD)lpszCmdBlat.Length();

                dwPos += dwLenContentVar;
            } else
                dwPos += dwEnd + 1;

            lpVarNameForSearch.Free();
            lpContentVar.Free();
        } else
            dwPos++;
    }

    ReadCommandLine(lpszCmdBlat.Get(),argc,argv);
    lpszCmdBlat.Clear();
    lpszParamCgi.Clear();

    //LPTSTR lpszRemoteHost=GetEnv(__T("REMOTE_HOST"));
    Buf    lpszRemoteAddr;       GetEnv(__T("REMOTE_ADDR"),          lpszRemoteAddr);
    Buf    lpszServerName;       GetEnv(__T("SERVER_NAME"),          lpszServerName);
    Buf    lpszHttpVia;          GetEnv(__T("HTTP_VIA"),             lpszHttpVia);
    Buf    lpszHttpForwarded;    GetEnv(__T("HTTP_FORWARDED"),       lpszHttpForwarded);
    Buf    lpszHttpForwardedFor; GetEnv(__T("HTTP_X_FORWARDED_FOR"), lpszHttpForwardedFor);
    Buf    lpszHttpUserAgent;    GetEnv(__T("HTTP_USER_AGENT"),      lpszHttpUserAgent);
    Buf    lpszHttpReferer;      GetEnv(__T("HTTP_REFERER"),         lpszHttpReferer);
    _TCHAR tmpBuf[0x2000];

    if ( *lpszHttpUserAgent.Get() ) {
        _stprintf(tmpBuf, __T("X-Web-Browser: Send using %s\r\n"), lpszHttpUserAgent.Get());
        lpszOtherHeader.Add(tmpBuf);
    }

    if ( *lpszHttpForwarded.Get() ) {
        _stprintf(tmpBuf, __T("X-Forwarded: %s\r\n"), lpszHttpForwarded.Get());
        lpszOtherHeader.Add(tmpBuf);
    }

    if ( *lpszHttpForwardedFor.Get() ) {
        _stprintf(tmpBuf, __T("X-X-Forwarded-For: %s\r\n"), lpszHttpForwardedFor.Get());
        lpszOtherHeader.Add(tmpBuf);
    }

    if ( *lpszHttpVia.Get() ) {
        _stprintf(tmpBuf, __T("X-Via: %s\r\n"), lpszHttpVia.Get());
        lpszOtherHeader.Add(tmpBuf);
    }

    if ( *lpszHttpReferer.Get() ) {
        _stprintf(tmpBuf, __T("X-Referer: %s\r\n"), lpszHttpReferer.Get());
        lpszOtherHeader.Add(tmpBuf);
    }

    _stprintf(tmpBuf, __T("Received: from %s by %s with HTTP; "),
              lpszRemoteAddr.Get(), lpszServerName.Get());
    lpszFirstReceivedData.Add(tmpBuf);

    lpszRemoteAddr.Free();
    lpszServerName.Free();
    lpszHttpVia.Free();
    lpszHttpForwarded.Free();
    lpszHttpForwardedFor.Free();
    lpszHttpUserAgent.Free();
    lpszHttpReferer.Free();

    lpszMethod.Free();
    lpszPost.Free();
    lpszParamCgi.Free();
    lpszQueryString.Free();
    lpszPathTranslated.Free();
    lpszCmdBlat.Free();

    return TRUE;
}

/***************************************************************************/
// end of CGI stuff
/***************************************************************************/
