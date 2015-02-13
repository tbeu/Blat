/*
gssfuncs.cpp
*/

#include "declarations.h"

#ifdef __WATCOMC__
    #define WIN32
#endif

#include <tchar.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

/*
#define ntohl(netAddr) \
    (u_long)((u_long)((u_long)((netAddr >>  0) & 0xffl) << 24) | \
             (u_long)((u_long)((netAddr >>  8) & 0xffl) << 16) | \
             (u_long)((u_long)((netAddr >> 16) & 0xffl) <<  8) | \
             (u_long)((u_long)((netAddr >> 24) & 0xffl) <<  0))
#define htonl(netAddr) ntohl(netAddr)
 */

extern "C" {
#if (WINVER > 0x0500)
    #include <winsock2.h>
#else
    #include <winsock.h>
#endif
}
#include "blat.h"
#include "common_data.h"

#if SUPPORT_GSSAPI  //Added 2003-11-20 Joseph Calzaretta

extern void printMsg(COMMON_DATA & CommonData, LPTSTR p, ... );

extern void base64_encode(_TUCHAR * in, size_t length, LPTSTR out, int inclCrLf);
extern int  base64_decode(_TUCHAR * in, LPTSTR out);

// Turn results from the server into an integer representing the return code.
int GssSession::extract_server_retval(LPTSTR str)
{
    int ret = 0;
    for (int i=0; i<3; i++)
    {
        if (!str[i]) return -1;
        if (str[i]<__T('0')) return -1;
        if (str[i]>__T('9')) return -1;
        ret = 10*ret + str[i] - __T('0');
    }
    return ret;
}

// Get the major/minor status message of this GssException
LPTSTR GssSession::GssException::status_message()
{

    // Don't need to do the rest if we've done it before
    if (statusmsginit) return statusmsg;

    // major==0 & minor==0 means no message
    if (!major_status && !minor_status)
    {
        _tcscpy(statusmsg, __T(""));
        statusmsginit = TRUE;
        return statusmsg;
    }

    //This is a little dodgy because it needs to create a GssSession object in order to get
    //its StatusText().  Unfortunately GssSessions throw GssExceptions so we have to be careful to
    //avoid the possibility of infinite regress.  The way to do it is to make sure that GssException's
    //constructor cannot possibly throw an exception, which means this function should NEVER be called
    //when constructing a GssException of any of its derived classes.
    try {
        GssSession sess1;
        _tcsncpy(statusmsg,sess1.StatusText(major_status, minor_status),1023); //Get the status information
        statusmsg[1023]=0; // Make sure we don't ever have a buffer overrun.
    }
    catch (GssException&) {
        // Can't get a text message
        _stprintf(statusmsg,__T("GSSAPI status message unavailable!\n  Major status=0X%08lX, Minor status=0X%08lX.\n"),major_status,minor_status);
    }

    statusmsginit = TRUE;
    return statusmsg;
}


LPTSTR GssSession::GssException::message()
{
    _tcscpy(bothmsgs,error_message());
    LPTSTR stat = status_message();
    if (*stat)
    {
        _tcscat(bothmsgs,__T("\n  "));
        _tcscat(bothmsgs,stat);
    }
    return bothmsgs;
}

