/*
    blatcgi.hpp
 */
#ifndef __BLATCGI_HPP__
#define __BLATCGI_HPP__ 1

// CGI Support by Gilles Vollant <info@winimage.com>, February 1999
// Added in Blat Version 1.8.3

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"
#include "common_data.h"

extern BOOL getFileSize(LPCTSTR lpFn, DWORD &dwSize);
extern void GetEnv(LPTSTR lpszEnvVar, Buf &buf);
extern void ReadPostData(Buf &buf);
extern DWORD SearchNextPos(LPCTSTR lpszParamCgi, BOOL fSearchEqual);
extern DWORD SearchNextPercent(LPCTSTR lpszParamCgi);
extern void SearchVar(Buf &lpszParamCgi,LPCTSTR lpszVarName,BOOL fMimeDecode, Buf &ret);
extern BOOL BuildMessageAndCmdLine(Buf &lpszParamCgi, LPCTSTR lpszPathTranslated, Buf &lpszCmdBlat, Buf &lpszMessage);
extern DWORD WINAPI ReadCommandLine(LPTSTR szParcLine, int & argc, LPTSTR* &argv);
extern BOOL DoCgiWork(int & argc, LPTSTR*  &argv, Buf &lpszMessage,
                      Buf & lpszCgiSuccessUrl, Buf &lpszCgiFailureUrl,
                      Buf & lpszFirstReceivedData, Buf &lpszOtherHeader);

#endif  // #ifndef __BLATCGI_HPP__
