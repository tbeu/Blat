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
#include "common_data.h"
#include "blatext.hpp"
#include "macros.h"
#include "attach.hpp"
#include "uuencode.hpp"
#include "base64.hpp"
#include "filetype.hpp"
#include "bldhdrs.hpp"
#include "unicode.hpp"
#include "yenc.hpp"


static void releaseNode ( NODES * &nextNode )
{
    if ( nextNode ) {
        releaseNode( nextNode->next );
        free( nextNode->attachmentName );
        if ( nextNode->description )
            free( nextNode->description );

        free( nextNode );
        nextNode = NULL;
    }
}


void getAttachmentInfo( COMMON_DATA & CommonData, int attachNbr, LPTSTR & attachName, DWORD & attachSize, int & attachType, LPTSTR & attachDescription )
{
    FUNCTION_ENTRY();
    NODES * tmpNode;

    tmpNode = CommonData.attachList;
    for ( ; attachNbr && tmpNode->next; attachNbr-- )
        tmpNode = tmpNode->next;

    attachName        = tmpNode->attachmentName;
    attachSize        = tmpNode->fileSize;
    attachType        = tmpNode->fileType;
    attachDescription = tmpNode->description;
    FUNCTION_EXIT();
}


void releaseAttachmentInfo ( COMMON_DATA & CommonData )
{
    FUNCTION_ENTRY();
    releaseNode( CommonData.attachList );
    FUNCTION_EXIT();
}


