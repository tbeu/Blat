#ifndef _WINFILE_H
#define _WINFILE_H

#ifndef true
    #define true                (1==1)
    #define false               (1==0)
#endif
#ifndef TRUE
    #define TRUE                true
    #define FALSE               false
#endif

#ifndef bool
    #define bool                BOOL
#endif

/*
    WinFile class
*/
#define CREATEFILE_FILENAME_ONLY    1

class WinFile {
private:
    HANDLE hFile;
public:
    WinFile() {
        hFile = INVALID_HANDLE_VALUE;
        }
    ~WinFile() {
        Close();
    }
    void Close() {
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
            }
        }
#if CREATEFILE_FILENAME_ONLY
    bool OpenThisFile( LPCTSTR lpFileName )
        {
        Close();
        hFile = ::CreateFile(
            lpFileName,                             // file name
            FILE_READ_DATA,                         // access mode
            FILE_SHARE_READ|FILE_SHARE_WRITE,       // share mode
            NULL,                                   // Security Attributes pointer
            OPEN_EXISTING,                          // how to create
            FILE_FLAG_SEQUENTIAL_SCAN,              // file attributes
            NULL                                    // handle to template file
            );
        if ( hFile == INVALID_HANDLE_VALUE )
            hFile = ::CreateFile(
                lpFileName,                         // file name
                FILE_READ_DATA,                     // access mode
                0,                                  // share mode
                NULL,                               // Security Attributes pointer
                OPEN_EXISTING,                      // how to create
                FILE_FLAG_SEQUENTIAL_SCAN,          // file attributes
                NULL                                // handle to template file
                );
        if ( hFile == INVALID_HANDLE_VALUE )
            hFile = ::CreateFile(
                lpFileName,                         // file name
                GENERIC_READ,                       // access mode
                FILE_SHARE_READ|FILE_SHARE_WRITE,   // share mode
                NULL,                               // Security Attributes pointer
                OPEN_EXISTING,                      // how to create
                FILE_FLAG_SEQUENTIAL_SCAN,          // file attributes
                NULL                                // handle to template file
                );
        if ( hFile == INVALID_HANDLE_VALUE )
            hFile = ::CreateFile(
                lpFileName,                         // file name
                GENERIC_READ,                       // access mode
                0,                                  // share mode
                NULL,                               // Security Attributes pointer
                OPEN_EXISTING,                      // how to create
                FILE_FLAG_SEQUENTIAL_SCAN,          // file attributes
                NULL                                // handle to template file
                );
#else
    bool OpenThisFile(
        LPCTSTR lpFileName,                         // file name
        DWORD dwDesiredAccess,                      // access mode
        DWORD dwShareMode,                          // share mode
        LPSECURITY_ATTRIBUTES lpSecurityAttributes, // SD
        DWORD dwCreationDisposition,                // how to create
        DWORD dwFlagsAndAttributes,                 // file attributes
        HANDLE hTemplateFile                        // handle to template file
        ) {
        Close();
        hFile = ::CreateFile(
            lpFileName,                             // file name
            dwDesiredAccess,                        // access mode
            dwShareMode,                            // share mode
            lpSecurityAttributes,                   // SD
            dwCreationDisposition,                  // how to create
            dwFlagsAndAttributes,                   // file attributes
            hTemplateFile                           // handle to template file
            );
        if ( hFile == INVALID_HANDLE_VALUE )
            hFile = ::CreateFile(
                lpFileName,                         // file name
                dwDesiredAccess,                    // access mode
                0,                                  // share mode (no sharing)
                lpSecurityAttributes,               // SD
                dwCreationDisposition,              // how to create
                dwFlagsAndAttributes,               // file attributes
                hTemplateFile                       // handle to template file
                );
#endif
        return (hFile != INVALID_HANDLE_VALUE);
        }
    bool IsDiskFile() {
        return GetFileType(hFile) == FILE_TYPE_DISK;
        }
    DWORD GetSize() {
        return GetFileSize(hFile, NULL);
        }
    bool ReadThisFile(
        LPTSTR lpBuffer,
        DWORD nNumberOfBytesToRead,
        LPDWORD lpNumberOfBytesRead,
        LPOVERLAPPED lpOverlapped
        ) {
        if (hFile != INVALID_HANDLE_VALUE) {
            if ( ::ReadFile(hFile,
                            (LPVOID)lpBuffer,
                            nNumberOfBytesToRead,
                            lpNumberOfBytesRead,
                            lpOverlapped) ) {
#if defined(_UNICODE) || defined(UNICODE)
                unsigned char * pBuffer = (unsigned char *)lpBuffer;
                DWORD x;
                x = *lpNumberOfBytesRead;
                while ( x ) {
                    --x;
                    lpBuffer[x] = pBuffer[x];
                    }
#endif
                return true;
                }
            }
        return false;
        }
#if SUPPORT_MULTIPART

    bool SetPosition(
        LONG lDistanceToMove,  // number of bytes to move file pointer
        PLONG lpDistanceToMoveHigh,
                               // pointer to high-order DWORD of
                               // distance to move
        DWORD dwMoveMethod     // how to move
        ) {
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD retValue =
            ::SetFilePointer(
                hFile,
                lDistanceToMove,
                lpDistanceToMoveHigh,
                dwMoveMethod);

  #ifdef INVALID_SET_FILE_POINTER
            if ( retValue != INVALID_SET_FILE_POINTER )
                return true;
  #else
            retValue = retValue;
  #endif
            return ( ::GetLastError() == NO_ERROR );
            }
        return false;
        }
#endif
    };

#endif
