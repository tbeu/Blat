/*
gssfuncs.cpp
*/

#include "declarations.h"

#ifdef __WATCOMC__
    #define WIN32
#endif

#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
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

#if SUPPORT_GSSAPI  //Added 2003-11-20 Joseph Calzaretta

#include "gssfuncs.h" // Please read the comments here for information about how to use GssSession

extern char quiet;
extern void printMsg(const char *p, ... );
extern void server_error (const char * message);
extern char debug;

extern void base64_encode(const unsigned char *in, int length, char *out, int inclCrLf);
extern int base64_decode(const unsigned char *in, char *out);

// Turn results from the server into an integer representing the return code.
int const GssSession::extract_server_retval(const char* str)
{
    int ret = 0;
    for (int i=0; i<3; i++)
    {
        if (!str[i]) return -1;
        if (str[i]<'0') return -1;
        if (str[i]>'9') return -1;
        ret = 10*ret + str[i] - '0';
    }
    return ret;
}

// Get the major/minor status message of this GssException
const char* GssSession::GssException::status_message()
{

    // Don't need to do the rest if we've done it before
    if (statusmsginit) return statusmsg;

    // major==0 & minor==0 means no message
    if (!major_status && !minor_status)
    {
        strcpy(statusmsg, "");
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
        strncpy(statusmsg,sess1.StatusText(major_status, minor_status),1023); //Get the status information
        statusmsg[1023]=0; // Make sure we don't ever have a buffer overrun.
    }
    catch (GssException&) {
        // Can't get a text message
        sprintf(statusmsg,"GSSAPI status message unavailable!\n  Major status=0X%08lX, Minor status=0X%08lX.\n",major_status,minor_status);
    }

    statusmsginit = TRUE;
    return statusmsg;
}


const char* GssSession::GssException::message()
{
    strcpy(bothmsgs,error_message());
    const char *stat = status_message();
    if (*stat)
    {
        strcat(bothmsgs,"\n  ");
        strcat(bothmsgs,stat);
    }
    return bothmsgs;
}

