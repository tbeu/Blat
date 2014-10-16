#ifndef GSSFUNCS_H_0ABDBB3F_385E_4ac0_8E1E_438680517515
#define GSSFUNCS_H_0ABDBB3F_385E_4ac0_8E1E_438680517515

#include <stdio.h>
#include <windows.h>

#include "buf.h"
#include "gensock.h"
#include "gssapi_generic.h"

/*

This is the header for the GssSession class.

There is a global object, pGss, which is a pointer to a GssSession object, if one exists.
The code should always ensure that (!pGss) and then use pGss-> to access GSSAPI capabilities.

GssSession functions may throw exceptions of type GssSession::GssException, which has several derived types.

The public capabilities of GssSession are:

  GssSession()
    creates an object, loads the GSSAPI library and the corresponding functions needed from the library.
    If it fails to load the library it will throw a GssSession::GssNoLibrary exception.

  ~GssSession()
    destroys the object, cleaning up and unloading the library as necessary.

  LPTSTR MechtypeText(LPTSTR szBuffer = NULL);
    determines the name of the default GSSAPI security mechanism, which can only be done at runtime.
    if a buffer is passed into szBuffer, the string is copied into it (and returned).
    if no buffer is passed, it returns a pointer to memory from within the object... this memory can
      be considered valid until the object is destroyed, and the data within it is valid until
      this function is called again.

  LPTSTR StatusText(OM_uint32 major_status, OM_uint32 minor_status, LPTSTR szBuffer = NULL)
    takes a GSSAPI major and minor status codes and converts them to a string representation.
    if a buffer is passed into szBuffer, the string is copied into it (and returned).
    if no buffer is passed, it returns a pointer to memory from within the object... this memory can
      be considered valid until the object is destroyed, and the data within it is valid until
      this function is called again.

  void Authenticate(BOOL (*getline) (LPTSTR), BOOL (*putline) (LPTSTR),LPTSTR username, LPTSTR servicename, BOOL mutual, protLev lev)
    This function does the actual authentication to the server.

    getline and putline are pointers to functions taking a LPTSTR and returning a BOOL.
      These functions are used by Authenticate() to talk to the server.  getline overwrites its argument,
      while putline just reads from its argument.  Both return TRUE for success and FALSE for failure.
      These functions are a compromise to preserve the abstraction barrier between GssSession and the
      server I/O.  I had originally implemented a stream class derived from iostream, which was "cleaner"
      but a lot more complicated and seemed to make the binary bigger by dragging in a whole bunch of
      unnecessary iostream functionality.

    username is the client username to authenticate as, and comes from the "-u" parameter.  The server will decide
      if the available GSSAPI security context has the authority to authenticate as username.

    servicename is the name of the smtp service to contact, and comes from the "-service" parameter. (If that
      parameter is not specified, the system should use "smtp@server" where server comes from the "-server"
      parameter.)

    mutual is either TRUE for mutual authentication (the "-k" option) or FALSE for client-only authentication
      (the "-kc" option)

    lev is the protection level for the authenticated session.  It uses the "-level" parameter and can be one of
      GSSAUTH_P_NONE (cleartext over the wire),
      GSSAUTH_P_INTEGRITY (data integrity guaranteed, encoded over the wire)
      GSSAUTH_P_PRIVACY (data integrity and privacy guaranteed, encoded and encrypted over the wire), the default

  protLev GetProtectionLevel() const
    Gets the value of lev that was passed into Authenticate().

  BOOL IsReadyToEncrypt() const
    determines if the GssSession object is ready to encrypt or decrypt messages.  Only is TRUE after Authenticate()
    has succeeded and if the protection level isn't set to GSSAUTH_P_NONE

  Buf Encrypt(Buf& msg)
    Takes an unencrypted buffer as input, and returns the encrypted buffer as output.

  Buf Decrypt(Buf& msg)
    Takes an encrypted buffer as input, and returns the decrypted buffer as output.

*/

