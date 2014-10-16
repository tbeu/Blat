/*
    attach.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blat.h"
#include "winfile.h"


#if BLAT_LITE
#else
extern void   douuencode(Buf & source, Buf & out, const char * filename, int part, int lastpart);
#endif
extern void   base64_encode(Buf & source, Buf & out, int inclCrLf, int inclPad);
extern void   printMsg(const char *p, ... );              // Added 23 Aug 2000 Craig Morrison
extern void   fixup(char * string, Buf * tempstring2, int headerLen, int linewrap);
extern char * getShortFileName (char * fileName);
extern void   getContentType(char *sDestBuffer, char *foundType, char *defaultType, char *sFileName);
extern void   incrementBoundary( char boundary[] );
extern void   decrementBoundary( char boundary[] );

#if SUPPORT_YENC
extern void   yEncode( Buf & source, Buf & out, char * filename, long fulllen,
                       int part, int lastpart, unsigned long &full_crc_val );
#endif
#if INCLUDE_NNTP
extern Buf     groups;
#endif

extern char    textmode[TEXTMODE_SIZE+1];      // added 15 June 1999 by James Greene "greene@gucc.org"
extern char    formattedContent;
extern int     attach;
extern char    attachfile[64][MAX_PATH+1];
extern char    charset[40];
extern char    attachtype[64];
extern char    boundaryPosted;
extern char    needBoundary;

#if BLAT_LITE
#else
extern char    uuencode;
extern char    base64;
extern char    yEnc;
extern char    eightBitMimeSupported;

extern unsigned long maxMessageSize;
extern unsigned int  uuencodeBytesLine;

extern Buf     userContentType;
#endif


WIN32_FIND_DATA FindFileData;

typedef struct NODES {
    NODES * next;
    char  * attachmentName;
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


void getAttachmentInfo( int attachNbr, char * & attachName, DWORD & attachSize, int & attachType )
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


int collectAttachmentInfo ( DWORD & totalsize, int msgBodySize )
{
    int     i, x;
    HANDLE  ihandle;
    int     filewasfound;
    char    path[MAX_PATH+1];
    char  * pathptr;
    int     nbrOfFilesFound;
    WinFile fileh;
    NODES * tmpNode;
    char    attachedfile[MAX_PATH+1];
    int     errorFound;

    attachList = NULL;
    tmpNode    = NULL;

    totalsize = (DWORD)msgBodySize;
    nbrOfFilesFound = 0;
    errorFound = FALSE;

    // Process any attachments
    for ( i = 0; (i < attach) && !errorFound; i++ ) {

        // Get the path for opening file
        strcpy(path, attachfile[i]);
        pathptr = strrchr(path,'\\');
        if ( !pathptr )
            pathptr = strrchr(path,':');

        if ( pathptr ) {
            *(pathptr+1) = 0;
        } else {
            path[0] = '\0';
        }

        ihandle = FindFirstFile(attachfile[i], &FindFileData);
        filewasfound = (ihandle != INVALID_HANDLE_VALUE);

        while ( filewasfound && !errorFound ) {
            if ( !(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ) {
                strcpy(attachedfile, path);
                strcat(attachedfile,FindFileData.cFileName);
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

                            x = (int)strlen(attachedfile) + 1;
                            tmpNode->attachmentName = (char *) malloc( x );
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


int add_one_attachment ( Buf &messageBuffer, int buildSMTP, char * attachment_boundary,
                         DWORD startOffset, DWORD &length,
                         int part, int totalparts, int attachNbr, int * prevAttachType )
{
    int           yEnc_This;
    char          tmpstr1[0x200];
    char          tmpstr2[0x200];
    char        * p;
    DWORD         dummy;
    Buf           localHdr;
    DWORD         attachSize, fullFileSize;
    int           attachType;
    char        * attachName;
    char        * shortname;
    Buf           fileBuffer;
    unsigned long full_crc_val;
    WinFile       fileh;

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

    getAttachmentInfo( attachNbr, attachName, fullFileSize, attachType );
    shortname = getShortFileName(attachName);

    // Process the attachment
    if ( startOffset == 0 ) {
        // Do the header bit...
        p = messageBuffer.GetTail() - 3;
        if ( p[2] != '\n' ) {
            messageBuffer.Add( "\r\n" );
            localHdr = "\r\n";
        } else {
            if ( p[0] != '\n' )
                localHdr = "\r\n";
        }

        full_crc_val = (unsigned long)(-1L);

        if ( attachType == EMBED_ATTACHMENT )
            printMsg("Embedded binary file: %s\n",attachName);
        else
        if ( attachType == BINARY_ATTACHMENT )
            printMsg("Attached binary file: %s\n",attachName);
        else
            printMsg("Attached text file: %s\n",attachName);

#if BLAT_LITE
#else
        if ( uuencode || yEnc_This || (!buildSMTP && !base64) ) {
            if ( needBoundary && !boundaryPosted ) {
                localHdr.Add( "--" BOUNDARY_MARKER );
                localHdr.Add( attachment_boundary );
                boundaryPosted = TRUE;
            }
        } else
#endif
        {
            Buf tmpstr3;

            fixup( shortname, &tmpstr3, 22, TRUE );

            if ( attachType == EMBED_ATTACHMENT ) {

                // 9/18/1998 by Toby Korn
                // Replaced default Content-Type with a lookup based on file extension
#if BLAT_LITE
                getContentType (tmpstr1, NULL, NULL                 , shortname);
#else
                getContentType (tmpstr1, NULL, userContentType.Get(), shortname);
#endif
                sprintf( tmpstr2, "Content-ID: <%s>\r\n", tmpstr3.Get() );
                strcat(  tmpstr2, "Content-Disposition: INLINE\r\n" );
                strcat(  tmpstr2, "Content-Transfer-Encoding: BASE64\r\n" );
            } else
            if ( attachType == BINARY_ATTACHMENT ) {

                // 9/18/1998 by Toby Korn
                // Replaced default Content-Type with a lookup based on file extension
                getContentType (tmpstr1, NULL, NULL, shortname);
                sprintf( tmpstr2, "Content-Disposition: ATTACHMENT;\r\n" \
                                  " filename=\"%s\"\r\n", tmpstr3.Get() );
                strcat(  tmpstr2, "Content-Transfer-Encoding: BASE64\r\n" );
            } else
            if ( attachType == TEXT_ATTACHMENT ) {
                sprintf( tmpstr1, "Content-Type: text/%s; charset=%s\r\n",
                                  textmode, ( charset[0] ) ? charset : "US-ASCII" );    // modified 15. June 1999 by JAG
                sprintf( tmpstr2, "Content-Disposition: ATTACHMENT;\r\n" \
                                  " filename=\"%s\"\r\n" \
                                  "Content-Description: \"%s\"\r\n", tmpstr3.Get(), tmpstr3.Get() );
            } else
            /* if ( attachType == INLINE_ATTACHMENT ) */ {
                sprintf( tmpstr1, "Content-Type: text/%s; charset=%s\r\n",
                                  textmode, ( charset[0] ) ? charset : "US-ASCII" );    // modified 15. June 1999 by JAG
                strcat(  tmpstr1, "Content-Disposition: INLINE\r\n" );
                sprintf( tmpstr2, "Content-Description: \"%s\"\r\n", tmpstr3.Get() );
            }

            if ( *prevAttachType == EMBED_ATTACHMENT )
                if ( attachType != EMBED_ATTACHMENT ) {
                    localHdr.Add( "--" BOUNDARY_MARKER );
                    localHdr.Add( attachment_boundary, 21 );
                    localHdr.Add( "--\r\n\r\n" );
                    decrementBoundary( attachment_boundary );
                }
            localHdr.Add( "--" BOUNDARY_MARKER );
            localHdr.Add( attachment_boundary );
            localHdr.Add( tmpstr1 );
            localHdr.Add( tmpstr2 );
            boundaryPosted = TRUE;
        }

        *prevAttachType = attachType;
        if ( formattedContent )
            if ( (localHdr.Length() > 2) && (*(localHdr.GetTail()-3) != '\n') )
                localHdr.Add( "\r\n" );

        // put the localHdr at the end of existing message...
        messageBuffer.Add(localHdr);
    }

    //get the text of the file into a string buffer
    if ( !fileh.OpenThisFile(attachName) ) {
        printMsg("error reading %s, aborting\n",attachName);
        return(3);
    }

#if SUPPORT_MULTIPART
    if ( startOffset ) {
        if ( !fileh.SetPosition( (LONG)startOffset, 0, FILE_BEGIN ) ) {
            printMsg("error reading %s, aborting\n", attachName);
            fileh.Close();
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
        printMsg("error reading %s, aborting\n", attachName);
        fileh.Close();
        return(5);
    }
    fileh.Close();

    if ( (attachType == BINARY_ATTACHMENT) || (attachType == EMBED_ATTACHMENT) ) {
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
        p = fileBuffer.Get();
        for ( ; attachSize; attachSize-- ) {
            if ( *p == 0x1A )
                break;

            if ( *p )
                messageBuffer.Add( *p );

            p++;
        }
        length = (int)(p - fileBuffer.Get());
    }

    return(0);
}


int add_attachments ( Buf &messageBuffer, int buildSMTP, char * attachment_boundary, int nbrOfAttachments )
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