//  Create a GssSession object (see gssfuncs.h)
GssSession::GssSession() : state(Nothing), protection_level(GSSAUTH_P_NONE), max_prewrapped(5192)
{
#if defined(_WIN64)
    hinstLib = LoadLibrary(__T("gssapi64"));
#else
    hinstLib = LoadLibrary(__T("gssapi32"));
#endif
    if (!hinstLib) {
        Cleanup();
        GssNoLibrary();
        return;
    }
    state = LoadedLibrary;

    if (!LoadFunction((void *&)my_gss_import_name,"gss_import_name")) {
        Cleanup();
        GssBadFunction(__T("gss_import_name"));
        return;
    }
    if (!LoadFunction((void *&)my_gss_display_name,"gss_display_name")) {
        Cleanup();
        GssBadFunction(__T("gss_display_name"));
        return;
    }
    if (!LoadFunction((void *&)my_gss_init_sec_context,"gss_init_sec_context")) {
        Cleanup();
        GssBadFunction(__T("gss_init_sec_context"));
        return;
    }
    if (!LoadFunction((void *&)my_gss_release_name,"gss_release_name")) {
        Cleanup();
        GssBadFunction(__T("gss_release_name"));
        return;
    }
    if (!LoadFunction((void *&)my_gss_unwrap,"gss_unwrap")) {
        Cleanup();
        GssBadFunction(__T("gss_unwrap"));
        return;
    }
    if (!LoadFunction((void *&)my_gss_wrap,"gss_wrap")) {
        Cleanup();
        GssBadFunction(__T("gss_wrap"));
        return;
    }
    if (!LoadFunction((void *&)my_gss_wrap_size_limit,"gss_wrap_size_limit")) {
        Cleanup();
        GssBadFunction(__T("gss_wrap_size_limit"));
        return;
    }
    if (!LoadFunction((void *&)my_gss_delete_sec_context,"gss_delete_sec_context")) {
        Cleanup();
        GssBadFunction(__T("gss_delete_sec_context"));
        return;
    }
    if (!LoadFunction((void *&)my_gss_display_status,"gss_display_status")) {
        Cleanup();
        GssBadFunction(__T("gss_display_status"));
        return;
    }
    if (!LoadFunction((void *&)my_gss_indicate_mechs,"gss_indicate_mechs")) {
        Cleanup();
        GssBadFunction(__T("gss_indicate_mechs"));
        return;
    }
    if (!LoadFunction((void *&)my_gss_oid_to_str,"gss_oid_to_str")) {
        Cleanup();
        GssBadFunction(__T("gss_oid_to_str"));
        return;
    }
    if (!LoadFunction((void *&)my_gss_release_buffer,"gss_release_buffer")) {
        Cleanup();
        GssBadFunction(__T("gss_release_buffer"));
        return;
    }
    if (!LoadFunction((void *&)my_gss_release_oid_set,"gss_release_oid_set")) {
        Cleanup();
        GssBadFunction(__T("gss_release_oid_set"));
        return;
    }

    PMY_GSS_C_NT_HOSTBASED_SERVICE = (gss_OID*) GetProcAddress(hinstLib,"GSS_C_NT_HOSTBASED_SERVICE");
    if (!PMY_GSS_C_NT_HOSTBASED_SERVICE) {
        PMY_GSS_C_NT_HOSTBASED_SERVICE = (gss_OID*) GetProcAddress(hinstLib,"gss_nt_service_name");
    }
    if (!PMY_GSS_C_NT_HOSTBASED_SERVICE) {
        Cleanup();
        GssBadObject(__T("gss_nt_service_name"));
        return;
    }
    MY_GSS_C_NT_HOSTBASED_SERVICE = *PMY_GSS_C_NT_HOSTBASED_SERVICE;

    state = Initialized;
}

//  Unwind object given the value of state
void GssSession::Cleanup()
{
    OM_uint32 min_stat;

    switch (state) {

    case ReadyToCommunicate:
    case Authenticated:
    case GettingContext:

        /* flush security context */
        //if (CommonData.debug) printMsg( CommonData, __T("Releasing GSS credentials\n") );
        {
            gss_buffer_desc send_token;
            my_gss_delete_sec_context(&min_stat, &context, &send_token);
            my_gss_release_buffer(&min_stat, &send_token);
        }

    case Initialized:
    case LoadedLibrary:
        //unload library
        if (hinstLib) FreeLibrary(hinstLib);
        hinstLib = NULL;

    case Nothing:
    default:
        // nothing left to do
        state = Nothing;
    }

}

//  Destroy a GssSession object (see gssfuncs.h)
GssSession::~GssSession()
{
    Cleanup();
}

//  Get text version of major/minor status pair (see gssfuncs.h)
LPTSTR GssSession::StatusText(OM_uint32 major_status, OM_uint32 minor_status, LPTSTR szBuffer)
{

    LPTSTR szStatusMsg;
    if (szBuffer)
    {
        szStatusMsg = szBuffer;
    } else {
        static _TCHAR szStaticBuffer[1024]=__T("");
        szStaticBuffer[1023]=0;
        szStatusMsg = szStaticBuffer;
    }
    szStatusMsg[0]=0;

    OM_uint32 mst, ctxt=0;
    gss_buffer_desc stng;
    BOOL firsttime = TRUE;
    ctxt=0;
    do
    {
        my_gss_display_status(&mst, major_status, GSS_C_GSS_CODE, GSS_C_NO_OID, &ctxt, &stng);
        if (!firsttime) _tcsncat(szStatusMsg,__T(", "),1023-_tcslen(szStatusMsg));
        _tcsncat(szStatusMsg,(LPTSTR)stng.value,1023-_tcslen(szStatusMsg));
        firsttime = FALSE;
    } while (ctxt);
    if (minor_status)
    {
        _tcsncat(szStatusMsg,__T(": "),1023-_tcslen(szStatusMsg));
        firsttime = TRUE;
        ctxt=0;
        do
        {
            my_gss_display_status(&mst, minor_status, GSS_C_MECH_CODE, GSS_C_NO_OID, &ctxt, &stng);
            if (!firsttime) _tcsncat(szStatusMsg,__T(", "),1023-_tcslen(szStatusMsg));
            _tcsncat(szStatusMsg,(LPTSTR)stng.value,1023-_tcslen(szStatusMsg));

            firsttime = FALSE;
        } while (ctxt);
    }

    return szStatusMsg;

}