enum protLev {GSSAUTH_P_NONE=1, GSSAUTH_P_INTEGRITY=2, GSSAUTH_P_PRIVACY=4};
typedef GSS_DLLIMP OM_uint32 (KRB5_CALLCONV * MY_GSS_IMPORT_NAME)         (OM_uint32 FAR *, gss_buffer_t, gss_OID, gss_name_t FAR * );
typedef GSS_DLLIMP OM_uint32 (KRB5_CALLCONV * MY_GSS_DISPLAY_NAME)        (OM_uint32 FAR *, gss_name_t, gss_buffer_t, gss_OID FAR * );
typedef GSS_DLLIMP OM_uint32 (KRB5_CALLCONV * MY_GSS_RELEASE_BUFFER)      (OM_uint32 FAR *, gss_buffer_t );
typedef GSS_DLLIMP OM_uint32 (KRB5_CALLCONV * MY_GSS_RELEASE_NAME)        (OM_uint32 FAR *, gss_name_t );
typedef GSS_DLLIMP OM_uint32 (KRB5_CALLCONV * MY_GSS_INIT_SEC_CONTEXT)    (OM_uint32 FAR *, gss_cred_id_t, gss_ctx_id_t FAR *, gss_name_t, gss_OID, OM_uint32, OM_uint32, gss_channel_bindings_t, gss_buffer_t, gss_OID FAR *, gss_buffer_t, OM_uint32 FAR *, OM_uint32 FAR * );
typedef GSS_DLLIMP OM_uint32 (KRB5_CALLCONV * MY_GSS_UNWRAP)              (OM_uint32 FAR *, gss_ctx_id_t, gss_buffer_t, gss_buffer_t, int FAR *, gss_qop_t FAR * );
typedef GSS_DLLIMP OM_uint32 (KRB5_CALLCONV * MY_GSS_WRAP)                (OM_uint32 FAR *, gss_ctx_id_t, int, gss_qop_t, gss_buffer_t, int FAR *, gss_buffer_t );
typedef GSS_DLLIMP OM_uint32 (KRB5_CALLCONV * MY_GSS_WRAP_SIZE_LIMIT)     (OM_uint32 FAR *, gss_ctx_id_t, int, gss_qop_t, OM_uint32, OM_uint32 *);
typedef GSS_DLLIMP OM_uint32 (KRB5_CALLCONV * MY_GSS_DELETE_SEC_CONTEXT)  (OM_uint32 FAR *, gss_ctx_id_t FAR *, gss_buffer_t );
typedef GSS_DLLIMP OM_uint32 (KRB5_CALLCONV * MY_GSS_DISPLAY_STATUS)      (OM_uint32 FAR *, OM_uint32, int, gss_OID, OM_uint32 FAR *, gss_buffer_t );
typedef GSS_DLLIMP OM_uint32 (KRB5_CALLCONV * MY_GSS_INDICATE_MECHS)      (OM_uint32 FAR *, gss_OID_set FAR * );
typedef GSS_DLLIMP OM_uint32 (KRB5_CALLCONV * MY_GSS_OID_TO_STR)          (OM_uint32 FAR *, gss_OID, gss_buffer_t );
typedef GSS_DLLIMP OM_uint32 (KRB5_CALLCONV * MY_GSS_RELEASE_OID_SET)     (OM_uint32 FAR *, gss_OID_set FAR * );

class GssSession {

public:

    // Exceptions
    class GssException {
    private:
        _TCHAR errmsg[1024];
        _TCHAR statusmsg[1024];
        BOOL  statusmsginit;
        _TCHAR bothmsgs[2049];
    protected:
        OM_uint32 major_status;
        OM_uint32 minor_status;
        void clear() {_tcscpy(errmsg,__T(""));}
        void add(LPTSTR toadd) {if (!toadd) return; _tcsncat(errmsg,toadd,1023-_tcslen(errmsg)); errmsg[1023]=0;}
    public:
        GssException(LPTSTR szMsg = __T("An exception has occurred in GssSession.")) :
          major_status(0), minor_status(0), statusmsginit(FALSE)
          { clear(); add(szMsg);}

        LPTSTR error_message() {return (LPTSTR)errmsg;} // Error message
        LPTSTR status_message();  // Error message representation of major_status/minor_status pair
        LPTSTR message(); // Return both error_message() and status_message()

    };

