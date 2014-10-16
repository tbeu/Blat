// CGI Support by Gilles Vollant <info@winimage.com>, February 1999
// Added in Blat Version 1.8.3

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "blat.h"
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


BOOL getFileSize(LPCSTR lpFn, DWORD &dwSize)
{
    WIN32_FIND_DATA ffblk;
    HANDLE hFind;
    char szName[MAX_PATH+1];


    strcpy(szName,lpFn);
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
void GetEnv(LPSTR lpszEnvVar, Buf &buf) {

    DWORD dwLen = 0;

    if ( lpszEnvVar ) {
        dwLen = GetEnvironmentVariable(lpszEnvVar, NULL, 0);
    }

    if ( dwLen == 0 )
        buf = "";
    else {
        buf.AllocExact(dwLen);
        GetEnvironmentVariable(lpszEnvVar, buf.Get(), dwLen);
    }
}

void ReadPostData(Buf &buf)
{
    DWORD dwStep=4096;
    DWORD  dwReadThis;
    size_t dwThisStep;
    DWORD dwTotalBytes;
    Buf   lpszContentLength;

    GetEnv("CONTENT_LENGTH", lpszContentLength);
    dwTotalBytes = atol(lpszContentLength.Get());
    buf.Clear();
    buf.AllocExact(dwTotalBytes + 1);

    do {
        dwThisStep = min(dwTotalBytes-buf.Length(),dwStep);
        dwReadThis = (DWORD) dwThisStep;
        if ( dwThisStep > 0 ) {
            dwReadThis = 0;
            if ( !ReadFile(GetStdHandle(STD_INPUT_HANDLE),buf.GetTail(),dwReadThis,
                                        &dwReadThis,NULL) )
                dwReadThis = 0;
        }
        buf.SetLength(buf.Length() + dwReadThis);

    } while ( (dwReadThis==dwThisStep) && (buf.Length()<dwTotalBytes) );

    *buf.GetTail()='\0';
}

static int hextoint( char c )
{
    if ( c >= '0' && c <= '9' )
        return c-'0';

    return toupper(c)-'A'+10;
}

static void url_decode( char * cp )
{
    for ( ; *cp; cp++ ) {
        if ( *cp == '+' )
            *cp = ' ';
        else
            if ( *cp == '%' ) {
                *cp = (char)(hextoint(*(cp+1)) * 16 + hextoint(*(cp+2)));
                memmove( cp+1, cp+3, strlen(cp+3)+1 );
            }
    }
}

DWORD SearchNextPos(LPCSTR lpszParamCgi, BOOL fSearchEqual)
{
    char  cSup;
    DWORD dwNext;

    if ( fSearchEqual )
        cSup = '=';
    else
        cSup = '\0';

    dwNext = 0;
    while ( (*(lpszParamCgi+dwNext) != '&') && (*(lpszParamCgi+dwNext) != '\0') && (*(lpszParamCgi+dwNext) != cSup) )
        dwNext++;

    return dwNext;
}

DWORD SearchNextPercent(LPCSTR lpszParamCgi)
{
    DWORD dwNext = 0;

    while ( (*(lpszParamCgi+dwNext) != '%') && (*(lpszParamCgi+dwNext) != '\0') )
        dwNext++;
    return dwNext;
}

void SearchVar(Buf &lpszParamCgi,LPCSTR lpszVarName,BOOL fMimeDecode, Buf &ret)
{
    LPSTR lpszProvCmp,lpszVarForCmp;
    DWORD dwVarNameLen=lstrlen(lpszVarName);
    DWORD dwLineLen=lpszParamCgi.Length();
    DWORD dwPos=0;
    ret.Clear();

    Buf lpAlloc;
    lpAlloc.AllocExact((dwVarNameLen+0x10)*2);
    lpszProvCmp=lpAlloc.Get();
    lpszVarForCmp=lpAlloc.Get()+(((dwVarNameLen+7)/4)*4);

    strcpy(lpszVarForCmp,lpszVarName);
    strcat(lpszVarForCmp,"=");
    *(lpszProvCmp+dwVarNameLen+1) = '\0';

    while ( dwPos < dwLineLen ) {
        DWORD dwNextPos = SearchNextPos(lpszParamCgi.Get()+dwPos,FALSE);
        if ( dwPos+dwVarNameLen >= dwLineLen )
            break;

        memcpy(lpszProvCmp,lpszParamCgi.Get()+dwPos,dwVarNameLen+1);
        if ( _stricmp(lpszProvCmp,lpszVarForCmp) == 0 ) {
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
}

BOOL BuildMessageAndCmdLine(Buf &lpszParamCgi,LPCSTR lpszPathTranslated, Buf &lpszCmdBlat, Buf &lpszMessage)
{
    DWORD dwSize=0;
    Buf lpszParamFile;
    lpszMessage.Clear();

    if ( lstrlen(lpszPathTranslated) > 0 )
        if ( getFileSize(lpszPathTranslated,dwSize) )
            if ( dwSize > 0 ) {
                WinFile hf;
                if ( hf.OpenThisFile(lpszPathTranslated) ) {
                    DWORD dwSizeRead = 0;
                    lpszParamFile.Alloc(dwSize+10);
                    hf.ReadThisFile(lpszParamFile.Get(),dwSize,&dwSizeRead,NULL);
                    *(lpszParamFile.Get()+dwSizeRead)='\0';
                    lpszParamFile.SetLength(dwSizeRead);
                    hf.Close();
                }
            }

    if ( !lpszParamFile.Length() )
        lpszParamFile.Add("");

    if ( !lpszParamFile.Length() ) {
        Buf lpszNewParamFile;

        lpszNewParamFile.Add("-");
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
            if ( pbvo->szCgiEntry == (char *)1 )
                continue;

            Buf lpszValue;
            char szVarNamePrefixed[128];

            if ( pbvo->szCgiEntry ) {
                SearchVar(lpszParamCgi, pbvo->szCgiEntry, TRUE, lpszValue);
                if ( !lpszValue.Length() ) {
                    wsprintf(szVarNamePrefixed, "Blat_%s", pbvo->szCgiEntry);
                    SearchVar(lpszParamCgi, szVarNamePrefixed, TRUE, lpszValue);
                }
            }

            if ( !lpszValue.Length() ) {
                if ( !pbvo->optionString )
                    continue;

                SearchVar(lpszParamCgi, &pbvo->optionString[1], TRUE, lpszValue);
                if ( !lpszValue.Length() ) {
                    wsprintf(szVarNamePrefixed, "Blat_%s", &pbvo->optionString[1]);
                    SearchVar(lpszParamCgi, szVarNamePrefixed, TRUE, lpszValue);
                }
            }

            if ( lpszValue.Length() ) {
                DWORD dwLen = lpszValue.Length();
                DWORD i;

                for ( i = 0; i < dwLen; i++ )
                    if ( (*(lpszValue.Get()+i)) == '"' )
                        (*(lpszValue.Get()+i)) = '\'';  // to avoid security problem, like including other parameter like -attach

                if ( pbvo->additionArgC ) {
                    lpszNewParamFile.Add(" ");
                    lpszNewParamFile.Add(pbvo->optionString);
                    lpszNewParamFile.Add(" \"");
                    lpszNewParamFile.Add(lpszValue.Get());
                    lpszNewParamFile.Add("\"");
                } else {
                    if ( dwLen > 0 )
                        /*
                         * Assume that by finding the boolean option specified, the site developer wants to use the option.
                         * Therefore, unless we find the developer specifically tells us 'N', we will presume the developer
                         * goofed and forgot to mention 'Y'.
                         */
                        if ( ((*lpszValue.Get())!='N') && ((*lpszValue.Get())!='n') && ((*lpszValue.Get())!='0') ) {
                            lpszNewParamFile.Add(" ");
                            lpszNewParamFile.Add(pbvo->optionString);
                        }
                }
                lpszValue.Clear();
            }
        }
        lpszParamFile.Move(lpszNewParamFile);
    }
    lpszCmdBlat.Move(lpszParamFile);

    //printf("command : \"%s\"\n",lpszCmdBlat);

    lpszMessage.Add("");

    DWORD dwPos     = 0;
    DWORD dwLineLen = lpszParamCgi.Length();

    while ( dwPos < dwLineLen ) {
        char  szNameForCmp[7];
        DWORD dwEndVar;
        DWORD dwEndVarName;
        BOOL  fCopyCurrentVar;

        dwEndVar        = SearchNextPos(lpszParamCgi.Get()+dwPos,FALSE);
        dwEndVarName    = SearchNextPos(lpszParamCgi.Get()+dwPos,TRUE);
        fCopyCurrentVar = TRUE;

        if ( dwEndVarName > 5 ) {
            memcpy(szNameForCmp,lpszParamCgi.Get()+dwPos,5);
            szNameForCmp[5] = '\0';
            fCopyCurrentVar = (_stricmp(szNameForCmp,"Blat_") != 0);
        }
        if ( fCopyCurrentVar ) {
            Buf lpCurVar(dwEndVar+0x10);
            memcpy(lpCurVar.Get(),lpszParamCgi.Get()+dwPos,dwEndVar);
            lpCurVar.SetLength(dwEndVar);
            *lpCurVar.GetTail()='\0';
            url_decode(lpCurVar.Get());
            lpszMessage.Add(lpCurVar.Get());
            lpszMessage.Add("\r\n");
        }
        dwPos += dwEndVar+1;

    }

    return TRUE;
}

DWORD WINAPI ReadCommandLine(LPSTR szParcLine,int & argc, char** &argv)
{

    DWORD dwCurLigne,dwCurCol;
    BOOL fInQuote = FALSE;
    //DWORD nNb;
    dwCurLigne = 1;
    dwCurCol = 0;
    DWORD dwCurLigAllocated=0x10;
    LPSTR lpszOldArgv=*argv;                             // exe name

    argv  = (char**)malloc(sizeof(char*)*3);
    *argv = (char* )malloc(lstrlen(lpszOldArgv)+10);
    strcpy(*argv,lpszOldArgv);

    *(argv+1)  = (char*)malloc(dwCurLigAllocated+10);
    *(argv+2)  = NULL;
    **(argv+1) = '\0';



    while ( ((*szParcLine) != '\0') && ((*szParcLine) != '\x0a') && ((*szParcLine) != '\x0d') ) {
        char c = (*szParcLine);
        if ( c == '"' )
            fInQuote = ! fInQuote;
        else
            if ( (c == ' ') && (!fInQuote) ) {     // && (dwCurLigne+1 < MAXPARAM))
                argv = (char**)realloc(argv,sizeof(char*)*(dwCurLigne+0x10));
                dwCurLigne++;
                dwCurLigAllocated    = 0x10;
                *(argv+dwCurLigne)   = (char*)malloc(dwCurLigAllocated+10);
                *(argv+dwCurLigne+1) = NULL;

                dwCurCol = 0;
            } else {
                char * lpszCurLigne;
                if ( dwCurCol >= dwCurLigAllocated ) {
                    dwCurLigAllocated += 0x20;
                    *(argv+dwCurLigne) = (char*)realloc(*(argv+dwCurLigne),dwCurLigAllocated+10);
                }
                lpszCurLigne = *(argv+dwCurLigne);
                *(lpszCurLigne+dwCurCol) = c;
                dwCurCol++;
                *(lpszCurLigne+dwCurCol) = '\0';
            }
            szParcLine++;
    }

    if ( dwCurCol > 0 )
        dwCurLigne++;

    argc = dwCurLigne;
    return dwCurLigne;
}


BOOL DoCgiWork(int & argc, char**  &argv, Buf &lpszMessage,
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

    GetEnv("REQUEST_METHOD", lpszMethod);
    lpszPost="";
    if ( _stricmp(lpszMethod.Get(),"POST") == 0)
        ReadPostData(lpszPost);

    GetEnv("QUERY_STRING", lpszQueryString);
//    lpszParamCgi.AllocExact(lpszQueryString.Length()+lpszPost.Length()+10);

    lpszParamCgi.Add( lpszQueryString.Get() );
    if (lpszQueryString.Length() && lpszPost.Length())
        lpszParamCgi.Add( '&' );

    lpszParamCgi.Add( lpszPost.Get() );
    GetEnv("PATH_TRANSLATED", lpszPathTranslated);

    BuildMessageAndCmdLine(lpszParamCgi,lpszPathTranslated.Get(),lpszCmdBlat,lpszMessage);

    SearchVar(lpszParamCgi, "BLAT_SUCCESS", TRUE, lpszCgiSuccessUrl);
    SearchVar(lpszParamCgi, "BLAT_FAILURE", TRUE, lpszCgiFailureUrl);

    // now replace %__% by var

    DWORD dwPos     = 0;
    DWORD dwLineLen = lpszCmdBlat.Length();

    while ( dwPos < dwLineLen ) {
        if ( *(lpszCmdBlat.Get()+dwPos) == '%' ) {
            Buf   lpContentVar;
            DWORD dwEnd;

            dwEnd = (SearchNextPercent(lpszCmdBlat.Get()+dwPos+1));
            if ( *(lpszCmdBlat.Get()+dwPos+1+dwEnd) == '\0' )
                break;

            Buf lpVarNameForSearch(dwEnd+0x10);
            memcpy(lpVarNameForSearch.Get(),lpszCmdBlat.Get()+dwPos+1,dwEnd);
            lpVarNameForSearch.SetLength(dwEnd);
            *lpVarNameForSearch.GetTail() = '\0';

            SearchVar(lpszParamCgi, lpVarNameForSearch.Get(), TRUE, lpContentVar);
            if ( lpContentVar.Length() ) {
                DWORD dwLenContentVar;

                dwLenContentVar = lpContentVar.Length();
                lpszCmdBlat.Alloc(dwLineLen+dwLenContentVar+0x10);
                memmove(lpszCmdBlat.Get()+dwPos+dwLenContentVar,lpszCmdBlat.Get()+dwPos+dwEnd+2,dwLineLen-(dwPos+dwEnd+1));
                memcpy(lpszCmdBlat.Get()+dwPos,lpContentVar.Get(),dwLenContentVar);
                dwLineLen = lstrlen(lpszCmdBlat.Get());

                dwPos += dwLenContentVar;
            } else
                dwPos += dwEnd + 1;
        } else
            dwPos++;
    }

    ReadCommandLine(lpszCmdBlat.Get(),argc,argv);
    lpszCmdBlat.Clear();

    lpszParamCgi.Clear();

    //LPSTR lpszRemoteHost=GetEnv("REMOTE_HOST");
    Buf  lpszRemoteAddr;       GetEnv("REMOTE_ADDR",          lpszRemoteAddr);
    Buf  lpszServerName;       GetEnv("SERVER_NAME",          lpszServerName);
    Buf  lpszHttpVia;          GetEnv("HTTP_VIA",             lpszHttpVia);
    Buf  lpszHttpForwarded;    GetEnv("HTTP_FORWARDED",       lpszHttpForwarded);
    Buf  lpszHttpForwardedFor; GetEnv("HTTP_X_FORWARDED_FOR", lpszHttpForwardedFor);
    Buf  lpszHttpUserAgent;    GetEnv("HTTP_USER_AGENT",      lpszHttpUserAgent);
    Buf  lpszHttpReferer;      GetEnv("HTTP_REFERER",         lpszHttpReferer);
    char tmpBuf[0x2000];

    if ( *lpszHttpUserAgent.Get() ) {
        wsprintf(tmpBuf, "X-Web-Browser: Send using %s\r\n", lpszHttpUserAgent.Get());
        lpszOtherHeader.Add(tmpBuf);
    }

    if ( *lpszHttpForwarded.Get() ) {
        wsprintf(tmpBuf, "X-Forwarded: %s\r\n", lpszHttpForwarded.Get());
        lpszOtherHeader.Add(tmpBuf);
    }

    if ( *lpszHttpForwardedFor.Get() ) {
        wsprintf(tmpBuf, "X-X-Forwarded-For: %s\r\n", lpszHttpForwardedFor.Get());
        lpszOtherHeader.Add(tmpBuf);
    }

    if ( *lpszHttpVia.Get() ) {
        wsprintf(tmpBuf, "X-Via: %s\r\n", lpszHttpVia.Get());
        lpszOtherHeader.Add(tmpBuf);
    }

    if ( *lpszHttpReferer.Get() ) {
        wsprintf(tmpBuf, "X-Referer: %s\r\n", lpszHttpReferer.Get());
        lpszOtherHeader.Add(tmpBuf);
    }

    wsprintf(tmpBuf, "Received: from %s by %s with HTTP; ",
             lpszRemoteAddr.Get(), lpszServerName.Get());
    lpszFirstReceivedData.Add(tmpBuf);

    return TRUE;
}

/***************************************************************************/
// end of CGI stuff
/***************************************************************************/
