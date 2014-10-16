/**********************************************************/
/* Implementation (would normally go in its own .c file): */

#define _CRT_SECURE_NO_WARNINGS 1
#include <tchar.h>
#include <windows.h>
#include <string.h>
#include "punycode.h"
#include "buf.h"


/*** Bootstring parameters for Punycode ***/

enum { base = 36, tmin = 1, tmax = 26, skew = 38, damp = 700,
       initial_bias = 72, initial_n = 0x80, delimiter = 0x2D };

/* basic(cp) tests whether cp is a basic code point: */
#define basic(cp) ((punycode_uint)(cp) < 0x80)

/* delim(cp) tests whether cp is a delimiter: */
#define delim(cp) ((cp) == delimiter)

/* decode_digit(cp) returns the numeric value of a basic code */
/* point (for use in representing integers) in the range 0 to */
/* base-1, or base if cp is does not represent a value.       */

static punycode_uint decode_digit(punycode_uint cp)
{
    return  ((cp - 48) < 10) ? (cp - 22) :
            ((cp - 65) < 26) ? (cp - 65) :
            ((cp - 97) < 26) ? (cp - 97) : base;
}

/* encode_digit(d,flag) returns the basic code point whose value      */
/* (when used for representing integers) is d, which needs to be in   */
/* the range 0 to base-1.  The lowercase form is used unless flag is  */
/* nonzero, in which case the uppercase form is used.  The behavior   */
/* is undefined if flag is nonzero and digit d has no uppercase form. */

static _TCHAR encode_digit(punycode_uint d, int flag)
{
    return (_TCHAR)((d + 22 + 75 * (d < 26) - ((flag != 0) << 5)) & 0x0FF);
    /*  0..25 map to ASCII a..z or A..Z */
    /* 26..35 map to ASCII 0..9         */
}

/* flagged(bcp) tests whether a basic code point is flagged */
/* (uppercase).  The behavior is undefined if bcp is not a  */
/* basic code point.                                        */

#define flagged(bcp) ((punycode_uint)(bcp) - 65 < 26)

/* encode_basic(bcp,flag) forces a basic code point to lowercase */
/* if flag is zero, uppercase if flag is nonzero, and returns    */
/* the resulting code point.  The code point is unchanged if it  */
/* is caseless.  The behavior is undefined if bcp is not a basic */
/* code point.                                                   */

static _TCHAR encode_basic(punycode_uint bcp, int flag)
{

    bcp -= ((bcp - 97) < 26) << 5;
    return (_TCHAR)((bcp + ((!flag && ((bcp - 65) < 26)) << 5)) & 0x0FF);
}

/*** Platform-specific constants ***/

/* maxint is the maximum value of a punycode_uint variable: */
static const punycode_uint maxint = (punycode_uint)(-1);
/* Because maxint is unsigned, -1 becomes the maximum value. */

/*** Bias adaptation function ***/

static punycode_uint adapt( punycode_uint delta, punycode_uint numpoints, int firsttime )
{
    punycode_uint k;

    delta = (firsttime) ? (delta / damp) : (delta >> 1);
    /* delta >> 1 is a faster way of doing delta / 2 */
    delta += (delta / numpoints);

    for (k = 0;  delta > (((base - tmin) * tmax) / 2);  k += base) {
        delta /= (base - tmin);
    }

    return k + (base - tmin + 1) * delta / (delta + skew);
}

/*** Main encode function ***/