// Determine name of security mechanism at runtime (see gssfuncs.h)
LPTSTR GssSession::MechtypeText(LPTSTR szBuffer)
{
    LPTSTR mechtype;
    OM_uint32 maj_stat, min_stat;
    gss_OID_set mech_set;

    if (szBuffer)
    {
        mechtype = szBuffer;
    } else {
        static _TCHAR szStaticBuffer[1024]=__T("");
        szStaticBuffer[1023]=0;
        mechtype = szStaticBuffer;
    }

    _tcscpy(mechtype, __T("UNKNOWN"));

    if ( my_gss_indicate_mechs ) {
        maj_stat = my_gss_indicate_mechs(&min_stat, &mech_set);

        if ((!maj_stat) && (mech_set->count))
        {
            {
                gss_OID oid = &(mech_set->elements[0]);

                gss_buffer_desc buf;

                maj_stat = my_gss_oid_to_str(&min_stat, oid, &buf);

                if (!maj_stat)
                {
                    _tcscpy(mechtype, (LPTSTR) buf.value);
                    my_gss_release_buffer(&min_stat, &buf);
                }
            }

            if ( my_gss_release_oid_set )
                maj_stat = my_gss_release_oid_set(&min_stat, &mech_set);
        }
    }

    // Convert known GSSAPI mechanism OIDs to human-meaningful names,
    //    unknown mechanisms are left as ugly { # # # } string
    if      (_tcsicmp(mechtype, __T("{ 1 2 840 113554 1 2 2 }"))==0) _tcscpy(mechtype,__T("Kerberos v5"));
    else if (_tcsicmp(mechtype, __T("{ 1 2 840 113554 1 2 3 }"))==0) _tcscpy(mechtype,__T("Kerberos5 v2"));
    else if (_tcsicmp(mechtype, __T("{ 1 3 5 1 5 2 }")         )==0) _tcscpy(mechtype,__T("Kerberos v5 (old)"));
    else if (_tcsicmp(mechtype, __T("{ 1 3 6 1 5 5 1 3 }")     )==0) _tcscpy(mechtype,__T("SPKM-3 (X.509)"));
    else if (_tcsicmp(mechtype, __T("{ 1 3 6 1 5 5 1 2 }")     )==0) _tcscpy(mechtype,__T("SPKM-2 (X.509)"));
    else if (_tcsicmp(mechtype, __T("{ 1 3 6 1 5 5 1 1 }")     )==0) _tcscpy(mechtype,__T("SPKM-1 (X.509)"));
    else if (_tcsicmp(mechtype, __T("{ 1 3 6 1 5 5 1 }")       )==0) _tcscpy(mechtype,__T("SPKM (X.509)"));
    else if (_tcsicmp(mechtype, __T("{ 1 3 6 1 5 5 2 }")       )==0) _tcscpy(mechtype,__T("SPNEGO pseudo-mechanism"));
    else if (_tcsicmp(mechtype, __T("{ 1 3 6 1 5 5 9 }")       )==0) _tcscpy(mechtype,__T("LIPKEY"));

    return mechtype;
}


//  Authenticate to SMTP server (see gssfuncs.h)

