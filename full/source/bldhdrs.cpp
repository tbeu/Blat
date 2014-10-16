/*
    blathdrs.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __WATCOMC__
#include <mapi.h>
#else
#include <mapiwin.h>
#endif

#include "blat.h"
#include "gensock.h"

extern LPCSTR GetNameWithoutPath(LPCSTR lpFn);
extern int    CheckIfNeedQuotedPrintable(unsigned char *pszStr, int inHeader);
extern int    GetLengthQuotedPrintable(unsigned char *pszStr, int inHeader);
extern void   ConvertToQuotedPrintable(Buf & source, Buf & out, int inHeader);
extern void   base64_encode(Buf & source, Buf & out, int inclCrLf, int inclPad);

#if SMART_CONTENT_TYPE
extern char * getShortFileName (char * fileName);
extern void   getContentType(char *sDestBuffer, char *foundType, char *defaultType, char *sFileName);
#endif
#if SUPPORT_SALUTATIONS
extern void   find_and_strip_salutation( Buf & email_addresses );
#endif

extern char         blatVersion[];
extern char         blatVersionSuf[];

#if INCLUDE_NNTP
extern char         NNTPHost[];
extern Buf          groups;
#endif

extern Buf          destination;
extern Buf          cc_list;
extern Buf          bcc_list;
extern char         loginname[];    // RFC 821 MAIL From. <loginname>. There are some inconsistencies in usage
extern char         senderid[];     // Inconsistent use in Blat for some RFC 822 Field definitions
extern char         sendername[];   // RFC 822 Sender: <sendername>
extern char         fromid[];       // RFC 822 From: <fromid>
extern char         replytoid[];    // RFC 822 Reply-To: <replytoid>
extern char         returnpathid[]; // RFC 822 Return-Path: <returnpath>
extern char         textmode[];
extern char         bodyFilename[];
extern char         my_hostname[];
extern char         formattedContent;
extern char         mime;
extern int          attach;
extern char         needBoundary;
extern char         ConsoleDone;
extern char         haveEmbedded;
extern char         haveAttachments;
extern Buf          alternateText;
extern Buf          TempConsole;

#if BLAT_LITE
#else
extern char         xheaders[];
extern char         aheaders1[];
extern char         aheaders2[];

extern char         uuencode;
extern char         base64;
extern char         yEnc;

extern char         eightBitMimeSupported;
extern char         eightBitMimeRequested;
extern char         binaryMimeSupported;
//extern char         binaryMimeRequested;
#endif

extern char         charset[];          // Added 25 Apr 2001 Tim Charron (default ISO-8859-1)
extern const char * days[];
extern const char * stdinFileName;
extern const char * defaultCharset;
extern char         subject[];

#if BLAT_LITE
#else
extern char         organization[];

char         forcedHeaderEncoding = 0;
char         noheader             = 0;
#endif

char         impersonating        = FALSE;
char         returnreceipt        = FALSE;
char         disposition          = FALSE;
char         ssubject             = FALSE;
char         includeUserAgent     = FALSE;
char         sendUndisclosed      = FALSE;

char         priority[2];
char         sensitivity[2];

static const char   abclist[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

static const char * months[]  = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};


void incrementBoundary( char boundary[] )
{
    int x;

    for ( x = 0; x < 62; x++ )
        if ( boundary[20] == abclist[x] )
            break;

    boundary[20] = abclist[(x + 1) % 62];
}


void decrementBoundary( char boundary[] )
{
    int x;

    for ( x = 0; x < 62; x++ )
        if ( boundary[20] == abclist[x] )
            break;

    if ( --x < 0 )
        x = 61;

    boundary[20] = abclist[x];
}


void addStringToHeaderNoQuoting(char * string, Buf & outS, int & headerLen, int linewrap )
{
    // quoting not necessary
    char * pStr1;
    char * pStr2;
    char * pStr;

    pStr = pStr1 = pStr2 = string;
    for ( ; ; ) {
        for ( ; ; ) {
            pStr = strchr(pStr, ' ');
            if ( !pStr )
                break;

            if ( linewrap && ((headerLen + pStr - pStr2 ) >= 75) ) {
                if ( pStr1 != pStr2 )
                    pStr = pStr1;
                break;
            }
            pStr1 = pStr;
            pStr++;
        }
        if ( !pStr )
            break;

        outS.Add( pStr2, (int)(pStr - pStr2) );
        outS.Add( "\r\n " );
        headerLen = 1;
        pStr++;
        pStr1 = pStr2 = pStr;
    }
    headerLen += (int)strlen( pStr2 );
    outS.Add( pStr2 );
}


/*
 * outString==NULL ..modify in place
 * outString!=NULL...don't modify string.  Put new string in outString.
 *
 * creates "=?charset?q?text?=" output from 'string', as defined by RFCs 2045 and 2046,
 *      or "=?charset?b?text?=".
 */