punycode_status punycode_encode( punycode_uint input_length,
                                 const punycode_uint input[],
                                 const BYTE case_flags[],
                                 punycode_uint *output_length,
                                 _TCHAR output[] )
{
    punycode_uint n, delta, h, b, out, max_out, bias, j, m, q, k, t;

    /* Initialize the state: */

    n = initial_n;
    delta = out = 0;
    max_out = *output_length;
    bias = initial_bias;

    /* Handle the basic code points: */

    for (j = 0;  j < input_length;  ++j) {
        if (basic(input[j])) {
            if ( (max_out - out) < 2 )
                return punycode_big_output;

            output[out++] = case_flags ? (_TCHAR)encode_basic(input[j], case_flags[j]) : (_TCHAR)input[j];
        }
        /* else if (input[j] < n) return punycode_bad_input; */
        /* (not needed for Punycode with unsigned code points) */
    }

    h = b = out;

    /* h is the number of code points that have been handled, b is the  */
    /* number of basic code points, and out is the number of characters */
    /* that have been output.                                           */

    if (b > 0)
        output[out++] = (_TCHAR)delimiter;

    /* Main encoding loop: */

    while (h < input_length) {
        /* All non-basic code points < n have been     */
        /* handled already.  Find the next larger one: */

        for (m = maxint, j = 0;  j < input_length;  ++j) {
            /* if (basic(input[j])) continue; */
            /* (not needed for Punycode) */
            if ( (input[j] >= n) && (input[j] < m) )
                m = input[j];
        }

        /* Increase delta enough to advance the decoder's    */
        /* <n,i> state to <m,0>, but guard against overflow: */

        if ( (m - n) > ((maxint - delta) / (h + 1)) )
            return punycode_overflow;

        delta += (m - n) * (h + 1);
        n = m;

        for (j = 0;  j < input_length;  ++j) {
            /* Punycode does not need to check whether input[j] is basic: */
            if ( (input[j] < n) /* || basic(input[j]) */ ) {
                if (++delta == 0)
                    return punycode_overflow;
            }

            if (input[j] == n) {
                /* Represent delta as a generalized variable-length integer: */

                for (q = delta, k = base;  ;  k += base) {
                    if (out >= max_out)
                        return punycode_big_output;

                    t = (k <= (bias /* + tmin */)) ? tmin :     /* +tmin not needed */
                        (k >= (bias + tmax))       ? tmax : k - bias;
                    if (q < t)
                        break;
                    output[out++] = (_TCHAR)encode_digit(t + (q - t) % (base - t), 0);
                    q = (q - t) / (base - t);
                }

                output[out++] = (_TCHAR)encode_digit(q, case_flags && case_flags[j]);
                bias = adapt(delta, h + 1, h == b);
                delta = 0;
                ++h;
            }
        }

        ++delta, ++n;
    }

    *output_length = out;
    return punycode_success;
}

/*** Main decode function ***/

punycode_status punycode_decode( punycode_uint input_length,
                                 const _TCHAR input[],
                                 punycode_uint *output_length,
                                 punycode_uint output[],
                                 BYTE case_flags[] )
{
    punycode_uint n, out, i, max_out, bias,
                  b, j, in, oldi, w, k, digit, t;

    /* Initialize the state: */

    n = initial_n;
    out = i = 0;
    max_out = *output_length;
    bias = initial_bias;

    /* Handle the basic code points:  Let b be the number of input code */
    /* points before the last delimiter, or 0 if there is none, then    */
    /* copy the first b code points to the output.                      */

    for (b = j = 0;  j < input_length;  ++j)
        if (delim(input[j]))
            b = j;

    if (b > max_out)
        return punycode_big_output;

    for (j = 0;  j < b;  ++j) {

        if (case_flags)
            case_flags[out] = (BYTE)flagged(input[j]);

        if (!basic(input[j]))
            return punycode_bad_input;

        output[out++] = input[j];
    }

    /* Main decoding loop:  Start just after the last delimiter if any  */
    /* basic code points were copied; start at the beginning otherwise. */

    for (in = (b > 0) ? (b + 1) : 0;  in < input_length;  ++out) {

        /* in is the index of the next character to be consumed, and */
        /* out is the number of code points in the output array.     */

        /* Decode a generalized variable-length integer into delta,  */
        /* which gets added to i.  The overflow checking is easier   */
        /* if we increase i as we go, then subtract off its starting */
        /* value at the end to obtain delta.                         */

        for ( oldi = i, w = 1, k = base;  ;  k += base ) {
            if ( in >= input_length )
                return punycode_bad_input;

            digit = decode_digit(input[in++]);
            if ( digit >= base )
                return punycode_bad_input;

            if ( digit > ((maxint - i) / w) )
                return punycode_overflow;

            i += digit * w;
            t = (k <= (bias /* + tmin */)) ? tmin :     /* +tmin not needed */
                (k >= (bias + tmax))       ? tmax : k - bias;
            if (digit < t)
                break;

            if ( w > (maxint / (base - t)) )
                return punycode_overflow;

            w *= (base - t);
        }

        bias = adapt(i - oldi, out + 1, oldi == 0);

        /* i was supposed to wrap around from out+1 to 0,   */
        /* incrementing n each time, so we'll fix that now: */

        if ( (i / (out + 1)) > (maxint - n) )
            return punycode_overflow;

        n += i / (out + 1);
        i %= (out + 1);

        /* Insert n at position i of the output: */

        /* not needed for Punycode: */
        /* if (decode_digit(n) <= base) return punycode_invalid_input; */
        if (out >= max_out)
            return punycode_big_output;

        if (case_flags) {
            memmove(case_flags + i + 1, case_flags + i, (out - i) * sizeof(_TCHAR));

            /* Case of last character determines uppercase flag: */
            case_flags[i] = (BYTE)flagged(input[in - 1]);
        }

        memmove(output + i + 1, output + i, (out - i) * sizeof(_TCHAR) * sizeof *output);
        output[i++] = n;
    }

    *output_length = out;
    return punycode_success;
}

