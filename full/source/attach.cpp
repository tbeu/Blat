/*
    attach.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "blat.h"
#include "winfile.h"


#if BLAT_LITE
#else
extern void   douuencode(Buf & source, Buf & out, LPTSTR filename, int part, int lastpart);
#endif
extern void   base64_encode(Buf & source, Buf & out, int inclCrLf, int inclPad);
extern void   printMsg(LPTSTR p, ... );                     // Added 23 Aug 2000 Craig Morrison
extern void   fixup(LPTSTR string, Buf * tempstring2, int headerLen, int linewrap);
extern LPTSTR getShortFileName (LPTSTR fileName);
extern void   getContentType(Buf & pDestBuffer, LPTSTR foundType, LPTSTR defaultType, LPTSTR sFileName);
extern void   incrementBoundary( _TCHAR boundary[] );
extern void   decrementBoundary( _TCHAR boundary[] );
extern void   convertUnicode( Buf &sourceText, int * utf, LPTSTR charset, int utfRequested );
#if defined(_UNICODE) || defined(UNICODE)
void checkInputForUnicode ( Buf & stringToCheck );
#endif

#if SUPPORT_YENC
extern void   yEncode( Buf & source, Buf & out, LPTSTR filename, long fulllen,
                       int part, int lastpart, unsigned long &full_crc_val );
#endif
#if INCLUDE_NNTP
extern Buf    groups;
#endif

extern _TCHAR textmode[TEXTMODE_SIZE+1];        // added 15 June 1999 by James Greene "greene@gucc.org"
extern _TCHAR formattedContent;
extern int    attach;
extern _TCHAR attachfile[64][MAX_PATH+1];
extern _TCHAR charset[40];
extern LPTSTR defaultCharset;
extern _TCHAR attachtype[64];
extern _TCHAR boundaryPosted;
extern _TCHAR needBoundary;
extern int    attachFoundFault;
extern _TCHAR mime;
extern int    utf;

#if BLAT_LITE
#else
extern _TCHAR uuencode;
extern _TCHAR base64;
extern _TCHAR yEnc;
extern _TCHAR eightBitMimeSupported;
extern _TCHAR eightBitMimeRequested;

extern unsigned long maxMessageSize;
extern unsigned int  uuencodeBytesLine;

extern Buf     userContentType;
#endif


WIN32_FIND_DATA FindFileData;

typedef struct NODES {
    NODES * next;
    LPTSTR  attachmentName;
    int     fileType;
    DWORD   fileSize;
} _NODES;

static NODES * attachList;

static void releaseNode ( NODES * &nextNode )
{
    if ( nextNode ) {
        releaseNode( nextNode->next );
        free( nextNode->attachmentName );
        free( nextNode );
        nextNode = NULL;
    }
}


void getAttachmentInfo( int attachNbr, LPTSTR & attachName, DWORD & attachSize, int & attachType )
{
    NODES * tmpNode;

    tmpNode = attachList;
    for ( ; attachNbr && tmpNode->next; attachNbr-- )
        tmpNode = tmpNode->next;

    attachName = tmpNode->attachmentName;
    attachSize = tmpNode->fileSize;
    attachType = tmpNode->fileType;
}


void releaseAttachmentInfo ( void )
{
    releaseNode( attachList );
}


int collectAttachmentInfo ( DWORD & totalsize, size_t msgBodySize )
{
    int      i;
    size_t   x;
    HANDLE   ihandle;
    int      filewasfound;
    _TCHAR   path[MAX_PATH+1];
    LPTSTR   pathptr;
    int      nbrOfFilesFound;
    WinFile  fileh;
    NODES *  tmpNode;
    _TCHAR   attachedfile[MAX_PATH+1];
    int      errorFound;

    attachList = NULL;
    tmpNode    = NULL;

    totalsize = (DWORD)msgBodySize;
    nbrOfFilesFound = 0;
    errorFound = FALSE;
    attachFoundFault = FALSE;

    // Process any attachments
    for ( i = 0; (i < attach) && !errorFound; i++ ) {

        // Get the path for opening file
        _tcscpy(path, attachfile[i]);
        pathptr = _tcsrchr(path,__T('\\'));
        if ( !pathptr )
            pathptr = _tcsrchr(path,__T(':'));

        if ( pathptr ) {
            *(pathptr+1) = __T('\0');
        } else {
            path[0] = __T('\0');
        }

        ihandle = FindFirstFile((LPTSTR)attachfile[i], &FindFileData);
        filewasfound = (ihandle != INVALID_HANDLE_VALUE);
        if ( !filewasfound ) {
            attachFoundFault = TRUE;
        }

        while ( filewasfound && !errorFound ) {
            if ( !(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ) {
                _tcscpy(attachedfile, path);
                _tcscat(attachedfile,FindFileData.cFileName);
                if ( fileh.OpenThisFile(attachedfile) ) {
                    // if ( fileh.IsDiskFile() && FindFileData.nFileSizeLow && !FindFileData.nFileSizeHigh ) {
                    if ( fileh.IsDiskFile() && !FindFileData.nFileSizeHigh ) {
                        DWORD tmpSize = totalsize;

                        totalsize += FindFileData.nFileSizeLow;
                        if ( totalsize < tmpSize ) {// If the size exceeded 4G
                            totalsize  = 0;
                            errorFound = TRUE;
                        } else {
                            if ( attachList == NULL ) {
                                tmpNode = attachList = (NODES *) malloc( sizeof(NODES) );
                            } else {
                                tmpNode->next = (NODES *) malloc( sizeof(NODES) );
                                tmpNode = tmpNode->next;
                            }

                            tmpNode->next = NULL;

                            x = (_tcslen(attachedfile) + 1) * sizeof(_TCHAR);
                            tmpNode->attachmentName = (LPTSTR) malloc( x );
                            memcpy( tmpNode->attachmentName, attachedfile, x );

                            tmpNode->fileType = attachtype[i];
                            tmpNode->fileSize = FindFileData.nFileSizeLow;

                            nbrOfFilesFound++;
                        }
                    }
                    fileh.Close();
                }
            }
            filewasfound = FindNextFile(ihandle, &FindFileData);
        }
        FindClose( ihandle );
    }

    return nbrOfFilesFound;
}


void getMaxMsgSize ( int buildSMTP, DWORD &length )
{
#if BLAT_LITE
    buildSMTP = buildSMTP;

    length = (DWORD)(-1);
#else
  #if !SUPPORT_YENC
    buildSMTP = buildSMTP;
  #else
    int     yEnc_This;

    yEnc_This = yEnc;
    if ( buildSMTP && !eightBitMimeSupported )
        yEnc_This = FALSE;

    if ( !yEnc_This )
  #endif
    {
        if ( uuencode ) {
            if ( length % uuencodeBytesLine )
                length -= (length % uuencodeBytesLine);
        } else {
            // base64 encoding for this attachment.
            if ( length % 54 )
                length -= (length % 54);
        }
    }
#endif
}


int add_one_attachment ( Buf &messageBuffer, int buildSMTP, LPTSTR attachment_boundary,
                         DWORD startOffset, DWORD &length,
                         int part, int totalparts, int attachNbr, int * prevAttachType )
{
    int           yEnc_This;
    Buf           tmpstr2;
    LPTSTR        p;
    DWORD         dummy;
    Buf           localHdr;
    DWORD         attachSize, fullFileSize;
    int           attachType;
    LPTSTR        attachName;
    LPTSTR        shortname;
    Buf           fileBuffer;
    unsigned long full_crc_val;
    WinFile       fileh;
    Buf           shortNameBuf;
    Buf           textFileBuffer;
    _TCHAR        localCharset[40];

#if BLAT_LITE
    buildSMTP  = buildSMTP; // remove compiler warnings.
    totalparts = totalparts;
    part       = part;
#endif

#if SUPPORT_YENC
    yEnc_This = yEnc;
    if ( buildSMTP && !eightBitMimeSupported )
#endif
        yEnc_This = FALSE;

    if ( memcmp( charset, __T("utf-"), 4*sizeof(_TCHAR) ) == 0 )
        localCharset[0] = __T('\0');
    else
        _tcscpy( localCharset, charset );

    textFileBuffer.Free();
    tmpstr2.Clear();
    getAttachmentInfo( attachNbr, attachName, fullFileSize, attachType );
    shortNameBuf.Clear();
    shortname = getShortFileName(attachName);
    shortNameBuf = shortname;

#if defined(_UNICODE) || defined(UNICODE)
    {
 #if BLAT_LITE
 #else
        _TCHAR savedEightBitMimeRequested;
 #endif
        _TCHAR savedMime;
        int    savedUTF;

 #if BLAT_LITE
 #else
        savedEightBitMimeRequested = eightBitMimeRequested;
 #endif
        savedUTF  = utf;
        savedMime = mime;

        utf = 0;
        checkInputForUnicode( shortNameBuf );
        utf = savedUTF;
        mime = savedMime;
 #if BLAT_LITE
 #else
        eightBitMimeRequested = savedEightBitMimeRequested;
 #endif
    }
#endif

    if ( (attachType == TEXT_ATTACHMENT) || (attachType == INLINE_ATTACHMENT) ) {
        int    utfRequested;
        int    tempUTF;

        //get the text of the file into a string buffer
        if ( !fileh.OpenThisFile(attachName) ) {
            printMsg(__T("error reading %s, aborting\n"),attachName);
            tmpstr2.Free();
            localHdr.Free();
            fileBuffer.Free();
            shortNameBuf.Free();
            return(3);
        }
        attachSize = fullFileSize;
        fileBuffer.Alloc( attachSize + 1 );
        fileBuffer.SetLength( attachSize );

        if ( !fileh.ReadThisFile(fileBuffer.Get(), attachSize, &dummy, NULL) ) {
            printMsg(__T("error reading %s, aborting\n"), attachName);
            fileh.Close();
            tmpstr2.Free();
            localHdr.Free();
            fileBuffer.Free();
            shortNameBuf.Free();
            return(5);
        }
        fileh.Close();
        tempUTF = 0;
  #if BLAT_LITE
  #else
        if ( eightBitMimeSupported )
            utfRequested = 8;
        else
  #endif
            utfRequested = 7;
        convertUnicode( fileBuffer, &tempUTF, localCharset, utfRequested );
        textFileBuffer = fileBuffer;
        fullFileSize = (DWORD)textFileBuffer.Length();
        startOffset = 0;
        if ( fullFileSize > length )
            length = fullFileSize;

        fileBuffer.Free();
    }

    // Process the attachment
    if ( startOffset == 0 ) {
        // Do the header bit...
        p = messageBuffer.GetTail() - 3;
        if ( p[2] != __T('\n') ) {
            messageBuffer.Add( __T("\r\n") );
            localHdr = __T("\r\n");
        } else {
            if ( p[0] != __T('\n') )
                localHdr = __T("\r\n");
        }

        full_crc_val = (unsigned long)(-1L);

        if ( attachType == EMBED_ATTACHMENT )
            printMsg(__T("Embedded binary file: %s\n"),attachName);
        else
        if ( attachType == BINARY_ATTACHMENT )
            printMsg(__T("Attached binary file: %s\n"),attachName);
        else
            printMsg(__T("Attached text file: %s\n"),attachName);

#if BLAT_LITE
#else
        if ( uuencode || yEnc_This || (!buildSMTP && !base64) ) {
            if ( needBoundary && !boundaryPosted ) {
                localHdr.Add( __T("--") BOUNDARY_MARKER );
                localHdr.Add( attachment_boundary );
                boundaryPosted = TRUE;
            }
        } else
#endif
        {
            Buf tmpstr3;
            Buf tmpstr1;

            fixup( shortNameBuf.Get(), &tmpstr3, 22, TRUE );

            tmpstr1.Alloc( _MAX_PATH*5 );
            tmpstr1.Clear();
            if ( attachType == EMBED_ATTACHMENT ) {

                // 9/18/1998 by Toby Korn
                // Replaced default Content-Type with a lookup based on file extension
#if BLAT_LITE
                getContentType (tmpstr1, NULL, NULL                 , shortname);
#else
                getContentType (tmpstr1, NULL, userContentType.Get(), shortname);
#endif
                tmpstr2 = __T("Content-ID: <");
                tmpstr2.Add( tmpstr3 );
                tmpstr2.Add( __T(">\r\n") );
                tmpstr2.Add( __T("Content-Transfer-Encoding: BASE64\r\n") );
                tmpstr2.Add( __T("Content-Disposition: INLINE\r\n") );
            } else
            if ( attachType == BINARY_ATTACHMENT ) {

                // 9/18/1998 by Toby Korn
                // Replaced default Content-Type with a lookup based on file extension
                tmpstr2    = __T("Content-Transfer-Encoding: BASE64\r\n");
                getContentType (tmpstr1, NULL, NULL, shortname);
                tmpstr2.Add( __T("Content-Disposition: ATTACHMENT;\r\n") );
                tmpstr2.Add( __T(" filename=\"") );
                tmpstr2.Add( tmpstr3 );
                tmpstr2.Add( __T("\"\r\n") );
            } else {
                _TCHAR bitSize;
                Buf    disposition;
                LPTSTR pString;

                if ( attachType == TEXT_ATTACHMENT ) {
                    disposition = __T("ATTACHMENT;\r\n");
                    disposition.Add( __T(" filename=\"") );
                    disposition.Add( tmpstr3 );
                    disposition.Add( __T("\"") );
                }
                else
                /* if ( attachType == INLINE_ATTACHMENT ) */ {
                    disposition = __T("INLINE");
                }

                tmpstr1 = __T("Content-Type: text/");
                tmpstr1.Add( textmode );
                tmpstr1.Add( __T(";\r\n") );
                tmpstr1.Add( __T(" charset=\"") );
                if ( localCharset[0] )
                    tmpstr1.Add( localCharset );
                else
                    tmpstr1.Add( defaultCharset );    // modified 15. June 1999 by JAG
                tmpstr1.Add( __T("\";\r\n") );
                tmpstr1.Add( __T(" name=\"") );
                tmpstr1.Add( tmpstr3 );
                tmpstr1.Add( __T("\";\r\n") );
                tmpstr1.Add( __T(" reply-type=original\r\n") );

                bitSize = __T('7');
                pString = textFileBuffer.Get();
                if ( pString ) {
                    if ( *pString == 0xFEFF )
                        pString++;

                    for ( ; ; ) {
                        if ( *pString == __T('\0') )
                            break;
                        if ( *pString > 0x007F ) {
                            bitSize = __T('8');
                            break;
                        }
                        pString++;
                    }
                }
                tmpstr2    = __T("Content-Transfer-Encoding: ");
                tmpstr2.Add( bitSize );
                tmpstr2.Add( __T("BIT\r\n") );
                tmpstr2.Add( __T("Content-Disposition: ") );
                tmpstr2.Add( disposition );
                tmpstr2.Add( __T("\r\n") );
                tmpstr2.Add( __T("Content-Description: \"") );
                tmpstr2.Add( tmpstr3 );
                tmpstr2.Add( __T("\"\r\n") );
                disposition.Free();
            }

            if ( *prevAttachType == EMBED_ATTACHMENT ) {
                if ( attachType != EMBED_ATTACHMENT ) {
                    localHdr.Add( __T("--") BOUNDARY_MARKER );
                    localHdr.Add( attachment_boundary, 21 );
                    localHdr.Add( __T("--\r\n\r\n") );
                    decrementBoundary( attachment_boundary );
                }
            }
            localHdr.Add( __T("--") BOUNDARY_MARKER );
            localHdr.Add( attachment_boundary );
            localHdr.Add( tmpstr1 );
            localHdr.Add( tmpstr2 );
            boundaryPosted = TRUE;
            tmpstr1.Free();
            tmpstr2.Free();
        }

        *prevAttachType = attachType;
        if ( formattedContent )
            if ( (localHdr.Length() > 2) && (*(localHdr.GetTail()-3) != __T('\n')) )
                localHdr.Add( __T("\r\n") );

        // put the localHdr at the end of existing message...
        messageBuffer.Add(localHdr);
    }

    if ( (attachType == BINARY_ATTACHMENT) || (attachType == EMBED_ATTACHMENT) ) {
        //get the text of the file into a string buffer
        if ( !fileh.OpenThisFile(attachName) ) {
            printMsg(__T("error reading %s, aborting\n"),attachName);
            textFileBuffer.Free();
            tmpstr2.Free();
            localHdr.Free();
            fileBuffer.Free();
            shortNameBuf.Free();
            return(3);
        }

#if SUPPORT_MULTIPART
        if ( startOffset ) {
            if ( !fileh.SetPosition( (LONG)startOffset, 0, FILE_BEGIN ) ) {
                printMsg(__T("error reading %s, aborting\n"), attachName);
                fileh.Close();
                textFileBuffer.Free();
                tmpstr2.Free();
                localHdr.Free();
                fileBuffer.Free();
                shortNameBuf.Free();
                return(5);
            }
        }
#endif
        attachSize = fullFileSize - startOffset;
        if ( attachSize > length )
            attachSize = length;

        fileBuffer.Alloc( attachSize + 1 );
        fileBuffer.SetLength( attachSize );

        if ( !fileh.ReadThisFile(fileBuffer.Get(), attachSize, &dummy, NULL) ) {
            printMsg(__T("error reading %s, aborting\n"), attachName);
            fileh.Close();
            textFileBuffer.Free();
            tmpstr2.Free();
            localHdr.Free();
            fileBuffer.Free();
            shortNameBuf.Free();
            return(5);
        }
        fileh.Close();

#if SUPPORT_YENC
        if ( yEnc_This ) {
            if ( totalparts == 1 )
                yEncode( fileBuffer, messageBuffer, shortname, (long)fullFileSize, 0, 0, full_crc_val );
            else
                yEncode( fileBuffer, messageBuffer, shortname, (long)fullFileSize, part, totalparts, full_crc_val );
        } else
#endif
        {
#if BLAT_LITE
#else
            if ( uuencode || (!buildSMTP && !base64) ) {
                douuencode( fileBuffer, messageBuffer, shortname, part, totalparts );
            } else
#endif
            {
                base64_encode( fileBuffer, messageBuffer, TRUE, TRUE );
            }
        }
    } else {
        attachSize = (DWORD)textFileBuffer.Length();
        if ( attachSize ) {
            fileBuffer = textFileBuffer;

            p = fileBuffer.Get();
            if ( p ) {
                for ( ; attachSize; attachSize-- ) {
                    if ( *p == 0x1A )
                        break;

                    if ( *p )
                        messageBuffer.Add( *p );

                    p++;
                }
            }
            length = (DWORD)(p - fileBuffer.Get());
        }
    }

    textFileBuffer.Free();
    tmpstr2.Free();
    localHdr.Free();
    fileBuffer.Free();
    shortNameBuf.Free();
    return(0);
}


int add_attachments ( Buf &messageBuffer, int buildSMTP, LPTSTR attachment_boundary, int nbrOfAttachments )
{
    int   retval;
    DWORD length;
    int   attachNbr;
    int   prevAttachType;

    prevAttachType = -1;
    for ( attachNbr = 0; attachNbr < nbrOfAttachments; attachNbr++ ) {
        length = (DWORD)-1;
        retval = add_one_attachment( messageBuffer, buildSMTP, attachment_boundary,
                                     0, length, 1, 1, attachNbr, &prevAttachType );
        if ( retval )
            return retval;
    }

    return(0);
}