//  Create a GssSession object (see gssfuncs.h)
GssSession::GssSession() : state(Nothing), protection_level(GSSAUTH_P_NONE), max_prewrapped(5192)
{
    hinstLib = LoadLibrary("gssapi32");
    if (!hinstLib) {
        Cleanup();
        GssNoLibrary();
        return;
    }
    state = LoadedLibrary;

    if (!LoadFunction((void *&)my_gss_import_name,"gss_import_name")) {
        Cleanup();
        GssBadFunction("gss_import_name");
        return;
    }
    if (!LoadFunction((void *&)my_gss_display_name,"gss_display_name")) {
        Cleanup();
        GssBadFunction("gss_display_name");
        return;
    }
    if (!LoadFunction((void *&)my_gss_init_sec_context,"gss_init_sec_context")) {
        Cleanup();
        GssBadFunction("gss_init_sec_context");
        return;
    }
    if (!LoadFunction((void *&)my_gss_release_name,"gss_release_name")) {
        Cleanup();
        GssBadFunction("gss_release_name");
        return;
    }
    if (!LoadFunction((void *&)my_gss_unwrap,"gss_unwrap")) {
        Cleanup();
        GssBadFunction("gss_unwrap");
        return;
    }
    if (!LoadFunction((void *&)my_gss_wrap,"gss_wrap")) {
        Cleanup();
        GssBadFunction("gss_wrap");
        return;
    }
    if (!LoadFunction((void *&)my_gss_wrap_size_limit,"gss_wrap_size_limit")) {
        Cleanup();
        GssBadFunction("gss_wrap_size_limit");
        return;
    }
    if (!LoadFunction((void *&)my_gss_delete_sec_context,"gss_delete_sec_context")) {
        Cleanup();
        GssBadFunction("gss_delete_sec_context");
        return;
    }
    if (!LoadFunction((void *&)my_gss_display_status,"gss_display_status")) {
        Cleanup();
        GssBadFunction("gss_display_status");
        return;
    }
    if (!LoadFunction((void *&)my_gss_indicate_mechs,"gss_indicate_mechs")) {
        Cleanup();
        GssBadFunction("gss_indicate_mechs");
        return;
    }
    if (!LoadFunction((void *&)my_gss_oid_to_str,"gss_oid_to_str")) {
        Cleanup();
        GssBadFunction("gss_oid_to_str");
        return;
    }
    if (!LoadFunction((void *&)my_gss_release_buffer,"gss_release_buffer")) {
        Cleanup();
        GssBadFunction("gss_release_buffer");
        return;
    }
    if (!LoadFunction((void *&)my_gss_release_oid_set,"gss_release_oid_set")) {
        Cleanup();
        GssBadFunction("gss_release_oid_set");
        return;
    }

    PMY_GSS_C_NT_HOSTBASED_SERVICE = (gss_OID*) GetProcAddress(hinstLib,"GSS_C_NT_HOSTBASED_SERVICE");
    if (!PMY_GSS_C_NT_HOSTBASED_SERVICE) {
        PMY_GSS_C_NT_HOSTBASED_SERVICE = (gss_OID*) GetProcAddress(hinstLib,"gss_nt_service_name");
    }
    if (!PMY_GSS_C_NT_HOSTBASED_SERVICE) {
        Cleanup();
        GssBadObject("gss_nt_service_name");
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
        if (debug) printMsg("Releasing GSS credentials\n");
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
const char* GssSession::StatusText(OM_uint32 major_status, OM_uint32 minor_status, char *szBuffer)
{

    char *szStatusMsg;
    if (szBuffer)
    {
        szStatusMsg = szBuffer;
    }
    else
    {
        static char szStaticBuffer[1024]="";
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
        if (!firsttime) strncat(szStatusMsg,", ",1023-strlen(szStatusMsg));
        strncat(szStatusMsg,(const char*)stng.value,1023-strlen(szStatusMsg));
        firsttime = FALSE;
    } while (ctxt);
    if (minor_status)
    {
        strncat(szStatusMsg,": ",1023-strlen(szStatusMsg));
        firsttime = TRUE;
        ctxt=0;
        do
        {
            my_gss_display_status(&mst, minor_status, GSS_C_MECH_CODE, GSS_C_NO_OID, &ctxt, &stng);
            if (!firsttime) strncat(szStatusMsg,", ",1023-strlen(szStatusMsg));
            strncat(szStatusMsg,(const char*)stng.value,1023-strlen(szStatusMsg));

            firsttime = FALSE;
        } while (ctxt);
    }

    return szStatusMsg;

}

// Determine name of security mechanism at runtime (see gssfuncs.h)
const char* GssSession::MechtypeText(char *szBuffer)
{
    char *mechtype;
    OM_uint32 maj_stat, min_stat;
    gss_OID_set mech_set;

    if (szBuffer)
    {
        mechtype = szBuffer;
    }
    else
    {
        static char szStaticBuffer[1024]="";
        szStaticBuffer[1023]=0;
        mechtype = szStaticBuffer;
    }

    strcpy(mechtype, "UNKNOWN");

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
                    strcpy(mechtype, (char *) buf.value);
                    my_gss_release_buffer(&min_stat, &buf);
                }
            }

            if ( my_gss_release_oid_set )
                maj_stat = my_gss_release_oid_set(&min_stat, &mech_set);
        }
    }

    // Convert known GSSAPI mechanism OIDs to human-meaningful names,
    //    unkown mechanisms are left as ugly { # # # } string
    if      (_stricmp(mechtype, "{ 1 2 840 113554 1 2 2 }")==0) strcpy(mechtype,"Kerberos v5");
    else if (_stricmp(mechtype, "{ 1 2 840 113554 1 2 3 }")==0) strcpy(mechtype,"Kerberos5 v2");
    else if (_stricmp(mechtype, "{ 1 3 5 1 5 2 }"         )==0) strcpy(mechtype,"Kerberos v5 (old)");
    else if (_stricmp(mechtype, "{ 1 3 6 1 5 5 1 3 }"     )==0) strcpy(mechtype,"SPKM-3 (X.509)");
    else if (_stricmp(mechtype, "{ 1 3 6 1 5 5 1 2 }"     )==0) strcpy(mechtype,"SPKM-2 (X.509)");
    else if (_stricmp(mechtype, "{ 1 3 6 1 5 5 1 1 }"     )==0) strcpy(mechtype,"SPKM-1 (X.509)");
    else if (_stricmp(mechtype, "{ 1 3 6 1 5 5 1 }"       )==0) strcpy(mechtype,"SPKM (X.509)");
    else if (_stricmp(mechtype, "{ 1 3 6 1 5 5 2 }"       )==0) strcpy(mechtype,"SPNEGO pseudo-mechanism");
    else if (_stricmp(mechtype, "{ 1 3 6 1 5 5 9 }"       )==0) strcpy(mechtype,"LIPKEY");

    return mechtype;
}