void GssSession::Authenticate(COMMON_DATA & CommonData, BOOL (*getline) (struct _COMMON_DATA &,LPTSTR), BOOL (*putline) (struct _COMMON_DATA &,LPTSTR), LPTSTR username, LPTSTR servicename, BOOL mutual, protLev lev)
{

    if (!username) username=__T("");
    if (!servicename) servicename=__T("");

    protection_level = lev;
    mutual_authentication = mutual;

    if (state!=Initialized) {
        Cleanup();
        GssException(__T("Already authenticated."));
        return;
    }

    _TCHAR out_data[MAXOUTLINE];

    gss_buffer_desc request_buf, send_token;
    gss_buffer_t sec_token;
    gss_name_t target_name;

    gss_OID mech_name;
    gss_qop_t quality;
    int cflags;
    OM_uint32 maj_stat, min_stat;
    _TCHAR buf1[8192], buf2[8192];
    _TCHAR server_conf_flags;

    u_long buf_size;
    int result;

    _tcscpy(buf1,servicename);
    request_buf.char_value = buf1;
    request_buf.length = _tcslen(buf1) + 1;
    maj_stat = my_gss_import_name(&min_stat, &request_buf, MY_GSS_C_NT_HOSTBASED_SERVICE, &target_name);

    if (maj_stat != GSS_S_COMPLETE) {
        _TCHAR szMsg[1024];
        _stprintf(szMsg,__T("Couldn't get service name for [%s]"), buf1);
        Cleanup();
        throw GssException(szMsg);
    }


    if (CommonData.debug)
    {
        maj_stat = my_gss_display_name(&min_stat, target_name, &request_buf, &mech_name);
        printMsg(CommonData, __T("Using service name [%s]\n"),request_buf.value);
        maj_stat = my_gss_release_buffer(&min_stat, &request_buf);
    }

    /* now start the security context initialization loop... */
    sec_token = GSS_C_NO_BUFFER;
    context = GSS_C_NO_CONTEXT;
    BOOL firsttime;
    firsttime = TRUE;
    if (CommonData.debug)
        printMsg(CommonData, __T("Sending credentials\n"));
    state = GettingContext;
    do {
        min_stat = NULL;
        maj_stat = my_gss_init_sec_context(&min_stat, GSS_C_NO_CREDENTIAL,
            &context,
            target_name,
            GSS_C_NO_OID,
            (mutual_authentication ? GSS_C_MUTUAL_FLAG : 0),
            0,
            NULL,
            sec_token,
            NULL,
            &send_token,
            (unsigned int*)&cflags,
            NULL);


        if ((maj_stat!=GSS_S_COMPLETE) && (maj_stat!=GSS_S_CONTINUE_NEEDED)) {
            putline(CommonData, __T("=QUIT\r\n"));
            getline(CommonData, NULL);
            OM_uint32 tmpstat; my_gss_release_name(&tmpstat, &target_name);
            Cleanup();
            throw GssNonzeroStatus(maj_stat,min_stat,__T("Error exchanging credentials"));
        }

        if (firsttime)
        {
            _stprintf (out_data, __T("AUTH GSSAPI\r\n"));
            if (!(putline(CommonData, out_data))) {
                OM_uint32 tmpstat; my_gss_release_name(&tmpstat, &target_name);
                Cleanup();
                throw GssBadSocket();
            }
            getline(CommonData, buf1);
            result = extract_server_retval(buf1);
            if ( result != 334 ) {
                _TCHAR szMsg[1024]=__T("The mail server doesn't require AUTH GSSAPI.  Are you sure the server supports AUTH?");
                _stprintf(szMsg+_tcslen(szMsg),__T(" (Code %d)"),result);
                OM_uint32 tmpstat; my_gss_release_name(&tmpstat, &target_name);
                Cleanup();
                throw GssException(szMsg);
            }
        }

        if (send_token.length)
        {
            base64_encode((_TUCHAR *)send_token.uchar_value, (size_t)send_token.length, buf1, FALSE);
            my_gss_release_buffer(&min_stat, &send_token);
            _stprintf (out_data, __T("%s\r\n"), buf1);
            if (!(putline(CommonData, out_data))) {
                OM_uint32 tmpstat; my_gss_release_name(&tmpstat, &target_name);
                Cleanup();
                throw GssBadSocket();
            }
            _tcscpy(buf1,__T(""));
            getline(CommonData, buf1);
            result = extract_server_retval(buf1);
            if (result != 334) {
                _TCHAR szMsg[1024]=__T("The mail server doesn't accept the token.");
                _stprintf(szMsg+_tcslen(szMsg),__T(" (Code %d)"),result);
                OM_uint32 tmpstat; my_gss_release_name(&tmpstat, &target_name);
                Cleanup();
                throw GssException(szMsg);
            }

        } else
            my_gss_release_buffer(&min_stat, &send_token);

        if (maj_stat == GSS_S_CONTINUE_NEEDED) {
            request_buf.length = base64_decode((_TUCHAR *)buf1+4, buf2);
            request_buf.char_value = buf2;
            sec_token = &request_buf;
        } else {
            _stprintf (out_data, __T("\r\n"));
            if (!(putline(CommonData, out_data))) {
                OM_uint32 tmpstat; my_gss_release_name(&tmpstat, &target_name);
                Cleanup();
                throw GssBadSocket();
            }
        }

        firsttime=FALSE;

    } while (maj_stat == GSS_S_CONTINUE_NEEDED);
    OM_uint32 tmpstat; my_gss_release_name(&tmpstat, &target_name);
    state = Authenticated;



    /* get security flags and buffer size */

    _tcscpy(buf1,__T(""));
    getline(CommonData, buf1);
    result = extract_server_retval(buf1);

    if (result != 334) {
        _TCHAR szMsg[1024]=__T("The mail server didn't return the proper security flag/buffer size info.");
        _stprintf(szMsg+_tcslen(szMsg),__T(" (Code %d)"),result);
        Cleanup();
        throw GssException(szMsg);

    }

    request_buf.length = base64_decode((_TUCHAR *)buf1+4, buf2);
    request_buf.char_value = buf2;

    maj_stat = my_gss_unwrap(&min_stat, context, &request_buf, &send_token,
        &cflags, &quality);

    if (maj_stat != GSS_S_COMPLETE) {
        Cleanup();
        throw GssNonzeroStatus(maj_stat,min_stat,__T("Couldn't unwrap security level data."));
    }
    if (CommonData.debug)
        printMsg(CommonData, __T("Credential exchange complete\n"));


    /* first octet is security levels supported. We want none, for now */

    server_conf_flags = (send_token.char_value)[0];
    if ( !((send_token.char_value)[0] & protection_level) ) {

        my_gss_release_buffer(&min_stat, &send_token);
        Cleanup();
        throw GssException(__T("Server does not support required encryption level."));


    }
    (send_token.char_value)[0] = 0;
    buf_size = ntohl( send_token.ulong_value[0] );

    my_gss_release_buffer(&min_stat, &send_token);
    if (CommonData.debug) {
        printMsg(CommonData, __T("Server supports: %s%s%s"),
                 (server_conf_flags & GSSAUTH_P_NONE)      ? __T("GSSAUTH_P_NONE ")      : __T(""),
                 (server_conf_flags & GSSAUTH_P_INTEGRITY) ? __T("GSSAUTH_P_INTEGRITY ") : __T(""),
                 (server_conf_flags & GSSAUTH_P_PRIVACY)   ? __T("GSSAUTH_P_PRIVACY")    : __T(""));
        printMsg(CommonData, __T("\nMaximum GSS token size is %lu after wrapping\n"),buf_size);
    }

    /* now respond in kind */

    if (protection_level != GSSAUTH_P_NONE)
    {
        maj_stat = my_gss_wrap_size_limit(&min_stat, context, (protection_level==GSSAUTH_P_PRIVACY), GSS_C_QOP_DEFAULT, buf_size, (unsigned int*)&max_prewrapped);

        if (maj_stat != GSS_S_COMPLETE){

            Cleanup();
            throw GssNonzeroStatus(maj_stat,min_stat,__T("Error determining maxmimum pre-wrapped token size."));
        }
        if (CommonData.debug)
            printMsg(CommonData, __T("Maximum GSS token size is %lu before wrapping\n"),max_prewrapped);

    }

    // Let's claim to support the same size
    buf_size = htonl(buf_size);
    memcpy(buf1, &buf_size, 4);
    buf1[0] = (char) protection_level;

    _tcscpy(buf1+4, username); /* server decides if principal can connect as user */
    request_buf.length = 4 + _tcslen(username) + 1;
    request_buf.char_value = buf1;

    maj_stat = my_gss_wrap(&min_stat, context, 0, GSS_C_QOP_DEFAULT, &request_buf,
        &cflags, &send_token);

    if (maj_stat != GSS_S_COMPLETE) {
        Cleanup();
        throw GssNonzeroStatus(maj_stat, min_stat, __T("Error creating security level request."));
    }

    base64_encode((_TUCHAR *)send_token.uchar_value, (size_t)send_token.length, buf1, FALSE);
    my_gss_release_buffer(&min_stat, &send_token);

    if (CommonData.debug) {
        printMsg(CommonData, __T("Requesting GSSAUTH_P_%s protection as \"%s\"\n"),
                 (protection_level==GSSAUTH_P_NONE)?"NONE":(protection_level==GSSAUTH_P_PRIVACY)?"PRIVACY":"INTEGRITY",
                 username);
    }

    _stprintf (out_data, __T("%s\r\n"), buf1);

    if (!(putline(CommonData, out_data))){
        Cleanup();
        throw GssBadSocket();
    }


    /* We should be done. Get status and finish up */

    _tcscpy(buf1,__T(""));
    getline(CommonData, buf1);
    result = extract_server_retval(buf1);

    if (result!=235) {

        _TCHAR szMsg[1024]=__T("The mail server did not properly finalize the authentication.");
        _stprintf(szMsg+_tcslen(szMsg),__T(" (Code %d)"),result);
        Cleanup();
        throw GssException(szMsg);

    }

    state = ReadyToCommunicate;
    if (CommonData.debug) {
        if (protection_level!=GSSAUTH_P_NONE)
            printMsg(CommonData, __T("From now on, everything is encrypted via GSSAUTH_P_%s.\n"),
                     (protection_level==GSSAUTH_P_PRIVACY)?__T("PRIVACY"):__T("INTEGRITY"));
    }

}