void fixup(char * string, Buf * outString, int headerLen, int linewrap )
{
    int    doBase64;
    int    i, qLen, bLen;
    Buf    outS;
    Buf    fixupString;
    Buf    tempstring;
    size_t charsetLen;
    int    stringOffset;

#if BLAT_LITE
#else
    char   savedEightBitMimeSupported;
    char   savedBinaryMimeSupported;

    savedEightBitMimeSupported = eightBitMimeSupported;
    savedBinaryMimeSupported   = binaryMimeSupported;
#endif

    charsetLen = strlen( charset );
    if ( !charsetLen )
        charsetLen = strlen( defaultCharset );

    charsetLen += 7;    // =? ?q? ?=

    stringOffset = 0;
    tempstring.Add( string );
    outS = "";

    if ( CheckIfNeedQuotedPrintable((unsigned char *)tempstring.Get(), TRUE ) ) {
        Buf             tmpstr;
        unsigned char * pStr;

        fixupString.Clear();

        i = tempstring.Length();
        qLen = GetLengthQuotedPrintable((unsigned char *)tempstring.Get(), TRUE );
        bLen = ((i / 3) * 4) + ((i % 3) ? 4 : 0);

#if BLAT_LITE
#else
        eightBitMimeSupported = FALSE;
        binaryMimeSupported   = 0;

        if ( forcedHeaderEncoding == 'q' ) {
            bLen = qLen + 1;
        }
        else
            if ( forcedHeaderEncoding == 'b' ) {
                qLen = bLen + 1;
            }
#endif
        if ( qLen <= bLen )
            doBase64 = FALSE;
        else
            doBase64 = TRUE;

        fixupString.Add( "=?" );
        if ( charset[0] )
            fixupString.Add(charset);
        else
            fixupString.Add(defaultCharset);

        if ( doBase64 ) {
            fixupString.Add( "?b?" );
            base64_encode(tempstring, tmpstr, FALSE, TRUE);
        } else {
            fixupString.Add( "?q?" );
            ConvertToQuotedPrintable(tempstring, tmpstr, TRUE );
        }

        outS.Add(fixupString);
        for ( ; linewrap && ((int)(fixupString.Length() + tmpstr.Length()) > (73-headerLen)); ) { // minimum fixup length too long?
            int x = 73-headerLen-fixupString.Length();

            pStr = (unsigned char *)tmpstr.Get();
            if ( doBase64 )
                x &= ~3;
            else {
                if ( pStr[x-2] == '=' )
                    x -= 2;
                else
                if ( pStr[x-1] == '=' )
                    x -= 1;
            }
            outS.Add( tmpstr.Get(), x );
            strcpy( tmpstr.Get(), (const char *)(pStr+x) );
            tmpstr.SetLength();
            outS.Add( "?=\r\n " );
            outS.Add( fixupString );
            headerLen = 1;
        }

        outS.Add( tmpstr );
        outS.Add( "?=" );
#if BLAT_LITE
#else
        eightBitMimeSupported = savedEightBitMimeSupported;
        binaryMimeSupported   = savedBinaryMimeSupported;
#endif
    } else {
        // quoting not necessary
        addStringToHeaderNoQuoting(tempstring.Get(), outS, headerLen, linewrap );
    }

    if ( outString == NULL ) {
        strcpy(string, outS.Get()); // Copy back to source string (calling program must ensure buffer is big enough)
    } else {
        outString->Clear();
        if ( outS.Length() )
            outString->Add(outS);
    }

    return;
}