int collectAttachmentInfo ( COMMON_DATA & CommonData, DWORD & totalsize, size_t msgBodySize )
{
    FUNCTION_ENTRY();
    int      i;
    size_t   x;
    HANDLE   ihandle;
    int      filewasfound;
    Buf      path;
    LPTSTR   pathptr;
    int      nbrOfFilesFound;
    WinFile  fileh;
    NODES *  tmpNode;
    Buf      attachedfile;
    int      errorFound;
    WIN32_FIND_DATA FindFileData;


    CommonData.attachList = NULL;
    tmpNode = NULL;

    totalsize = (DWORD)msgBodySize;
    nbrOfFilesFound = 0;
    errorFound = FALSE;
    CommonData.attachFoundFault = FALSE;

    // Process any attachments
    for ( i = 0; (i < CommonData.attach) && !errorFound; i++ ) {

        Buf description;
        Buf fileToSearchFor;

        description.Clear();

        // Get the path for opening file
        path = CommonData.attachfile[i];

        pathptr = _tcsrchr(path.Get(), __T('<'));
        if ( pathptr ) {
            *pathptr = __T('\0');
            description.Add( &pathptr[1] );
            pathptr = _tcsrchr(description.Get(), __T('>'));
            if ( pathptr )
                *pathptr = __T('\0');

            path.SetLength();
            description.SetLength();
        }

        fileToSearchFor = path;
        pathptr = _tcsrchr(path.Get(), __T('\\'));
        if ( !pathptr )
            pathptr = _tcsrchr(path.Get(), __T(':'));

        if ( pathptr ) {
            *(pathptr+1) = __T('\0');
        } else {
            path.Get()[0] = __T('\0');
        }
        path.SetLength();
        ihandle = FindFirstFile(fileToSearchFor.Get(), &FindFileData);
        filewasfound = (ihandle != INVALID_HANDLE_VALUE);
        if ( !filewasfound ) {
            printMsg(CommonData, __T("No files found matching search string \"%s\".\n"), fileToSearchFor.Get());
            if ( !CommonData.ignoreMissingAttachmentFiles )
                CommonData.attachFoundFault = TRUE;
        }

        while ( filewasfound && !errorFound ) {
            if ( !(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ) {
                attachedfile = path;
                attachedfile.Add(FindFileData.cFileName);
                if ( fileh.OpenThisFile(attachedfile.Get()) ) {
                    // if ( fileh.IsDiskFile() && FindFileData.nFileSizeLow && !FindFileData.nFileSizeHigh ) {
                    if ( fileh.IsDiskFile() ) {
                        if ( FindFileData.nFileSizeHigh ) {
                            printMsg(CommonData, __T("Found \"%s\" matching search string \"%s\", but it is too large (>= 4GB), therefore will not be attached or sent.\n"), attachedfile.Get(), fileToSearchFor.Get());
                        }
                        else {
                            DWORD tmpSize = totalsize;

                            totalsize += FindFileData.nFileSizeLow;
                            if ( totalsize < tmpSize ) {// If the size exceeded 4G
                                totalsize  = 0;
                                errorFound = TRUE;
                            } else {
                                if ( CommonData.attachList == NULL ) {
                                    tmpNode = CommonData.attachList = (NODES *) malloc( sizeof(NODES) );
                                } else {
                                    tmpNode->next = (NODES *) malloc( sizeof(NODES) );
                                    tmpNode = tmpNode->next;
                                }

                                tmpNode->next = NULL;

                                x = (attachedfile.Length() + 1) * sizeof(_TCHAR);
                                tmpNode->attachmentName = (LPTSTR) malloc( x );
                                memcpy( tmpNode->attachmentName, attachedfile.Get(), x );

                                if ( description.Length() ) {
                                    x = (description.Length() + 1) * sizeof(_TCHAR);
                                    tmpNode->description = (LPTSTR) malloc( x );
                                    memcpy( tmpNode->description, description.Get(), x );
                                }
                                else
                                    tmpNode->description = NULL;

                                tmpNode->fileType = CommonData.attachtype[i];
                                tmpNode->fileSize = FindFileData.nFileSizeLow;

                                nbrOfFilesFound++;
                            }
                        }
                    }
                    else {
                        printMsg(CommonData, __T("Found \"%s\" matching search string \"%s\", but it appears to not be a file that can be opened.\n"), attachedfile.Get(), fileToSearchFor.Get());
                    }
                    fileh.Close();
                }
                else {
                    printMsg(CommonData, __T("Found \"%s\" matching search string \"%s\", but the file cannot be opened.\n"), attachedfile.Get(), fileToSearchFor.Get());
                }
                attachedfile.Free();
            }
            filewasfound = FindNextFile(ihandle, &FindFileData);
        }
        FindClose( ihandle );
        fileToSearchFor.Free();
        description.Free();
        path.Free();
    }

    FUNCTION_EXIT();
    return nbrOfFilesFound;
}


void getMaxMsgSize ( COMMON_DATA & CommonData, int buildSMTP, DWORD &length )
{
    FUNCTION_ENTRY();
#if BLAT_LITE
    CommonData = CommonData;
    buildSMTP  = buildSMTP;

    length = (DWORD)(-1);
#else
  #if !SUPPORT_YENC
    buildSMTP = buildSMTP;
  #else
    int     yEnc_This;

    yEnc_This = CommonData.yEnc;
    if ( buildSMTP && !CommonData.eightBitMimeSupported )
        yEnc_This = FALSE;

    if ( !yEnc_This )
  #endif
    {
        if ( CommonData.uuencode ) {
            if ( length % CommonData.uuencodeBytesLine )
                length -= (length % CommonData.uuencodeBytesLine);
        } else {
            // base64 encoding for this attachment.
            if ( length % 54 )
                length -= (length % 54);
        }
    }
#endif
    FUNCTION_EXIT();
}


static _TCHAR getBitSize( LPTSTR pString )
{
    _TCHAR bitSize;

    bitSize = __T('7');
    if ( pString ) {

#if defined(_UNICODE) || defined(UNICODE)
        if ( *pString == 0xFEFF )
            pString++;
#endif
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
    return bitSize;
}

int add_one_attachment ( COMMON_DATA & CommonData, Buf &messageBuffer, int buildSMTP, LPTSTR attachment_boundary,
                         DWORD startOffset, DWORD &length,
                         int part, int totalparts, int attachNbr, int * prevAttachType )
{
    FUNCTION_ENTRY();
    int           yEnc_This;
    Buf           tmpstr1;
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
    LPTSTR        attachDescription;

#if BLAT_LITE
    buildSMTP  = buildSMTP; // remove compiler warnings.
    totalparts = totalparts;
    part       = part;
#endif

#if SUPPORT_YENC
    yEnc_This = CommonData.yEnc;
    if ( buildSMTP && !CommonData.eightBitMimeSupported )
#endif
        yEnc_This = FALSE;

    if ( memcmp( CommonData.charset.Get(), __T("utf-"), 4*sizeof(_TCHAR) ) == 0 )
        localCharset[0] = __T('\0');
    else
        _tcscpy( localCharset, CommonData.charset.Get() );

    textFileBuffer.Free();
    tmpstr2.Clear();
    getAttachmentInfo( CommonData, attachNbr, attachName, fullFileSize, attachType, attachDescription );
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
        savedEightBitMimeRequested = CommonData.eightBitMimeRequested;
  #endif
        savedUTF  = CommonData.utf;
        savedMime = CommonData.mime;

        CommonData.utf = 0;
        checkInputForUnicode( CommonData, shortNameBuf );
        CommonData.utf = savedUTF;
        CommonData.mime = savedMime;
  #if BLAT_LITE
  #else
        CommonData.eightBitMimeRequested = savedEightBitMimeRequested;
  #endif
    }
#endif

    tmpstr1.Alloc( _MAX_PATH*5 );
    tmpstr1.Clear();
    // 9/18/1998 by Toby Korn
    // Replaced default Content-Type with a lookup based on file extension
#if BLAT_LITE
    getContentType (CommonData, tmpstr1, NULL, NULL                            , shortname);
#else
    getContentType (CommonData, tmpstr1, NULL, CommonData.userContentType.Get(), shortname);
#endif
    if ( _memicmp( tmpstr1.Get(), __T("Content-Type: message/rfc822"), 28*sizeof(_TCHAR) ) == 0 )
        attachType |= 0x80;
    //else
    //if ( _memicmp( tmpstr1.Get(), __T("Content-Type: text/"), 19*sizeof(_TCHAR) ) == 0 )
    //    attachType |= 0x40;

    if ( ((attachType & 0x07) == TEXT_ATTACHMENT  ) ||
         ((attachType & 0x07) == INLINE_ATTACHMENT) ||
         (attachType == BINARY_MESSAGE_ATTACHMENT ) ) {
        int    utfRequested;
        int    tempUTF;

        //get the text of the file into a string buffer
        if ( !fileh.OpenThisFile(attachName) ) {
            printMsg(CommonData, __T("error opening %s, aborting\n"), attachName);
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
            printMsg(CommonData, __T("error reading %s, aborting\n"), attachName);
            fileh.Close();
            tmpstr2.Free();
            localHdr.Free();
            fileBuffer.Free();
            shortNameBuf.Free();
            return(5);
        }
        *fileBuffer.GetTail() = __T('\0');
        fileh.Close();
        tempUTF = 0;
#if BLAT_LITE
#else
        if ( CommonData.eightBitMimeSupported )
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
            printMsg(CommonData, __T("Embedded binary file: %s\n"), attachName);
        else
        if ( attachType == EMBED_MESSAGE_ATTACHMENT )
            printMsg(CommonData, __T("Embedded binary file: %s\n"), attachName);
        else
        if ( attachType == BINARY_ATTACHMENT )
            printMsg(CommonData, __T("Attached binary file: %s\n"), attachName);
        else
            printMsg(CommonData, __T("Attached text file: %s\n"), attachName);

#if BLAT_LITE
#else
        if ( CommonData.uuencode || yEnc_This || (!buildSMTP && !CommonData.base64) ) {
            if ( CommonData.needBoundary && !CommonData.boundaryPosted ) {
                localHdr.Add( __T("--") BOUNDARY_MARKER );
                localHdr.Add( attachment_boundary );
                CommonData.boundaryPosted = TRUE;
            }
        } else
#endif
        {
            Buf tmpstr3;

            switch( attachType ) {
                case EMBED_ATTACHMENT:
                case EMBED_MESSAGE_ATTACHMENT:
                    tmpstr2 = __T("Content-ID: <");
                    fixup(CommonData, shortNameBuf.Get(), &tmpstr3, 13, TRUE );
                    tmpstr2.Add( tmpstr3 );
                    tmpstr2.Add( __T(">\r\n") );
                    tmpstr2.Add( __T("Content-Transfer-Encoding: BASE64\r\n") );
                    tmpstr2.Add( __T("Content-Disposition: INLINE\r\n") );
                    break;

                case BINARY_ATTACHMENT:
                    tmpstr2    = __T("Content-Transfer-Encoding: BASE64\r\n");
                    tmpstr2.Add( __T("Content-Disposition: ATTACHMENT;\r\n") );
                    fixup(CommonData, shortNameBuf.Get(), &tmpstr3, 11, TRUE );
                    tmpstr2.Add( __T(" filename=\"") );
                    tmpstr2.Add( tmpstr3 );
                    tmpstr2.Add( __T("\"\r\n") );
                    break;

                case BINARY_MESSAGE_ATTACHMENT:
                case TEXT_ATTACHMENT:
                case TEXT_MESSAGE_ATTACHMENT:
                    if ( attachType == TEXT_ATTACHMENT ) {
                        tmpstr1 = __T("Content-Type: text/");
                        tmpstr1.Add( CommonData.textmode );
                        tmpstr1.Add( __T(";\r\n") );
                        fixup(CommonData, shortNameBuf.Get(), &tmpstr3, 7, TRUE );
                        tmpstr1.Add( __T(" name=\"") );
                        tmpstr1.Add( tmpstr3 );
                        tmpstr1.Add( __T("\"") );
                    }
                    else {
                        tmpstr1.Remove();
                        tmpstr1.Remove();
                    }
                    tmpstr1.Add( __T(";\r\n") );
                    tmpstr1.Add( __T(" charset=\"") );
                    if ( localCharset[0] )
                        tmpstr1.Add( localCharset );
                    else
                        tmpstr1.Add( defaultCharset );    // modified 15. June 1999 by JAG
                    tmpstr1.Add( __T("\"\r\n") );

                    tmpstr2    = __T("Content-Transfer-Encoding: ");
                    tmpstr2.Add( getBitSize( textFileBuffer.Get() ) );
                    tmpstr2.Add( __T("BIT\r\n") );
                    tmpstr2.Add( __T("Content-Disposition: ATTACHMENT;\r\n") );
                    fixup(CommonData, shortNameBuf.Get(), &tmpstr3, 11, TRUE );
                    tmpstr2.Add( __T(" filename=\"") );
                    tmpstr2.Add( tmpstr3 );
                    tmpstr2.Add( __T("\"\r\n") );
                    tmpstr2.Add( __T("Content-Description: \"") );
                    fixup(CommonData, shortNameBuf.Get(), &tmpstr3, 22, TRUE );
                    tmpstr2.Add( tmpstr3 );
                    tmpstr2.Add( __T("\"\r\n") );
                    break;

                case INLINE_ATTACHMENT:
                case INLINE_MESSAGE_ATTACHMENT:
                    if ( attachType == INLINE_ATTACHMENT ) {
                        tmpstr1 = __T("Content-Type: text/");
                        tmpstr1.Add( CommonData.textmode );
                    }
                    else {
                        tmpstr1.Remove();               // remove the line feed ('\n')
                        tmpstr1.Remove();               // remove the carriage return ('\r')
                    }
                    tmpstr1.Add( __T("; charset=\"") );
                    if ( localCharset[0] )
                        tmpstr1.Add( localCharset );
                    else
                        tmpstr1.Add( defaultCharset );    // modified 15. June 1999 by JAG
                    tmpstr1.Add( __T("\"\r\n") );

                    tmpstr2    = __T("Content-Transfer-Encoding: ");
                    tmpstr2.Add( getBitSize( textFileBuffer.Get() ) );
                    tmpstr2.Add( __T("BIT\r\n") );
                    tmpstr2.Add( __T("Content-Disposition: INLINE\r\n") );
                    tmpstr2.Add( __T("Content-Description: \"") );
                    fixup(CommonData, shortNameBuf.Get(), &tmpstr3, 22, TRUE );
                    tmpstr2.Add( tmpstr3 );
                    tmpstr2.Add( __T("\"\r\n") );
                    break;
            }

            if ( tmpstr2.Length() ) {
                if ( attachDescription ) {
                    tmpstr2.Add( __T("Content-Description: ") );
                    if ( _tcschr( attachDescription, __T(' ') ) != NULL ) {
                        tmpstr1.Add( __T("\"") );
                        tmpstr2.Add( attachDescription );
                        tmpstr1.Add( __T("\"") );
                    }
                    else
                        tmpstr2.Add( attachDescription );

                    tmpstr2.Add( __T("\r\n") );
                }
            }
            if ( *prevAttachType == EMBED_ATTACHMENT ) {
                if ( (attachType & 0x07) != EMBED_ATTACHMENT ) {
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
            CommonData.boundaryPosted = TRUE;
            tmpstr1.Free();
            tmpstr2.Free();
            tmpstr3.Free();
        }

        *prevAttachType = (attachType & 0x07);
        if ( CommonData.formattedContent )
            if ( (localHdr.Length() > 2) && (*(localHdr.GetTail()-3) != __T('\n')) )
                localHdr.Add( __T("\r\n") );

        // put the localHdr at the end of existing message...
        messageBuffer.Add(localHdr);
    }

    if ( (attachType == BINARY_ATTACHMENT) || ((attachType & 0x07) == EMBED_ATTACHMENT) ) {
        //get the text of the file into a string buffer
        if ( !fileh.OpenThisFile(attachName) ) {
            printMsg(CommonData, __T("error opening %s, aborting\n"), attachName);
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
                printMsg(CommonData, __T("error accessing %s, aborting\n"), attachName);
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
            printMsg(CommonData, __T("error reading %s, aborting\n"), attachName);
            fileh.Close();
            textFileBuffer.Free();
            tmpstr2.Free();
            localHdr.Free();
            fileBuffer.Free();
            shortNameBuf.Free();
            return(5);
        }
        *fileBuffer.GetTail() = __T('\0');
        fileh.Close();

#if SUPPORT_YENC
        if ( yEnc_This ) {
            if ( totalparts == 1 )
                yEncode( CommonData, fileBuffer, messageBuffer, shortname, (long)fullFileSize, 0, 0, full_crc_val );
            else
                yEncode( CommonData, fileBuffer, messageBuffer, shortname, (long)fullFileSize, part, totalparts, full_crc_val );
        } else
#endif
        {
#if BLAT_LITE
#else
            if ( CommonData.uuencode || (!buildSMTP && !CommonData.base64) ) {
                douuencode( CommonData, fileBuffer, messageBuffer, shortname, part, totalparts );
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
    tmpstr1.Free();
    tmpstr2.Free();
    localHdr.Free();
    fileBuffer.Free();
    shortNameBuf.Free();
    FUNCTION_EXIT();
    return(0);
}


int add_attachments ( COMMON_DATA & CommonData, Buf &messageBuffer, int buildSMTP, LPTSTR attachment_boundary, int nbrOfAttachments )
{
    FUNCTION_ENTRY();
    int   retval;
    DWORD length;
    int   attachNbr;
    int   prevAttachType;

    prevAttachType = -1;
    for ( attachNbr = 0; attachNbr < nbrOfAttachments; attachNbr++ ) {
        length = (DWORD)-1;
        retval = add_one_attachment( CommonData, messageBuffer, buildSMTP, attachment_boundary,
                                     0, length, 1, 1, attachNbr, &prevAttachType );
        if ( retval )
            return retval;
    }

    FUNCTION_EXIT();
    return(0);
}