// Encrypt a message (see gssfuncs.h)
Buf GssSession::Encrypt(COMMON_DATA & CommonData, Buf& msg)
{
    if (protection_level==GSSAUTH_P_NONE) return msg;
    Buf outp;

    if (state!=ReadyToCommunicate) {Cleanup(); throw GssException(__T("Not ready to communicate.  Need to authenticate first."));}

    outp.Clear();
    unsigned int bytes_encrypted = 0;

    while (bytes_encrypted < msg.Length())
    {

        OM_uint32 maj_stat, min_stat;
        gss_buffer_desc request_buf, send_token;
        int cflags;

        unsigned int desiredlen = (unsigned int)(msg.Length()-bytes_encrypted);
        if (desiredlen > max_prewrapped) desiredlen = max_prewrapped;
        request_buf.length = desiredlen;
        request_buf.void_value = (void*)(msg.Get() + bytes_encrypted);
        bytes_encrypted+=desiredlen;

        maj_stat = my_gss_wrap(&min_stat, context,
            (protection_level==GSSAUTH_P_PRIVACY), GSS_C_QOP_DEFAULT, &request_buf,
            &cflags, &send_token);

        if (maj_stat != GSS_S_COMPLETE) {Cleanup(); throw GssNonzeroStatus(maj_stat,min_stat,__T("Could not gss_wrap message."));}

        u_long len = htonl((u_long)send_token.length);
        outp.Add((LPTSTR)&len,4);
        outp.Add(send_token.char_value, (int)send_token.length);

        my_gss_release_buffer(&min_stat, &send_token);

        if (CommonData.debug && (bytes_encrypted<msg.Length()))
            printMsg(CommonData, __T("Message to be encrypted is being split into several tokens: %d bytes left\n"),
                     msg.Length()-bytes_encrypted);

    }

    return outp;

}