void fixupEmailHeaders(char * string, Buf * outString, int headerLen, int linewrap )
{
    int    doBase64;
    int    i, qLen, bLen;
    Buf    outS;
    Buf    fixupString;
    Buf    tempstring;
    size_t charsetLen;
    int    stringOffset;
    Buf    tmpstr;
    Buf    t;
    char   c;
    char * pStr1;
    char * pStr2;
    char * pStr3;
    char * pStr4;

#if BLAT_LITE
#else
    char   savedEightBitMimeSupported;
    char   savedBinaryMimeSupported;

    savedEightBitMimeSupported = eightBitMimeSupported;
    savedBinaryMimeSupported   = binaryMimeSupported;
#endif

    charsetLen = strlen( charset );
    if ( !charsetLen )
        charsetLen = strlen( defaultCharset );

    charsetLen += 7;    // =? ?q? ?=

    stringOffset = 0;
    tempstring.Add( string );
    outS = "";

    for ( ; ; ) {
        tmpstr.Clear();
        pStr3 = 0;
        pStr2 = strchr( tempstring.Get(), '<' );
        if ( !pStr2 )
            break;

        pStr3 = strchr( pStr2, '>' );
        if ( !pStr3 )
            break;

        *pStr3 = 0;
        if ( strchr( pStr2, '@' ) ) {
            *pStr2 = 0;
            pStr4 = pStr2;
            for ( ; ; ) {
                if ( pStr4 == tempstring.Get() )
                    break;
                if ( pStr4[-1] != ' ' )
                    break;
                pStr4--;
            }
            *pStr4 = 0;
            i = (int)(pStr4 - tempstring.Get());

            fixupString.Clear();

            if ( i && CheckIfNeedQuotedPrintable((unsigned char *)tempstring.Get(), TRUE ) ) {

                qLen = GetLengthQuotedPrintable((unsigned char *)tempstring.Get(), TRUE );
                bLen = ((i / 3) * 4) + ((i % 3) ? 4 : 0);

#if BLAT_LITE
#else
                eightBitMimeSupported = FALSE;
                binaryMimeSupported   = 0;

                if ( forcedHeaderEncoding == 'q' ) {
                    bLen = qLen + 1;
                }
                else
                    if ( forcedHeaderEncoding == 'b' ) {
                        qLen = bLen + 1;
                    }
#endif
                if ( qLen <= bLen )
                    doBase64 = FALSE;
                else
                    doBase64 = TRUE;

                fixupString.Add( "=?" );
                if ( charset[0] )
                    fixupString.Add(charset);
                else
                    fixupString.Add(defaultCharset);

                t.Clear();
                t.Add( tempstring.Get() );

                if ( doBase64 ) {
                    fixupString.Add( "?b?" );
                    base64_encode(t, tmpstr, FALSE, TRUE);
                } else {
                    fixupString.Add( "?q?" );
                    ConvertToQuotedPrintable(t, tmpstr, TRUE );
                }

                outS.Add(fixupString);
                headerLen += fixupString.Length();
                for ( ; linewrap && ((int)tmpstr.Length() > (73-headerLen)); ) { // minimum fixup length too long?
                    int x = 73-headerLen;

                    pStr1 = tmpstr.Get();
                    if ( doBase64 )
                        x &= ~3;
                    else {
                        if ( pStr1[x-2] == '=' )
                            x -= 2;
                        else
                        if ( pStr1[x-1] == '=' )
                            x -= 1;
                    }
                    outS.Add( tmpstr.Get(), x );
                    strcpy( tmpstr.Get(), (const char *)(pStr1+x) );
                    tmpstr.SetLength();
                    outS.Add( "?=\r\n " );
                    outS.Add(fixupString);
                    headerLen = 1;
                }
                outS.Add( tmpstr );
                outS.Add( "?=" );
                headerLen += tmpstr.Length() + 2;
#if BLAT_LITE
#else
                eightBitMimeSupported = savedEightBitMimeSupported;
                binaryMimeSupported   = savedBinaryMimeSupported;
#endif
            }
            else {
                // quoting not necessary
                if ( i )
                    addStringToHeaderNoQuoting(tempstring.Get(), outS, headerLen, linewrap );
            }

            *pStr4 = ' ';
            *pStr2 = '<';
            *pStr3 = '>';

            c = pStr3[1];
            pStr3[1] = 0;

            if ( CheckIfNeedQuotedPrintable((unsigned char *)pStr4, TRUE ) ) {
                fixupString.Clear();
                i = (int)(pStr3 + 1 - pStr4);
                qLen = GetLengthQuotedPrintable((unsigned char *)pStr4, TRUE );
                bLen = ((i / 3) * 4) + ((i % 3) ? 4 : 0);

#if BLAT_LITE
#else
                eightBitMimeSupported = FALSE;
                binaryMimeSupported   = 0;

                if ( forcedHeaderEncoding == 'q' ) {
                    bLen = qLen + 1;
                }
                else
                    if ( forcedHeaderEncoding == 'b' ) {
                        qLen = bLen + 1;
                    }
#endif
                if ( qLen <= bLen )
                    doBase64 = FALSE;
                else
                    doBase64 = TRUE;

                fixupString.Add( "=?" );
                if ( charset[0] )
                    fixupString.Add(charset);
                else
                    fixupString.Add(defaultCharset);

                t.Clear();
                t.Add( pStr4 );
                if ( doBase64 ) {
                    fixupString.Add( "?b?" );
                    base64_encode(t, tmpstr, FALSE, TRUE);
                } else {
                    fixupString.Add( "?q?" );
                    ConvertToQuotedPrintable(t, tmpstr, TRUE );
                }

                outS.Add(fixupString);
                for ( ; linewrap && ((int)(fixupString.Length() + tmpstr.Length()) > (73-headerLen)); ) { // minimum fixup length too long?
                    int x = 73-headerLen-fixupString.Length();

                    pStr1 = tmpstr.Get();
                    if ( doBase64 )
                        x &= ~3;
                    else {
                        if ( pStr1[x-2] == '=' )
                            x -= 2;
                        else
                        if ( pStr1[x-1] == '=' )
                    x -= 1;
                    }
                    outS.Add( tmpstr.Get(), x );
                    strcpy( tmpstr.Get(), (const char *)(pStr1+x) );
                    tmpstr.SetLength();
                    outS.Add( "?=\r\n " );
                    outS.Add(fixupString);
                    headerLen = 1;
                }
                outS.Add(tmpstr);
                outS.Add("?=");
                headerLen += tmpstr.Length() + 2;
#if BLAT_LITE
#else
                eightBitMimeSupported = savedEightBitMimeSupported;
                binaryMimeSupported   = savedBinaryMimeSupported;
#endif
            } else {
                // quoting not necessary
                addStringToHeaderNoQuoting(pStr4, outS, headerLen, linewrap );
            }

            pStr3[1] = c;
        }
        else {
            /* No email address format I know, so do normal processing on this short string. */
            *pStr3 = '>';
            c = pStr3[1];
            pStr3[1] = 0;
            tempstring.SetLength();

            fixup(tempstring.Get(), &tmpstr, headerLen, linewrap );
            outS.Add(tmpstr.Get());
            pStr3[1] = c;
        }

        *pStr3 = '>';
        pStr3++;
        fixupString.Clear();
        if ( *pStr3 == ',' ) {
            outS.Add(",");
            pStr3++;
            headerLen++;
        }
        for ( ; *pStr3 == ' '; ) {
            outS.Add(" ");
            pStr3++;
            headerLen++;
        }
        strcpy(tempstring.Get(), pStr3);
        tempstring.SetLength();
        if ( !tempstring.Length() )
            break;

        if ( linewrap && ((headerLen + tempstring.Length() ) >= 75) ) {
            outS.Add("\r\n ");
            headerLen = 1;
        }
    }

    if ( !pStr2 || !pStr3 ) {
        fixup(tempstring.Get(), &tmpstr, headerLen, linewrap );
        outS.Add(tmpstr.Get());
    }

    if ( outString == NULL ) {
        strcpy(string, outS.Get()); // Copy back to source string (calling program must ensure buffer is big enough)
    }
    else {
        outString->Clear();
        if ( outS.Length() )
            outString->Add(outS);
    }

    return;
}