#if defined(PUNYCODE_TEST)

/******************************************************************/
/* Wrapper for testing (would normally go in a separate .c file): */

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* For testing, we'll just set some compile-time limits rather than */
/* use malloc(), and set a compile-time option rather than using a  */
/* command-line option.                                             */

static void usage(LPTSTR * argv)
{
    _ftprintf(stderr,
              __T("\n")
              __T("%s -e reads code points and writes a Punycode string.\n")
              __T("%s -d reads a Punycode string and writes code points.\n")
              __T("\n")
              __T("Input and output are plain text in the native character set.\n")
              __T("Code points are in the form u+hex separated by whitespace.\n")
              __T("Although the specification allows Punycode strings to contain\n")
              __T("any characters from the ASCII repertoire, this test code\n")
              __T("supports only the printable characters, and needs the Punycode\n")
              __T("string to be followed by a newline.\n")
              __T("The case of the u in u+hex is the force-to-uppercase flag.\n")
              , argv[0], argv[0]);
}

static void fail(LPCTSTR msg)

{
    _fputts(msg,stderr);
}

static const _TCHAR too_big[]       = __T("input or output is too large, recompile with larger limits\n");
static const _TCHAR invalid_input[] = __T("invalid input\n");
static const _TCHAR overflow[]      = __T("arithmetic overflow\n");
static const _TCHAR io_error[]      = __T("I/O error\n");

/* The following string is used to convert printable */
/* characters between ASCII and the native charset:  */

static _TCHAR print_ascii[] = __T("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n")
                              __T("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n")
                              __T(" !\"#$%&'()*+,-./")
                              __T("0123456789:;<=>?")
                              __T("@ABCDEFGHIJKLMNO")
                              __T("PQRSTUVWXYZ[\\]^_")
                              __T("`abcdefghijklmno")
                              __T("pqrstuvwxyz{|}~\n");