    class GssNoLibrary : public GssException {
    public:
        GssNoLibrary() : GssException(__T("The library GSSAPI32.DLL could not be found."))
        {}
    };

    class GssBadFunction : public GssException {
    public:
        GssBadFunction(LPTSTR szFunc = __T("??")) : GssException(__T("Function "))
        {add(szFunc); add(__T(" unavailable from GSSAPI32.DLL."));}
    };

    class GssBadObject : public GssException {
    public:
        GssBadObject(LPTSTR szObject = __T("??")) : GssException(__T("Object "))
        {add(szObject); add(__T(" unavailable from GSSAPI32.DLL."));}
    };

    class GssNonzeroStatus : public GssException {
    public:
        GssNonzeroStatus(OM_uint32 maj=0, OM_uint32 min=0, LPTSTR szMsg=NULL) : GssException()
        {major_status = maj; minor_status = min; if (szMsg) {clear(); add(szMsg);}}
    };

    class GssBadSocket : public GssException {
    public:
        GssBadSocket() : GssException(__T("Could not output to the socket."))
        {}
    };

private:
    HINSTANCE hinstLib; // GSSAPI32.DLL library module
    // objects and function pointers into the library module
    gss_OID* PMY_GSS_C_NT_HOSTBASED_SERVICE;
    gss_OID MY_GSS_C_NT_HOSTBASED_SERVICE;
    MY_GSS_IMPORT_NAME my_gss_import_name;
    MY_GSS_DISPLAY_NAME my_gss_display_name;
    MY_GSS_RELEASE_BUFFER my_gss_release_buffer;
    MY_GSS_INIT_SEC_CONTEXT my_gss_init_sec_context;
    MY_GSS_RELEASE_NAME my_gss_release_name;
    MY_GSS_UNWRAP my_gss_unwrap;
    MY_GSS_WRAP my_gss_wrap;
    MY_GSS_WRAP_SIZE_LIMIT my_gss_wrap_size_limit;
    MY_GSS_DELETE_SEC_CONTEXT my_gss_delete_sec_context;
    MY_GSS_DISPLAY_STATUS my_gss_display_status;
    MY_GSS_INDICATE_MECHS my_gss_indicate_mechs;
    MY_GSS_OID_TO_STR my_gss_oid_to_str;
    MY_GSS_RELEASE_OID_SET my_gss_release_oid_set;
    // how to use GetProcAddress without caring about the function pointer type
    BOOL LoadFunction( void * &f, LPCSTR name) { return (f = (void *) GetProcAddress(hinstLib,name))!=0; }

    // More internal configuration/state information
    protLev protection_level;
    BOOL mutual_authentication;
    unsigned long max_prewrapped; // how long a message can be to be encyrpted into a single token
    gss_ctx_id_t context; // security context gained during Authenticate()
    enum {Nothing, LoadedLibrary, Initialized, GettingContext, Authenticated, ReadyToCommunicate} state;

    int extract_server_retval(LPTSTR str); // extract the 3-digit SMTP return code from the server's response

    // unwind and cleanup
    void Cleanup();

public:

    GssSession();
    ~GssSession();
    LPTSTR StatusText(OM_uint32 major_status, OM_uint32 minor_status, LPTSTR szBuffer = NULL);
    LPTSTR MechtypeText(LPTSTR szBuffer = NULL);
    void Authenticate(struct _COMMON_DATA & CommonData, BOOL (*getline) (struct _COMMON_DATA &, LPTSTR), BOOL (*putline) (struct _COMMON_DATA &, LPTSTR),LPTSTR username, LPTSTR servicename, BOOL mutual, protLev lev);
    protLev GetProtectionLevel() const {return protection_level;};

    BOOL IsReadyToEncrypt() const {return ((state==ReadyToCommunicate) && (protection_level != GSSAUTH_P_NONE));};

    Buf Encrypt(struct _COMMON_DATA & CommonData, Buf& msg);
    Buf Decrypt(Buf& msg);


};

#endif