// Decrypt a message (see gssfuncs.h)
Buf GssSession::Decrypt(Buf& msg)
{
    LPTSTR pStr;

    if (protection_level==GSSAUTH_P_NONE) return msg;
    Buf outp;

    if (state!=ReadyToCommunicate) {Cleanup(); throw GssException(__T("Not ready to communicate.  Need to authenticate first."));}

    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc request_buf, send_token;
    int cflags;

    if (msg.Length()<4)
    {Cleanup(); throw GssException(__T("Encrypted response too short... size byte missing\n"));}

    pStr = msg.Get();
    u_long claimed_len = (u_long)((pStr[0] & 0xFFl) << 24) | (u_long)((pStr[1] & 0xFFl) << 16) | (u_long)((pStr[2] & 0xFFl) << 8) | (u_long)((pStr[3] & 0xFFl) << 0);
    u_long actual_len = (u_long)(msg.Length()-4);

    if (claimed_len != actual_len)
    {
        Cleanup();
        _TCHAR szMsg[1024];
        _stprintf(szMsg,__T("Encrypted response size mismatch: the server claims the message is %lu bytes, but actually sent %lu bytes."),
                  claimed_len,actual_len);
        throw GssException(szMsg);
    }

    pStr += 4;
    request_buf.length = actual_len;
    request_buf.void_value = (void *)pStr;

    gss_qop_t quality;
    maj_stat = my_gss_unwrap(&min_stat, context, &request_buf, &send_token, &cflags, &quality);

    if (maj_stat != GSS_S_COMPLETE) {Cleanup(); throw GssNonzeroStatus(maj_stat,min_stat,__T("Could not gss_unwrap message."));}

    outp = Buf(send_token.char_value, (int)send_token.length);
    return outp;
}

#endif