int _tmain(int argc, LPTSTR * argv)
{
    punycode_status status;
    int r;
    unsigned int  input_length, output_length, j;
    _TUCHAR case_flags[unicode_max_length];

    if (argc < 2) {
        usage(argv);
        return(EXIT_FAILURE);
    }

    if (argv[1][0] != __T('-')) {
        usage(argv);
        return(EXIT_FAILURE);
    }

    if (argv[1][2] != __T('\0')) {
        usage(argv);
        return(EXIT_FAILURE);
    }

    if (argv[1][1] == __T('e')) {
        punycode_uint input[unicode_max_length];
        unsigned long codept;
        _TCHAR        output[ace_max_length+1];
        _TCHAR        uplus[3];
        int           c;

        /* Read the input code points: */

        if ( argc == 3 ) {
            LPTSTR there;
            int    needPunycode;
            LPTSTR start;
            Buf    tmpBuf;
            Buf    finishedURL;

            there = argv[2];
            start = there;
            _tcslwr(there);
            tmpBuf.Clear();
            finishedURL.Clear();
            needPunycode = 0;
            for (;;) {
                if ((*there == __T('\0')) || (*there == __T('.'))) {
                    _TCHAR oldC;

                    oldC = *there;
                    *there = __T('\0');
                    if (needPunycode) {
                        input_length = 0;
                        while ( start < there ) {
                            case_flags[input_length] = 0;
                            input[input_length] = (punycode_uint)*start;
                            if ((input[input_length] >= 0xD800) && (input[input_length] <= 0xDBFF) &&
                                (start[1] >= 0xDC00) && (start[1] <= 0xDFFF)) {
                                input[input_length] = 0x10000l + ((input[input_length] & 0x3FFl) << 10) + (start[1] & 0x3FFl);
                                start++;
                            }
                            input_length++;
                            start++;
                        }
                        /* Encode: */
                        output_length = ace_max_length;
                        status = punycode_encode(input_length, input, case_flags,
                                                 &output_length, output);
                        finishedURL.Add( __T("xn--") );
                        finishedURL.Add( output, output_length );
                        needPunycode = 0;
                    } else
                        finishedURL.Add( start );
                    *there = oldC;
                    if (*there++ == __T('\0'))
                        break;
                    finishedURL.Add( oldC );
                    start = there;
                } else
                if (*there++ > 0x00FF) {
                    needPunycode = 1;
                }
            }
            _putts(finishedURL.Get());
            _putts(__T("\n"));
            finishedURL.Free();
            tmpBuf.Free();

        } else {
            input_length = 0;
            for (;;) {
                r = _tscanf( __T("%2s%lx"), uplus, &codept );
                if (ferror(stdin))
                    fail(io_error);

                if ( (r == EOF) || (r == 0) )
                    break;

                if ( (r != 2 )|| (uplus[1] != __T('+')) || (codept > (punycode_uint)-1) ) {
                    fail(invalid_input);
                }

                if (input_length == unicode_max_length)
                    fail(too_big);

                if (uplus[0] == __T('u'))
                    case_flags[input_length] = 0;
                else
                if (uplus[0] == __T('U'))
                    case_flags[input_length] = 1;
                else
                    fail(invalid_input);

                input[input_length++] = codept;
            }

            /* Encode: */

            output_length = ace_max_length;
            status = punycode_encode(input_length, input, case_flags,
                                     &output_length, output);

            if (status == punycode_bad_input)
                fail(invalid_input);

            if (status == punycode_big_output)
                fail(too_big);

            if (status == punycode_overflow)
                fail(overflow);

            assert(status == punycode_success);

            /* Convert to native charset and output: */

            for (j = 0;  j < output_length;  ++j) {
                c = output[j];
                assert(basic(c));
                if (print_ascii[c] == 0)
                    fail(invalid_input);

                output[j] = print_ascii[c];
            }

            output[j] = 0;
            r = _putts(output);
            if (r == EOF)
                fail(io_error);
        }
        return EXIT_SUCCESS;
    }

    if (argv[1][1] == __T('d')) {
        _TCHAR        input[ace_max_length+2];
        LPTSTR        p;
        LPTSTR        pp;
        punycode_uint output[unicode_max_length];

        /* Read the Punycode input string and convert to ASCII: */
        if (argc == 3) {
            input_length = _tcslen(argv[2]);
            _tcscpy(input, argv[2]);
        } else {
            _fgetts(input, ace_max_length+2, stdin);
#if defined(_UNICODE) || defined(UNICODE)
            for ( int i = 0; ; i++ ) {
                input[i] = (_TCHAR)(input[i*2] + (input[(i*2)+1] << 8));
                if ( input[i] == __T('\0') )
                    break;
            }
#endif
            if (ferror(stdin))
                fail(io_error);

            if (feof(stdin))
                fail(invalid_input);

            input_length = _tcslen(input) - 1;
            if (input[input_length] != __T('\n'))
                fail(too_big);
        }
        input[input_length] = 0;

        for (p = input;  *p != 0;  ++p) {
            pp = _tcschr(print_ascii, *p);
            if (pp == 0)
                fail(invalid_input);

            *p = pp - print_ascii;
        }

        /* Decode: */

        output_length = unicode_max_length;
        status = punycode_decode(input_length, input, &output_length,
                                 output, case_flags);
        if (status == punycode_bad_input)
            fail(invalid_input);

        if (status == punycode_big_output)
            fail(too_big);

        if (status == punycode_overflow)
            fail(overflow);

        assert(status == punycode_success);

        /* Output the result: */

        for (j = 0;  j < output_length;  ++j) {
            r = _tprintf(__T("%s+%04lX\n"),
                         case_flags[j] ? __T("U") : __T("u"),
                         (unsigned long) output[j] );
            if (r < 0)
                fail(io_error);
        }

        return EXIT_SUCCESS;
    }

    usage(argv);
    return(EXIT_FAILURE);
}
#endif