//  Authenticate to SMTP server (see gssfuncs.h)

void GssSession::Authenticate(BOOL (*getline) (char*), BOOL (*putline) (char*),const char* username, const char* servicename, BOOL mutual, protLev lev)
{

    if (!username) username="";
    if (!servicename) servicename="";

    protection_level = lev;
    mutual_authentication = mutual;

    if (state!=Initialized) {
        Cleanup();
        GssException("Already authenticated.");
        return;
    }

    char out_data[MAXOUTLINE];

    gss_buffer_desc request_buf, send_token;
    gss_buffer_t sec_token;
    gss_name_t target_name;

    gss_OID mech_name;
    gss_qop_t quality;
    int cflags;
    OM_uint32 maj_stat, min_stat;
    char buf1[8192], buf2[8192];
    char server_conf_flags;

    u_long buf_size;
    int result;

    strcpy(buf1,servicename);
    request_buf.char_value = buf1;
    request_buf.length = strlen(buf1) + 1;
    maj_stat = my_gss_import_name(&min_stat, &request_buf, MY_GSS_C_NT_HOSTBASED_SERVICE,
        &target_name);

    if (maj_stat != GSS_S_COMPLETE) {
        char szMsg[1024];
        sprintf(szMsg,"Couldn't get service name for [%s]");
        Cleanup();
        throw GssException(szMsg);
    }


    if (debug)
    {
        maj_stat = my_gss_display_name(&min_stat, target_name, &request_buf,
            &mech_name);
        printMsg("Using service name [%s]\n",request_buf.value);
        maj_stat = my_gss_release_buffer(&min_stat, &request_buf);
    }

    /* now start the security context initialization loop... */
    sec_token = GSS_C_NO_BUFFER;
    context = GSS_C_NO_CONTEXT;
    BOOL firsttime;
    firsttime = TRUE;
    if (debug)
        printMsg("Sending credentials\n");
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
            putline("=QUIT\r\n");
            getline(NULL);
            OM_uint32 tmpstat; my_gss_release_name(&tmpstat, &target_name);
            Cleanup();
            throw GssNonzeroStatus(maj_stat,min_stat,"Error exchanging credentials");
        }

        if (firsttime)
        {
            sprintf (out_data, "AUTH GSSAPI\r\n");
            if (!(putline(out_data))) {
                OM_uint32 tmpstat; my_gss_release_name(&tmpstat, &target_name);
                Cleanup();
                throw GssBadSocket();
            }
            getline(buf1);
            result = extract_server_retval(buf1);
            if ( result != 334 ) {
                char szMsg[1024]="The mail server doesn't require AUTH GSSAPI.  Are you sure the server supports AUTH?";
                sprintf(szMsg+strlen(szMsg)," (Code %d)",result);
                OM_uint32 tmpstat; my_gss_release_name(&tmpstat, &target_name);
                Cleanup();
                throw GssException(szMsg);
            }
        }

        if (send_token.length)
        {
            base64_encode((const unsigned char *)send_token.uchar_value, (int)send_token.length, buf1, FALSE);
            my_gss_release_buffer(&min_stat, &send_token);
            sprintf (out_data, "%s\r\n", buf1);
            if (!(putline(out_data))) {
                OM_uint32 tmpstat; my_gss_release_name(&tmpstat, &target_name);
                Cleanup();
                throw GssBadSocket();
            }
            strcpy(buf1,"");
            getline(buf1);
            result = extract_server_retval(buf1);
            if (result != 334) {
                char szMsg[1024]="The mail server doesn't accept the token.";
                sprintf(szMsg+strlen(szMsg)," (Code %d)",result);
                OM_uint32 tmpstat; my_gss_release_name(&tmpstat, &target_name);
                Cleanup();
                throw GssException(szMsg);
            }

        }
        else
            my_gss_release_buffer(&min_stat, &send_token);

        if (maj_stat == GSS_S_CONTINUE_NEEDED) {
            request_buf.length = base64_decode((const unsigned char*)buf1+4, buf2);
            request_buf.char_value = buf2;
            sec_token = &request_buf;
        } else {
            sprintf (out_data, "\r\n");
            if (!(putline(out_data))) {
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

    strcpy(buf1,"");
    getline(buf1);
    result = extract_server_retval(buf1);

    if (result != 334) {
        char szMsg[1024]="The mail server didn't return the proper security flag/buffer size info.";
        sprintf(szMsg+strlen(szMsg)," (Code %d)",result);
        Cleanup();
        throw GssException(szMsg);

    }

    request_buf.length = base64_decode((const unsigned char*)buf1+4, buf2);
    request_buf.char_value = buf2;

    maj_stat = my_gss_unwrap(&min_stat, context, &request_buf, &send_token,
        &cflags, &quality);

    if (maj_stat != GSS_S_COMPLETE) {
        Cleanup();
        throw GssNonzeroStatus(maj_stat,min_stat,"Couldn't unwrap security level data.");
    }
    if (debug)
        printMsg("Credential exchange complete\n");


    /* first octet is security levels supported. We want none, for now */

    server_conf_flags = (send_token.char_value)[0];
    if ( !((send_token.char_value)[0] & protection_level) ) {

        my_gss_release_buffer(&min_stat, &send_token);
        Cleanup();
        throw GssException("Server does not support required encryption level.");


    }
    (send_token.char_value)[0] = 0;
    buf_size = ntohl( send_token.ulong_value[0] );

    my_gss_release_buffer(&min_stat, &send_token);
    if (debug) {
        printMsg("Server supports: %s%s%s",
            server_conf_flags & GSSAUTH_P_NONE ? "GSSAUTH_P_NONE " : "",
            server_conf_flags & GSSAUTH_P_INTEGRITY ? "GSSAUTH_P_INTEGRITY " : "",
            server_conf_flags & GSSAUTH_P_PRIVACY ? "GSSAUTH_P_PRIVACY" : "");
        printMsg("\nMaximum GSS token size is %ld after wrapping\n",buf_size);
    }

    /* now respond in kind */

    if (protection_level != GSSAUTH_P_NONE)
    {
        maj_stat = my_gss_wrap_size_limit(&min_stat, context, (protection_level==GSSAUTH_P_PRIVACY), GSS_C_QOP_DEFAULT, buf_size, (unsigned int*)&max_prewrapped);

        if (maj_stat != GSS_S_COMPLETE){

            Cleanup();
            throw GssNonzeroStatus(maj_stat,min_stat,"Error determining maxmimum pre-wrapped token size.");
        }
        if (debug)
            printMsg("Maximum GSS token size is %u before wrapping\n",max_prewrapped);

    }

    // Let's claim to support the same size
    buf_size = htonl(buf_size);
    memcpy(buf1, &buf_size, 4);
    buf1[0] = (char) protection_level;

    strcpy(buf1+4, username); /* server decides if principal can connect as user */
    request_buf.length = 4 + strlen(username) + 1;
    request_buf.char_value = buf1;

    maj_stat = my_gss_wrap(&min_stat, context, 0, GSS_C_QOP_DEFAULT, &request_buf,
        &cflags, &send_token);

    if (maj_stat != GSS_S_COMPLETE) {
        Cleanup();
        throw GssNonzeroStatus(maj_stat, min_stat, "Error creating security level request.");
    }

    base64_encode((const unsigned char*)send_token.uchar_value, (int)send_token.length, buf1, FALSE);
    my_gss_release_buffer(&min_stat, &send_token);

    if (debug) {
        printMsg("Requesting GSSAUTH_P_%s protection as \"%s\"\n",
            (protection_level==GSSAUTH_P_NONE)?"NONE":(protection_level==GSSAUTH_P_PRIVACY)?"PRIVACY":"INTEGRITY",
            username);
    }

    sprintf (out_data, "%s\r\n", buf1);

    if (!(putline(out_data))){
        Cleanup();
        throw GssBadSocket();
    }


    /* We should be done. Get status and finish up */

    strcpy(buf1,"");
    getline(buf1);
    result = extract_server_retval(buf1);

    if (result!=235) {

        char szMsg[1024]="The mail server did not properly finalize the authentication.";
        sprintf(szMsg+strlen(szMsg)," (Code %d)",result);
        Cleanup();
        throw GssException(szMsg);

    }

    state = ReadyToCommunicate;
    if (debug) {
        if (protection_level!=GSSAUTH_P_NONE)
            printMsg("From now on, everything is encrypted via GSSAUTH_P_%s.\n",
            (protection_level==GSSAUTH_P_PRIVACY)?"PRIVACY":"INTEGRITY");
    }

}


// Encrypt a message (see gssfuncs.h)
Buf GssSession::Encrypt(const Buf& msg)
{
    if (protection_level==GSSAUTH_P_NONE) return msg;
    Buf outp;

    if (state!=ReadyToCommunicate) {Cleanup(); throw GssException("Not ready to communicate.  Need to authenticate first.");}

    outp.Clear();
    unsigned int bytes_encrypted = 0;

    while (bytes_encrypted < msg.Length())
    {

        OM_uint32 maj_stat, min_stat;
        gss_buffer_desc request_buf, send_token;
        int cflags;

        unsigned int desiredlen = (msg.Length()-bytes_encrypted);
        if (desiredlen > max_prewrapped) desiredlen = max_prewrapped;
        request_buf.length = desiredlen;
        request_buf.void_value = (void*)((const char *)msg + bytes_encrypted);
        bytes_encrypted+=desiredlen;

        maj_stat = my_gss_wrap(&min_stat, context,
            (protection_level==GSSAUTH_P_PRIVACY), GSS_C_QOP_DEFAULT, &request_buf,
            &cflags, &send_token);

        if (maj_stat != GSS_S_COMPLETE) {Cleanup(); throw GssNonzeroStatus(maj_stat,min_stat,"Could not gss_wrap message.");}

        u_long len = htonl((u_long)send_token.length);
        outp.Add((char*)&len,4);
        outp.Add(send_token.char_value, (int)send_token.length);

        my_gss_release_buffer(&min_stat, &send_token);

        if (debug && (bytes_encrypted<msg.Length()))
            printMsg("Message to be encrypted is being split into several tokens: %d bytes left\n",
                msg.Length()-bytes_encrypted);

    }

    return outp;

}

// Decrypt a message (see gssfuncs.h)
Buf GssSession::Decrypt(const Buf& msg)
{

    if (protection_level==GSSAUTH_P_NONE) return msg;
    Buf outp;

    if (state!=ReadyToCommunicate) {Cleanup(); throw GssException("Not ready to communicate.  Need to authenticate first.");}

    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc request_buf, send_token;
    int cflags;

    if (msg.Length()<4)
    {Cleanup(); throw GssException("Encrypted response too short... size byte missing\n");}

    u_long claimed_len = ntohl(*(const u_long*)(const char*)msg);
    u_long actual_len = msg.Length()-4;

    if (claimed_len != actual_len)
    {
        Cleanup();
        char szMsg[1024];
        sprintf(szMsg,"Encrypted response size mismatch: the server claims the message is %d bytes, but actually sent %d bytes.",
            claimed_len,actual_len);
        throw GssException(szMsg);
    }

    request_buf.length = actual_len;
    request_buf.void_value = (void *)((const char *)msg+4);

    gss_qop_t quality;
    maj_stat = my_gss_unwrap(&min_stat, context, &request_buf, &send_token, &cflags, &quality);

    if (maj_stat != GSS_S_COMPLETE) {Cleanup(); throw GssNonzeroStatus(maj_stat,min_stat,"Could not gss_unwrap message.");}

    outp = Buf(send_token.char_value, (int)send_token.length);
    return outp;
}

#endif