void build_headers( BLDHDRS & bldHdrs )
{
    int                   i;
    int                   yEnc_This;
    int                   hours, minutes;
    char                  boundary2[25];
    char                  tmpstr[0x2000];   // [0x200];
    Buf                   tempstring;
    SYSTEMTIME            curtime;
    TIME_ZONE_INFORMATION tzinfo;
    DWORD                 retval;
    unsigned long         cpuTime;
    Buf                   fixedFromId;
    Buf                   fixedSenderId;
    Buf                   fixedLoginName;
    Buf                   fixedSenderName;
    Buf                   fixedReplyToId;
#if BLAT_LITE
#else
    Buf                   fixedOrganization;
#endif
    Buf                   fixedReturnPathId;
    Buf                   contentType;
    FILETIME              today;
    char FAR *            domainPtr;
    char *                pp;


#if SUPPORT_YENC
    yEnc_This = yEnc;
    if ( bldHdrs.buildSMTP && !eightBitMimeSupported && !binaryMimeSupported )
#endif
        yEnc_This = FALSE;

    needBoundary = FALSE;

    if ( alternateText.Length() ) {
#if BLAT_LITE
        alternateText.Clear();
#else
        if ( uuencode )
            base64 = TRUE;
        else
            if ( !base64 )
                mime = TRUE;

        uuencode  = FALSE;
        yEnc_This = FALSE;
  #if SUPPORT_YENC
        yEnc      = FALSE;
  #endif
#endif
    }

    if ( charset[0] ) {
        if ( strcmp(charset, defaultCharset) ) {
            mime      = TRUE;   // If -charset option was used, then set mime so the charset can be identified in the headers.
            yEnc_This = FALSE;
#if BLAT_LITE
#else
            uuencode  = FALSE;
#endif
#if SUPPORT_YENC
            yEnc      = FALSE;
#endif
        }
    } else
        strcpy(charset, defaultCharset);

    fixupEmailHeaders(fromid,       &fixedFromId      , 6 , TRUE); // "From: "
    fixupEmailHeaders(senderid,     &fixedSenderId    , 6 , TRUE); // "From: "
    fixupEmailHeaders(loginname,    &fixedLoginName   , 10, TRUE); // "Sender: " or "Reply-to: "
    fixupEmailHeaders(sendername,   &fixedSenderName  , 8 , TRUE); // "Sender: "
    fixupEmailHeaders(replytoid,    &fixedReplyToId   , 10, TRUE); // "Reply-to: "
#if BLAT_LITE
#else
    fixup(organization, &fixedOrganization, 14, TRUE); // "Organization: "
#endif

    if ( returnpathid[0] ) {
        sprintf( tmpstr, "<%s>", returnpathid );
        for ( ; ; ) {
            pp = strchr( &tmpstr[1], '<' );
            if ( !pp )
                break;

            strcpy(tmpstr, pp);
        }
        *(strchr(tmpstr,'>')+1) = 0;
        fixup(tmpstr, &fixedReturnPathId, 13, TRUE);    // "Return-Path: "
    }

    // create a header for the message

#if SUPPORT_MULTIPART
    if ( bldHdrs.part < 2 ) {
#endif
        // Generate a unique message boundary identifier.

        for ( i = 0 ; i < 21 ; i++ ) {
#if SUPPORT_MULTIPART
            bldHdrs.multipartID[i] = abclist[rand()%62];
            if ( !bldHdrs.attachNbr )
#endif
                bldHdrs.attachment_boundary[i] = abclist[rand()%62];
        }

        strcpy( &bldHdrs.attachment_boundary[21], "\r\n" );

#if SUPPORT_MULTIPART
        bldHdrs.multipartID[21] = '@';
        if ( bldHdrs.wanted_hostname && *bldHdrs.wanted_hostname )
            strcpy( &bldHdrs.multipartID[22], bldHdrs.wanted_hostname );
        else
            strcpy( &bldHdrs.multipartID[22], my_hostname );
#endif

        GetLocalTime( &curtime );
        retval  = GetTimeZoneInformation( &tzinfo );
        hours   = (int) tzinfo.Bias / 60;
        minutes = (int) tzinfo.Bias % 60;
        if ( retval == TIME_ZONE_ID_STANDARD ) {
            hours   += (int) tzinfo.StandardBias / 60;
            minutes += (int) tzinfo.StandardBias % 60;
        } else {
            hours   += (int) tzinfo.DaylightBias / 60;
            minutes += (int) tzinfo.DaylightBias % 60;
        }

        // rfc1036 & rfc822 acceptable format
        // Mon, 29 Jun 1994 02:15:23 GMT
        sprintf (tmpstr, "Date: %s, %.2d %s %.4d %.2d:%.2d:%.2d %+03d%02d\r\n",
                 days[curtime.wDayOfWeek],
                 curtime.wDay,
                 months[curtime.wMonth - 1],
                 curtime.wYear,
                 curtime.wHour,
                 curtime.wMinute,
                 curtime.wSecond,
                 -hours,
                 -minutes);
        bldHdrs.header->Add( tmpstr );

        // RFC 822 From: definition in Blat changed 2000-02-03 Axel Skough SCB-SE
        sprintf( tmpstr, "From: %s\r\n",
                 fromid[0] ? fixedFromId.Get() : fixedSenderId.Get() );
        bldHdrs.header->Add( tmpstr );

        // now add the Received: from x.x.x.x by y.y.y.y with HTTP;
        if ( bldHdrs.lpszFirstReceivedData->Length() ) {
            sprintf(tmpstr,"%s%s, %.2d %s %.2d %.2d:%.2d:%.2d %+03d%02d\r\n",
                    bldHdrs.lpszFirstReceivedData->Get(),
                    days[curtime.wDayOfWeek],
                    curtime.wDay,
                    months[curtime.wMonth - 1],
                    curtime.wYear,
                    curtime.wHour,
                    curtime.wMinute,
                    curtime.wSecond,
                    -hours, -minutes);
            bldHdrs.header->Add( tmpstr);
        }

        if ( impersonating ) {
            sprintf( tmpstr, "Sender: %s\r\n", fixedLoginName.Get() );
            bldHdrs.header->Add( tmpstr );
            if ( replytoid[0] ) {
                sprintf( tmpstr, "Reply-To: %s\r\n", fixedReplyToId.Get() );
                bldHdrs.header->Add( tmpstr );
            } else
            if ( formattedContent ) {
                sprintf( tmpstr, "Reply-to: %s\r\n", fixedLoginName.Get() );
                bldHdrs.header->Add( tmpstr );
            }
        } else {
            // RFC 822 Sender: definition in Blat changed 2000-02-03 Axel Skough SCB-SE
            if ( sendername[0] ) {
                sprintf( tmpstr, "Sender: %s\r\n", fixedSenderName.Get() );
                bldHdrs.header->Add( tmpstr );
            }
            // RFC 822 Reply-To: definition in Blat changed 2000-02-03 Axel Skough SCB-SE
            if ( replytoid[0] ) {
                sprintf( tmpstr, "Reply-To: %s\r\n", fixedReplyToId.Get() );
                bldHdrs.header->Add( tmpstr );
            }
        }

        if ( bldHdrs.buildSMTP ) {
            if ( destination.Length() ) {
#if SUPPORT_SALUTATIONS
                find_and_strip_salutation( destination );
#endif
                fixupEmailHeaders(destination.Get(), &tempstring, 4, TRUE);
                bldHdrs.header->Add( "To: " );
                bldHdrs.header->Add( tempstring );
                bldHdrs.header->Add( "\r\n" );
                tempstring.Clear();
            }
            else if ( !cc_list.Length() ) {
                if ( sendUndisclosed )
                    bldHdrs.header->Add( "To: Undisclosed recipients:;\r\n" );
            }

            if ( cc_list.Length() ) {
                // Add line for the Carbon Copies
                fixupEmailHeaders(cc_list.Get(), &tempstring, 4, TRUE);
                bldHdrs.header->Add( "Cc: " );
                bldHdrs.header->Add( tempstring );
                bldHdrs.header->Add( "\r\n" );
                tempstring.Clear();
            }

            if ( bldHdrs.addBccHeader && bcc_list.Length() ) {
                // Add line for the Blind Carbon Copies, for transmitting through POP3
                fixupEmailHeaders(bcc_list.Get(), &tempstring, 5, TRUE);
                bldHdrs.header->Add( "Bcc: " );
                bldHdrs.header->Add( tempstring );
                bldHdrs.header->Add( "\r\n" );
                tempstring.Clear();
            }

            // To use loginname for the RFC 822 Disposition and Return-receipt fields doesn't seem to be unambiguous.
            // Either separate definitions should be used for these fileds to get full flexibility or - as a compromise -
            // the content of the Reply-To. field would rather be used when specified. 2000-02-03 Axel Skough SCB-SE

            if ( disposition ) {
                sprintf( tmpstr, "Disposition-Notification-To: %s\r\n",
                         replytoid[0] ? fixedReplyToId.Get() : loginname );
                bldHdrs.header->Add( tmpstr );
            }

            if ( returnreceipt ) {
                sprintf( tmpstr, "Return-Receipt-To: %s\r\n",
                         replytoid[0] ? fixedReplyToId.Get() : loginname );
                bldHdrs.header->Add( tmpstr );
            }

            // Toby Korn tkorn@snl.com 8/4/1999
            // If priority is specified on the command line, add it to the header
            // The latter two options are X.400, mainly for Lotus Notes (blah)
            if ( priority [0] == '0') {
                bldHdrs.header->Add( "X-MSMail-Priority: Low\r\nX-Priority: 5\r\nPriority: normal\r\nImportance: normal\r\n" );
                bldHdrs.header->Add( "X-MimeOLE: Produced by Blat v" );
                bldHdrs.header->Add( blatVersion );
                bldHdrs.header->Add( "\r\n" );
            }
            else if (priority [0] == '1') {
                bldHdrs.header->Add( "X-MSMail-Priority: High\r\nX-Priority: 1\r\nPriority: urgent\r\nImportance: high\r\n" );
                bldHdrs.header->Add( "X-MimeOLE: Produced by Blat v" );
                bldHdrs.header->Add( blatVersion );
                bldHdrs.header->Add( "\r\n" );
            }
            // If sensitivity is specified on the command line, add it to the header
            // These are X.400
            if ( sensitivity [0] == '0') {
                bldHdrs.header->Add( "Sensitivity: Personal\r\n" );
            }
            else if (sensitivity [0] == '1') {
                bldHdrs.header->Add( "Sensitivity: Private\r\n" );
            }
            else if (sensitivity [0] == '2') {
                bldHdrs.header->Add( "Sensitivity: Company-Confidential\r\n" );
            }
#if INCLUDE_NNTP
        } else {
            if ( groups.Length() && NNTPHost[0] ) {
                fixup(groups.Get(), &tempstring, 12, TRUE);
                bldHdrs.header->Add( "Newsgroups: " );
                bldHdrs.header->Add( tempstring );
                bldHdrs.header->Add( "\r\n" );
                tempstring.Clear();
            }
#endif
        }

#if BLAT_LITE
#else
        if ( organization[0] ) {
            sprintf( tmpstr, "Organization: %s\r\n", fixedOrganization.Get() );
            bldHdrs.header->Add( tmpstr );
        }
#endif

        // RFC 822 Return-Path: definition in Blat entered 2000-02-03 Axel Skough SCB-SE
        if ( returnpathid[0] ) {
            sprintf( tmpstr, "Return-Path: %s\r\n", fixedReturnPathId.Get() );
            bldHdrs.header->Add( tmpstr );
        }

#if BLAT_LITE
        if ( includeUserAgent ) {
            sprintf( tmpstr, "User-Agent: Blat /%s%s (a " WIN_32_STR " SMTP mailer) (http://www.blat.net)\r\n", blatVersion, blatVersionSuf );
        } else {
            sprintf( tmpstr, "X-Mailer: Blat v%s%s, a " WIN_32_STR " SMTP mailer (http://www.blat.net)\r\n", blatVersion, blatVersionSuf );
        }
        bldHdrs.header->Add( tmpstr );
#else
        if ( xheaders[0] ) {
            bldHdrs.header->Add( xheaders );
            bldHdrs.header->Add( "\r\n" );
        }

        if ( aheaders1[0] ) {
            bldHdrs.header->Add( aheaders1 );
            bldHdrs.header->Add( "\r\n" );
        }

        if ( aheaders2[0] ) {
            bldHdrs.header->Add( aheaders2 );
            bldHdrs.header->Add( "\r\n" );
        }

        if ( noheader < 2 ) {
            if ( includeUserAgent ) {
                sprintf( tmpstr, "User-Agent: Blat /%s%s (a " WIN_32_STR " SMTP/NNTP mailer)", blatVersion, blatVersionSuf );
                bldHdrs.header->Add( tmpstr );
                if ( noheader == 0 )
                    bldHdrs.header->Add( " (http://www.blat.net)" );
            } else {
                sprintf( tmpstr, "X-Mailer: Blat v%s%s, a " WIN_32_STR " SMTP/NNTP mailer", blatVersion, blatVersionSuf );
                bldHdrs.header->Add( tmpstr );
                if ( noheader == 0 )
                    bldHdrs.header->Add( " http://www.blat.net" );
            }
            bldHdrs.header->Add( "\r\n" );
        }
#endif

#if SUPPORT_MULTIPART
        bldHdrs.multipartHdrs->Add( bldHdrs.header->Get() );
        bldHdrs.varHeaders->Clear();
        bldHdrs.varHeaders->Add( bldHdrs.header->Get() );
    }
    else
    {
        bldHdrs.header->Clear();
        bldHdrs.header->Add( bldHdrs.varHeaders->Get() );
    }
#endif

    domainPtr = gensock_getdomainfromhostname(bldHdrs.server_name);
    if (!domainPtr)
        domainPtr = bldHdrs.server_name;

    GetSystemTimeAsFileTime( &today );
#if defined(_WIN64)
	cpuTime = 0;
#else
    __asm {
        pushad
        rdtsc
        mov     cpuTime, eax
        popad
    }
#endif
    sprintf(tmpstr, "Message-ID: <%08lx$Blat.v%s$%08lx$%lx%lx@%s>\r\n",
                    today.dwHighDateTime, blatVersion, today.dwLowDateTime, GetCurrentProcessId(), cpuTime, domainPtr );
    bldHdrs.header->Add( tmpstr );

    if ( subject[0] ) {
        Buf fixedSubject;
        Buf newSubject;

        newSubject = subject;
        bldHdrs.header->Add( "Subject: " );

#if SUPPORT_MULTIPART
        Buf mpSubject;

        mpSubject  = subject;
        bldHdrs.multipartHdrs->Add( "Subject: " );
#endif
        if ( bldHdrs.attachName && *bldHdrs.attachName ) {
#if SUPPORT_MULTIPART
            if ( bldHdrs.nbrOfAttachments > 1 ) {
                int sizeFactor = 0;
                int x = bldHdrs.nbrOfAttachments;
                for ( ; x; ) {
                    x /= 10;
                sizeFactor++;
                }
                sprintf( tmpstr, " %0*u of %u", sizeFactor, bldHdrs.attachNbr+1, bldHdrs.nbrOfAttachments );
                newSubject.Add( tmpstr );
                mpSubject.Add(  tmpstr );
            }
#endif
            newSubject.Add( " \"" );
            newSubject.Add( GetNameWithoutPath(bldHdrs.attachName) );
            newSubject.Add( '\"' );

#if SUPPORT_YENC
            if ( yEnc_This && attach ) {
  #if SUPPORT_MULTIPART
                if ( bldHdrs.totalparts > 1 ) {
                    mpSubject.Add(  " yEnc" );
                    newSubject.Add( " yEnc" );
                } else {
                    sprintf( tmpstr, " %u yEnc bytes", bldHdrs.attachSize );
                    newSubject.Add( tmpstr );
                }
  #else
                sprintf( tmpstr, " %u yEnc bytes", bldHdrs.attachSize );
                newSubject.Add( tmpstr );
  #endif
            }
#endif
#if SUPPORT_MULTIPART
            if ( bldHdrs.totalparts > 1 ) {
                int sizeFactor = 0;
                int x = bldHdrs.totalparts;
                for ( ; x; ) {
                    x /= 10;
                    sizeFactor++;
                }
                sprintf( tmpstr, " [%0*u/%u]", sizeFactor, bldHdrs.part, bldHdrs.totalparts );
                newSubject.Add( tmpstr );
            }
#endif
        }

#if SUPPORT_MULTIPART
  #if INCLUDE_NNTP
        if ( groups.Length() && NNTPHost[0] ) {
            fixup(mpSubject.Get(), &fixedSubject, 9, FALSE);
            bldHdrs.multipartHdrs->Add( fixedSubject );
            bldHdrs.multipartHdrs->Add( "\r\n" );

            fixup(newSubject.Get(), &fixedSubject, 9, FALSE);
            bldHdrs.header->Add( fixedSubject );
            bldHdrs.header->Add( "\r\n" );
        }
        else
  #endif
        {
            fixup(mpSubject.Get(), &fixedSubject, 9, TRUE);
            bldHdrs.multipartHdrs->Add( fixedSubject );
            bldHdrs.multipartHdrs->Add( "\r\n" );

            fixup(newSubject.Get(), &fixedSubject, 9, TRUE);
            bldHdrs.header->Add( fixedSubject );
            bldHdrs.header->Add( "\r\n" );
        }
#else
        fixup(newSubject.Get(), &fixedSubject, 9, TRUE);
        bldHdrs.header->Add( fixedSubject );
        bldHdrs.header->Add( "\r\n" );
#endif
    } else {
        if ( !ssubject ) {          //$$ASD
            if ( lstrcmp(bodyFilename, "-") == 0 ) {
                bldHdrs.header->Add(        "Subject: Contents of console input\r\n" );
#if SUPPORT_MULTIPART
                bldHdrs.multipartHdrs->Add( "Subject: Contents of console input\r\n" );
#endif
            } else {
                sprintf( tmpstr, "Subject: Contents of file: %s\r\n", GetNameWithoutPath(bodyFilename) );
                bldHdrs.header->Add( tmpstr );
#if SUPPORT_MULTIPART
                bldHdrs.multipartHdrs->Add( tmpstr );
#endif
            }
        }
    }

    memcpy(boundary2, bldHdrs.attachment_boundary, 21 );
    strcpy( &boundary2[21], "\"\r\n" );

    // This is either mime, base64, uuencoded, or neither.  With or without attachments.  Whew!
    if ( mime ) {
        // Indicate MIME version and type

        if ( !bldHdrs.attachNbr || (bldHdrs.totalparts > 1) || alternateText.Length() ) {
            contentType.Add( "MIME-Version: 1.0\r\n" );
            if ( attach || alternateText.Length() ) {
                contentType.Add( "Content-Type:" );
                if ( haveAttachments ) {
                    contentType.Add( " multipart/mixed;\r\n" );
                } else
                    if ( haveEmbedded ) {
                        contentType.Add( " multipart/related;\r\n" );
                        if ( alternateText.Length() )
                            contentType.Add( " type=\"multipart/alternative\";\r\n" );
                    } else
                        contentType.Add( " multipart/alternative;\r\n" );

                contentType.Add( " boundary=\"" BOUNDARY_MARKER );
                contentType.Add( boundary2 );
                if ( !bldHdrs.attachNbr )
                    contentType.Add( "\r\nThis is a multi-part message in MIME format.\r\n" );
            } else {
#if SMART_CONTENT_TYPE
                char foundType  [0x200];

                strcpy( foundType, "text/" );
                strcat( foundType, textmode );
#endif
                contentType.Add( "Content-Transfer-Encoding: quoted-printable\r\n" );
#if SMART_CONTENT_TYPE
                if ( !ConsoleDone && !strcmp( textmode, "plain") )
                    getContentType( tmpstr, foundType, foundType, getShortFileName( bodyFilename ) );

                sprintf( tmpstr, "Content-Type: %s; charset=%s\r\n", foundType, charset );
#else
                sprintf( tmpstr, "Content-Type: text/%s; charset=%s\r\n", textmode, charset );    // modified 15. June 1999 by JAG
#endif
                contentType.Add( tmpstr );
            }
        }
    } else {
#if BLAT_LITE
#else
        if ( base64 ) {
            // Indicate MIME version and type
            contentType.Add( "MIME-Version: 1.0\r\n" );
            contentType.Add( "Content-Type:" );
            if ( haveAttachments ) {
                contentType.Add( " multipart/mixed;\r\n" );
            } else
                if ( haveEmbedded ) {
                    contentType.Add( " multipart/related;\r\n" );
                    // the next is required per RFC 2387
                    if ( alternateText.Length() )
                        contentType.Add( " type=\"multipart/alternative\";\r\n" );
                } else
                    if ( alternateText.Length() )
                        contentType.Add( " multipart/alternative;\r\n" );
                    else
                        contentType.Add( " multipart/mixed;\r\n" );

            contentType.Add( " boundary=\"" BOUNDARY_MARKER );
            contentType.Add( boundary2 );
            contentType.Add( "\r\nThis is a multi-part message in MIME format.\r\n" );
        } else
#endif
        {
            if ( attach ) {
#if BLAT_LITE
                    contentType.Add( "MIME-Version: 1.0\r\n" );
                    contentType.Add( "Content-Type:" );
                    contentType.Add( " multipart/mixed;\r\n" );
                    contentType.Add( " boundary=\"" BOUNDARY_MARKER );
                    contentType.Add( boundary2);
                    contentType.Add( "\r\nThis is a multi-part message in MIME format.\r\n" );
#else
                if ( uuencode || yEnc_This || !bldHdrs.buildSMTP ) {
 /*
  * Having this code enabled causes Mozilla to ignore attachments and treat them as inline text.
  *
  #if SMART_CONTENT_TYPE
                    char foundType  [0x200];

                    strcpy( foundType, "text/" );
                    strcat( foundType, textmode );
  #endif
                    sprintf( tmpstr, "Content-description: %s message body\r\n",
                             bldHdrs.buildSMTP ? "Mail" : "News" );
                    contentType.Add( tmpstr );
  #if SMART_CONTENT_TYPE
                    if ( !ConsoleDone && !strcmp( textmode, "plain") )
                        getContentType( tmpstr, foundType, foundType, getShortFileName( bodyFilename ) );

                    sprintf(tmpstr, "Content-Type: %s; charset=%s\r\n", foundType, charset );
  #else
                    sprintf(tmpstr, "Content-Type: text/%s; charset=%s\r\n", textmode, charset );
  #endif
                    contentType.Add( tmpstr );
  */
                } else {
                    contentType.Add( "MIME-Version: 1.0\r\n" );
                    contentType.Add( "Content-Type:" );
                    contentType.Add( " multipart/mixed;\r\n" );
                    contentType.Add( " boundary=\"" BOUNDARY_MARKER );
                    contentType.Add( boundary2);
  #if SUPPORT_MULTIPART
                    if ( !bldHdrs.attachNbr )
  #endif
                        contentType.Add( "\r\nThis is a multi-part message in MIME format.\r\n" );
                }
#endif
            } else {
#if SMART_CONTENT_TYPE
                char foundType  [0x200];

                strcpy( foundType, "text/" );
                strcat( foundType, textmode );
#endif
#if BLAT_LITE
#else
                if ( (binaryMimeSupported || eightBitMimeSupported) && (eightBitMimeRequested || yEnc_This) )
                    contentType.Add( "Content-Transfer-Encoding: 8BIT\r\n" );
                else
                    if ( CheckIfNeedQuotedPrintable((unsigned char *)TempConsole.Get(), FALSE) )
                        contentType.Add( "Content-Transfer-Encoding: 8BIT\r\n" );
                    else
#endif
                        contentType.Add( "Content-Transfer-Encoding: 7BIT\r\n" );

#if SMART_CONTENT_TYPE
                if ( !ConsoleDone && !strcmp( textmode, "plain") )
                    getContentType( tmpstr, foundType, foundType, getShortFileName( bodyFilename ) );

                sprintf(tmpstr, "Content-Type: %s; charset=%s\r\n", foundType, charset );
#else
                sprintf(tmpstr, "Content-Type: text/%s; charset=%s\r\n", textmode, charset );
#endif
                contentType.Add( tmpstr );
            }
        }
    }

#if SUPPORT_MULTIPART
    if ( bldHdrs.totalparts > 1 ) {
        bldHdrs.header->Add( "MIME-Version: 1.0\r\n" );
        bldHdrs.header->Add( "Content-Type:" );
  #if 01
        bldHdrs.header->Add( " message/partial;\r\n" );
        sprintf( tmpstr, "    id=\"%s\";\r\n" \
                         "    number=%u; total=%u;\r\n" \
                         "    boundary=\"" BOUNDARY_MARKER "%s",  // Include a boundary= incase it is not included above.
                 bldHdrs.multipartID, bldHdrs.part, bldHdrs.totalparts, boundary2 );
  #else
        bldHdrs.header->Add( " multipart/mixed;\r\n" );
        sprintf( tmpstr, " number=%u; total=%u; id=\"%s\"\r\n",
                 bldHdrs.part, bldHdrs.totalparts, bldHdrs.multipartID );
  #endif
        bldHdrs.header->Add( tmpstr );
        bldHdrs.multipartHdrs->Add( contentType );
    } else {
        bldHdrs.multipartHdrs->Clear();
        bldHdrs.header->Add( contentType );
    }
#else
    bldHdrs.header->Add( contentType );
#endif

    if ( bldHdrs.lpszOtherHeader->Length() )
        bldHdrs.header->Add( bldHdrs.lpszOtherHeader->Get() );

    if ( formattedContent )
        bldHdrs.header->Add( "\r\n" );

    bldHdrs.messageBuffer->Clear();
    bldHdrs.messageBuffer->Add( bldHdrs.header->Get() );

    if ( strstr(bldHdrs.messageBuffer->Get(), BOUNDARY_MARKER ) )
        needBoundary = TRUE;
}
