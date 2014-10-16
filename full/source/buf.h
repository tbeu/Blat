#ifndef BUF_H
#define BUF_H

/*
    Buf class for buffer manipulations
    Harri Pesonen 2002-12-04
*/

#ifndef __STDC_WANT_SECURE_LIB__
#define __STDC_WANT_SECURE_LIB__ 0
#endif

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE 1
#endif


//#include <stdio.h>
#include <windows.h>
#include <string.h>

#define Buf_Increment 0x2000

class Buf {
private:
    char *buffer;
    unsigned buflen;
    unsigned bufsize;

    void SetStringLen(const char *p, int len) {
        buflen = len;
        bufsize = buflen + 1;
        buffer = new char[bufsize];
        memcpy(buffer, p, len);
        buffer[buflen] = 0;
        }
    void SetString(const char *p) {
        buflen = lstrlen(p);
        bufsize = buflen + 1;
        buffer = new char[bufsize];
        strcpy(buffer, p);
        }
public:
    ~Buf() {
        if (buffer)
            delete [] buffer;
        }
    Buf() {
        buffer = 0;
        buflen = 0;
        bufsize = 0;
        }
    Buf(const char *p) {
        SetString(p);
        }
    Buf(const char *p, int len) {
        SetStringLen(p, len);
        }
    Buf(unsigned alloclen) {
        buflen = 0;
        bufsize = alloclen;
        buffer = new char[bufsize];
        }
    void Clear() {
        buflen = 0;
        if (bufsize)
            buffer[0] = 0;
        }
    void Free() {
        buflen = 0;
        if (buffer) {
            delete [] buffer;
            buffer = 0;
            bufsize = 0;
            }
        }
    void Clone(const Buf &buf) {
        if (buf.bufsize) {
            Alloc(buf.buflen + 1);
            memcpy(buffer, buf.buffer, buf.buflen);
            buflen = buf.buflen;
            buffer[buflen] = 0;
            }
        else
            Free();
        }
    Buf(const Buf &buf) {
        Clear();
        Clone(buf);
        }
    Buf & operator=( const Buf &buf ) {
        Clone(buf);
        return *this;
        }
    void Move(Buf &buf) {
        Free();
        buffer = buf.buffer;
        bufsize = buf.bufsize;
        buflen = buf.buflen;
        buf.buffer = 0;
        buf.bufsize = 0;
        buf.buflen = 0;
        }
    void Alloc(unsigned size) {
        if (size > bufsize) {
            bufsize = (size + (Buf_Increment - 1)) & (-1 ^ (Buf_Increment - 1));// 0x7FFFE000;
            char *newbuffer = new char[bufsize];
            if (buffer) {
                memcpy(newbuffer, buffer, buflen + 1);
                delete [] buffer;
                }
            buffer = newbuffer;
            }
        }
    void AllocExact(unsigned size) {
        if (size > bufsize) {
            bufsize = size;
            char *newbuffer = new char[bufsize];
            if (buffer) {
                memcpy(newbuffer, buffer, buflen + 1);
                delete [] buffer;
                }
            buffer = newbuffer;
            }
        }
    void Add(const char *text, int textlen) {
        int newlen = buflen + textlen;
        Alloc(newlen + 1);
        memcpy(buffer + buflen, text, textlen);
        buffer[newlen] = 0;
        buflen = newlen;
        }
    void Add(const char *text) {
        Add(text, lstrlen(text));
        }
    void Add(char ch) {
        Add(&ch, sizeof(char));
        }
    void Add(const Buf &buf) {
        Add(buf.buffer, buf.buflen);
        }
    char *Get() {
        return buffer;
        }
    unsigned Length() const {
        return buflen;
        }
    char *GetTail() {
        return buffer + buflen;
        }
    void Expand(int size) {
        Alloc(buflen + size);
        }
    void Adjust() {
        buflen += lstrlen(buffer + buflen);
        }
    void SetLength() {
        buflen = lstrlen(buffer);
        }
    void SetLength(unsigned newlen) {
        if (newlen < bufsize)
            buflen = newlen;
        }
    void Remove() {
        if (buflen)
            buflen--;
        }
    operator const char *() const{
        return buffer;
        }
    void operator=(const char *p) {
        if (buffer) {
            Clear();
            Add(p);
            }
        else
            SetString(p);
        }
    };

#endif