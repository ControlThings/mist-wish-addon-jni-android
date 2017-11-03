//
// Created by jan on 11/3/16.
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "mist_app.h"
#include "mist_model.h"
#include "mist_follow.h"

/*
To re-create JNI interface:

 To Renew header file using javah
    javah -classpath ../../../../MistNodeApi/build/intermediates/classes/debug/:/home/jan/Android/Sdk/platforms/android-16/android.jar -o mist_node_api_jni.h mist.node.MistNode

 To see a Java object's method signatures, use:
    javap -s -classpath ../../../../MistNodeApi/build/intermediates/classes/debug/:/home/jan/Android/Sdk/platforms/android-16/android.jar mist.node.MistNode

*/
#include "mist_node_api_jni.h"
#include "jni_utils.h"
#include "mist_node_api_helper.h"
#include "wish_fs.h"

#include "utlist.h"
#include "filesystem.h"
#include "bson_visit.h"


static JavaVM *javaVM;
static mist_model *model;
static wish_app_t *app;
static jobject mistNodeInstance;

struct endpoint_data {
    mist_ep *ep;
    char *epid; /* The "fullpath id" */
    jobject endpoint_object;
    struct endpoint_data *next;
};

static struct endpoint_data *endpoint_head = NULL;

static struct endpoint_data *lookup_ep_data_by_epid(char *epid) {
    struct endpoint_data *ep_data = NULL;
    LL_FOREACH(endpoint_head, ep_data) {
        if (strcmp(ep_data->epid, epid) == 0) {
            return ep_data;
        }
    }
    return NULL;
}

static enum mist_error hw_read(mist_ep *ep,  wish_protocol_peer_t* peer, int request_id) {
    enum mist_error retval = MIST_ERROR;
    android_wish_printf("in hw_read, ep %s", ep->id);

    JNIEnv * env = NULL;
    bool did_attach = false;

    if (javaVM == NULL) {
        android_wish_printf("in hw_read, javaVM is NULL!");
        return MIST_ERROR;
    }

    if (getJNIEnv(javaVM, &env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return MIST_ERROR;
    }

    /* Invoke method "MistNode.read", signature (Lmist/node/Endpoint;[BI)V */
    if (mistNodeInstance == NULL) {
        android_wish_printf("in hw_read, ep %s: mistNodeApiInstance is NULL!");
        return MIST_ERROR;
    }

    jclass mistNodeClass = (*env)->GetObjectClass(env, mistNodeInstance);
    jmethodID readMethodId = (*env)->GetMethodID(env, mistNodeClass, "read", "(Lmist/node/Endpoint;[BI)V");
    if (readMethodId == NULL) {
        android_wish_printf("Cannot get read method");
        return MIST_ERROR;
    }

    char epid[MIST_EPID_LEN] = { 0 };
    mist_ep_full_epid(ep, epid);
    struct endpoint_data *ep_data = lookup_ep_data_by_epid(epid);
    if (ep_data == NULL) {
        android_wish_printf("in hw_read, could not find ep_data for %s!", epid);
        return MIST_ERROR;
    }


    bson bs;
    bson_init(&bs);

    /* Serialise peer to BSON */
    if (peer != NULL) {
        bson_append_peer(&bs, NULL, peer);
    }
    bson_finish(&bs);

    jbyteArray java_peer = (*env)->NewByteArray(env, bson_size(&bs));
    if (java_peer == NULL) {
        android_wish_printf("Failed creating peer byte array");
        return MIST_ERROR;
    }
    if (bson_size(&bs) > 0 ) {
        (*env)->SetByteArrayRegion(env, java_peer, 0, bson_size(&bs), (const jbyte *) bson_data(&bs));
    }

    (*env)->CallVoidMethod(env, mistNodeInstance, readMethodId, ep_data->endpoint_object, java_peer, request_id);
    (*env)->DeleteLocalRef(env, java_peer);
    bson_destroy(&bs);

    if (did_attach) {
        detachThread(javaVM);
    }

    retval = MIST_NO_ERROR;

    check_and_report_exception(env);

    return retval;
}

static enum mist_error hw_write(mist_ep *ep, wish_protocol_peer_t* peer, int request_id, bson* args) {
    enum mist_error retval = MIST_ERROR;
    android_wish_printf("in hw_write");


    JNIEnv *env = NULL;
    bool did_attach = false;

    if (javaVM == NULL) {
        android_wish_printf("hw_write, javaVM is NULL!");
        return MIST_ERROR;
    }

    if (getJNIEnv(javaVM, &env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return MIST_ERROR;
    }

    /* Invoke method "write", signature (Lmist/node/Endpoint;[BI[B)V */
    jmethodID write_method_id = get_methodID(env, mistNodeInstance, "write", "(Lmist/node/Endpoint;[BI[B)V");
    if (write_method_id == NULL) {
        android_wish_printf("hw_write, Cannot find write method!");
        return MIST_ERROR;
    }

    char epid[MIST_EPID_LEN] = { 0 };
    mist_ep_full_epid(ep, epid);
    struct endpoint_data *ep_data = lookup_ep_data_by_epid(epid);
    if (ep_data == NULL) {
        android_wish_printf("in hw_read, could not find ep_data for %s!", epid);
        return MIST_ERROR;
    }

    bson bs;
    bson_init(&bs);
    /* Serialise peer to BSON */
    if (peer != NULL) {
        bson_append_peer(&bs, NULL, peer);
    }
    bson_finish(&bs);

    jbyteArray java_peer = (*env)->NewByteArray(env, bson_size(&bs));
    if (java_peer == NULL) {
        android_wish_printf("Failed creating peer byte array");
        return MIST_ERROR;
    }
    if (bson_size(&bs) > 0) {
        (*env)->SetByteArrayRegion(env, java_peer, 0, bson_size(&bs), (const jbyte *) bson_data(&bs));
    }

    jbyteArray java_args = NULL;
    if (args != NULL) {
        java_args = (*env)->NewByteArray(env, bson_size(args));
    }
    else {
        java_args = (*env)->NewByteArray(env, 0);
    }

    if (java_args == NULL) {
        android_wish_printf("Failed creating args byte array");
        return MIST_ERROR;
    }

    if (args != NULL) {
        (*env)->SetByteArrayRegion(env, java_args, 0, bson_size(args), (const jbyte *) bson_data(args));
    }

    (*env)->CallVoidMethod(env, mistNodeInstance, write_method_id, ep_data->endpoint_object, java_peer, request_id, java_args);
    (*env)->DeleteLocalRef(env, java_peer);
    (*env)->DeleteLocalRef(env, java_args);

    if (did_attach) {
        detachThread(javaVM);
    }
    bson_destroy(&bs);

    check_and_report_exception(env);

    retval = MIST_NO_ERROR;

    return retval;
}

static enum mist_error hw_invoke(mist_ep *ep, wish_protocol_peer_t* peer, int request_id, bson* args) {
    enum mist_error retval = MIST_ERROR;
    android_wish_printf("in hw_invoke");

    JNIEnv *env = NULL;
    bool did_attach = false;

    if (javaVM == NULL) {
        android_wish_printf("hw_write, javaVM is NULL!");
        return MIST_ERROR;
    }

    if (getJNIEnv(javaVM, &env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return MIST_ERROR;
    }

    /* Invoke method "invoke", signature (Lmist/node/Endpoint;[BI[B)V */
    jmethodID invoke_method_id = get_methodID(env, mistNodeInstance, "invoke", "(Lmist/node/Endpoint;[BI[B)V");
    if (invoke_method_id == NULL) {
        android_wish_printf("hw_invoke, Cannot find invoke method!");
        return MIST_ERROR;
    }

    char epid[MIST_EPID_LEN] = { 0 };
    mist_ep_full_epid(ep, epid);
    struct endpoint_data *ep_data = lookup_ep_data_by_epid(epid);
    if (ep_data == NULL) {
        android_wish_printf("in hw_invoke, could not find ep_data for %s!", epid);
        return MIST_ERROR;
    }

    bson bs;
    bson_init(&bs);
    /* Serialise peer to BSON */
    if (peer != NULL) {
        bson_append_peer(&bs, NULL, peer);
    }
    bson_finish(&bs);

    jbyteArray java_peer = (*env)->NewByteArray(env, bson_size(&bs));
    if (java_peer == NULL) {
        android_wish_printf("Failed creating peer byte array");
        return MIST_ERROR;
    }
    if (bson_size(&bs) > 0) {
        (*env)->SetByteArrayRegion(env, java_peer, 0, bson_size(&bs), (const jbyte *) bson_data(&bs));
    }

    jbyteArray java_args = NULL;
    if (args != NULL) {
        java_args = (*env)->NewByteArray(env, bson_size(args));
    }
    else {
        java_args = (*env)->NewByteArray(env, 0);
    }

    if (java_args == NULL) {
        android_wish_printf("Failed creating args byte array");
        return MIST_ERROR;
    }

    if (args != NULL) {
        (*env)->SetByteArrayRegion(env, java_args, 0, bson_size(args), (const jbyte *) bson_data(args));
    }

    (*env)->CallVoidMethod(env, mistNodeInstance, invoke_method_id, ep_data->endpoint_object, java_peer, request_id, java_args);
    (*env)->DeleteLocalRef(env, java_peer);
    (*env)->DeleteLocalRef(env, java_args);

    if (did_attach) {
        detachThread(javaVM);
    }
    bson_destroy(&bs);

    check_and_report_exception(env);


    retval = MIST_NO_ERROR;
    return retval;
}


/*
 * Class:     mist_node_MistNode
 * Method:    startMistApp
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_mistNodeApi_node_MistNode_startMistApp(JNIEnv *env, jobject java_this, jstring java_appName) {

    android_wish_printf("in startMistApp");
    /* Register a refence to the JVM */
    if ((*env)->GetJavaVM(env,&javaVM) < 0) {
        android_wish_printf("Failed to GetJavaVM");
        return;
    }

    /* Register the platform dependencies with Wish/Mist */

    wish_platform_set_malloc(malloc);
    wish_platform_set_realloc(realloc);
    wish_platform_set_free(free);
    srandom(time(NULL));
    wish_platform_set_rng(random);
    wish_platform_set_vprintf(android_wish_vprintf);
    wish_platform_set_vsprintf(vsprintf);

    /* File system functions are needed for Mist mappings! */
    wish_fs_set_open(my_fs_open);
    wish_fs_set_read(my_fs_read);
    wish_fs_set_write(my_fs_write);
    wish_fs_set_lseek(my_fs_lseek);
    wish_fs_set_close(my_fs_close);
    wish_fs_set_rename(my_fs_rename);
    wish_fs_set_remove(my_fs_remove);

    mistNodeInstance = (*env)->NewGlobalRef(env, java_this);

    mist_app_t *mist_app = start_mist_app();
    if (mist_app == NULL) {
        WISHDEBUG(LOG_CRITICAL, "Failed creating mist app!");
    }

    model = &mist_app->model;
    char *name_str =  (char*) (*env)->GetStringUTFChars(env, java_appName, NULL);
    app = wish_app_create(name_str);
    if (app == NULL) {
        WISHDEBUG(LOG_CRITICAL, "Failed creating wish app!");
    }
    wish_app_add_protocol(app, &mist_app->protocol);
    mist_app->app = app;
    (*env)->ReleaseStringUTFChars(env, java_appName, name_str);

    /* The app will login to core when the Bridge connects, this happens via the wish_app_connected(wish_app_t *app, bool connected) function */
}

/*
 * Class:     mist_node_MistNode
 * Method:    stopMistApp
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_mistNodeApi_node_MistNode_stopMistApp(JNIEnv *env, jobject java_this) {
    android_wish_printf("stopMistApp not implemented!");
}


/*
 * Class:     mist_node_MistNode
 * Method:    addEndpoint
 * Signature: (Lmist/node/Endpoint;)V
 */
JNIEXPORT void JNICALL Java_mistNodeApi_node_MistNode_addEndpoint(JNIEnv *env, jobject java_this, jobject java_Endpoint) {
    android_wish_printf("in addEndpoint");

    /* Get the class corresponding to java_Endpoint instance */
    jclass endpointClass = (*env)->GetObjectClass(env, java_Endpoint);
    if (endpointClass == NULL) {
        android_wish_printf("addEndpoint: Could not resolve endpoint class.");
        return;
    }

    char *epid_str = get_string_from_obj_field(env, java_Endpoint, "epid");

    if (epid_str == NULL) {
        android_wish_printf("addEndpoint: epid is null!");
        return;
    }

    /* Get "label" field, a String */

    char *label_str = get_string_from_obj_field(env, java_Endpoint, "label");

    /* Get "type" field, an int */
    jfieldID typeField = (*env)->GetFieldID(env, endpointClass, "type", "I");
    jint type = (*env)->GetIntField(env, java_Endpoint, typeField);

    /* Get "unit" field, a String */

    char* unit_str = get_string_from_obj_field(env, java_Endpoint, "unit");

    char *id_str = get_string_from_obj_field(env, java_Endpoint, "id");
    char *parent_epid_str = get_string_from_obj_field(env, java_Endpoint, "parent");

    android_wish_printf("addEndpoint, epid: %s, id: %s, label: %s, parent full path: %s, type: %i, unit: %s", epid_str, id_str, label_str, parent_epid_str, type, unit_str);

    struct endpoint_data *ep_data = wish_platform_malloc(sizeof (struct endpoint_data));
    if (ep_data == NULL) {
        android_wish_printf("Could allocate memory for endpoint callbacks");
        return;
    }
    memset(ep_data, 0, sizeof (struct endpoint_data));

    ep_data->endpoint_object = (*env)->NewGlobalRef(env, java_Endpoint);
    ep_data->epid = epid_str;


    /* Get readable boolean */
    jfieldID readableField = (*env)->GetFieldID(env, endpointClass, "readable", "Z");
    jboolean readable = (*env)->GetBooleanField(env, java_Endpoint, readableField);

    jfieldID writableField = (*env)->GetFieldID(env, endpointClass, "writable", "Z");
    jboolean writable = (*env)->GetBooleanField(env, java_Endpoint, writableField);

    jfieldID invokableField = (*env)->GetFieldID(env, endpointClass, "invokable", "Z");
    jboolean invokable = (*env)->GetBooleanField(env, java_Endpoint, invokableField);

    /* Actually add the endpoint to mist lib */

    // allocate a new endpoint and space for data
    mist_ep* ep = (mist_ep*) malloc(sizeof(mist_ep));
    if (ep == NULL) {
        android_wish_printf("Out of memory when allocating mist_ep");
        return;
    }
    memset(ep, 0, sizeof(mist_ep));

    ep->id = id_str; // Note: this must be free'ed when removing endpoint!
    ep->label = label_str; // Note: this must be free'ed when removing endpoint!
    ep->type = type;
    ep_data->ep = ep;

    if (readable) { ep->read = hw_read; }
    if (writable) { ep->write = hw_write; }
    if (invokable) { ep->invoke = hw_invoke; }

    ep->unit = "";
    ep->next = NULL;
    ep->prev = NULL;
    ep->dirty = false;
    ep->scaling = "";

    if (MIST_NO_ERROR != mist_ep_add(model, parent_epid_str, ep)) {
        android_wish_printf("addEndpoint returned error");
    }

    LL_APPEND(endpoint_head, ep_data);

    if (parent_epid_str != NULL) {
        free(parent_epid_str);
    }

}

/*
 * Class:     mist_node_MistNode
 * Method:    removeEndpoint
 * Signature: (Lmist/node/Endpoint;)V
 */
JNIEXPORT void JNICALL Java_mistNodeApi_node_MistNode_removeEndpoint(JNIEnv *env, jobject java_this, jobject java_endpoint) {

}

/*
 * Class:     mist_node_MistNode
 * Method:    readResponse
 * Signature: (Ljava/lang/String;I[B)V
 */
JNIEXPORT void JNICALL Java_mistNodeApi_node_MistNode_readResponse(JNIEnv *env, jobject java_this, jstring java_epid, jint request_id, jbyteArray java_data) {
    android_wish_printf("in readResponse");

    if (java_data == NULL) {
        android_wish_printf("in readResponse: java_data is NULL");
        return;
    }

    if (java_epid == NULL) {
        android_wish_printf("in readResponse: java_epid is NULL");
        return;
    }
    size_t data_length = (*env)->GetArrayLength(env, java_data);
    uint8_t *data = (uint8_t *) calloc(data_length, 1);
    if (data == NULL) {
        android_wish_printf("Malloc fails");
        return;
    }
    (*env)->GetByteArrayRegion(env, java_data, 0, data_length, data);

    char *epid_str =  (char*) (*env)->GetStringUTFChars(env, java_epid, NULL);

    bson bs;
    bson_init_with_data(&bs, data);
    mist_read_response(model->mist_app, epid_str, request_id, &bs);

    (*env)->ReleaseStringUTFChars(env, java_epid, epid_str);
    free(data);
}

/*
 * Class:     mist_node_MistNode
 * Method:    readError
 * Signature: (Ljava/lang/String;IILjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_mistNodeApi_node_MistNode_readError(JNIEnv *env, jobject java_this, jstring java_epid, jint request_id, jint code, jstring java_msg) {
    android_wish_printf("in readError");

    char *epid_str =  (char*) (*env)->GetStringUTFChars(env, java_epid, NULL);
    char *msg_str = (char*) (*env)->GetStringUTFChars(env, java_msg, NULL);

    mist_read_error(model->mist_app, epid_str, request_id, code, msg_str);

    (*env)->ReleaseStringUTFChars(env, java_epid, epid_str);
    (*env)->ReleaseStringUTFChars(env, java_msg, msg_str);
}

/*
 * Class:     mist_node_MistNode
 * Method:    writeResponse
 * Signature: (Ljava/lang/String;I)V
 */
JNIEXPORT void JNICALL Java_mistNodeApi_node_MistNode_writeResponse(JNIEnv *env, jobject java_this, jstring java_epid, jint request_id) {
    android_wish_printf("in writeResponse");
    if (java_epid == NULL) {
        android_wish_printf("in writeResponse: java_epid is NULL");
        return;
    }
    char *epid_str =  (char*) (*env)->GetStringUTFChars(env, java_epid, NULL);

    mist_write_response(model->mist_app, epid_str, request_id);

    (*env)->ReleaseStringUTFChars(env, java_epid, epid_str);
}

/*
 * Class:     mist_node_MistNode
 * Method:    writeError
 * Signature: (Ljava/lang/String;IILjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_mistNodeApi_node_MistNode_writeError(JNIEnv *env, jobject java_this, jstring java_epid, jint request_id, jint code, jstring java_msg) {
    android_wish_printf("in writeError");
    if (java_epid == NULL) {
        android_wish_printf("in writeError: java_epid is NULL");
        return;
    }
    char *epid_str =  (char*) (*env)->GetStringUTFChars(env, java_epid, NULL);
    char *msg_str = (char*) (*env)->GetStringUTFChars(env, java_msg, NULL);

    mist_write_error(model->mist_app, epid_str, request_id, code, msg_str);

    (*env)->ReleaseStringUTFChars(env, java_epid, epid_str);
    (*env)->ReleaseStringUTFChars(env, java_msg, msg_str);
}

/*
 * Class:     mist_node_MistNode
 * Method:    invokeResponse
 * Signature: (Ljava/lang/String;I[B)V
 */
JNIEXPORT void JNICALL Java_mistNodeApi_node_MistNode_invokeResponse(JNIEnv *env, jobject java_this, jstring java_epid, jint request_id, jbyteArray java_data) {
    android_wish_printf("in invokeResponse");

    if (java_data == NULL) {
        android_wish_printf("in invokeResponse: java_data is NULL");
        return;
    }

    if (java_epid == NULL) {
        android_wish_printf("in invokeResponse: java_epid is NULL");
        return;
    }
    size_t data_length = (*env)->GetArrayLength(env, java_data);
    uint8_t *data = (uint8_t *) calloc(data_length, 1);
    if (data == NULL) {
        android_wish_printf("Malloc fails");
        return;
    }
    (*env)->GetByteArrayRegion(env, java_data, 0, data_length, data);

    char *epid_str =  (char*) (*env)->GetStringUTFChars(env, java_epid, NULL);

    bson data_bs;
    bson_init_with_data(&data_bs, data);

    bson bs;
    bson_init(&bs);
    bson_append_bson(&bs, "data", &data_bs);
    bson_finish(&bs);
    mist_invoke_response(model->mist_app, epid_str, request_id, &bs);

    (*env)->ReleaseStringUTFChars(env, java_epid, epid_str);
    bson_destroy(&bs);
    free(data);
}

/*
 * Class:     mist_node_MistNode
 * Method:    invokeError
 * Signature: (Ljava/lang/String;IILjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_mistNodeApi_node_MistNode_invokeError(JNIEnv *env, jobject java_this, jstring java_epid, jint request_id, jint code, jstring java_msg) {
    android_wish_printf("in invokeError");
    if (java_epid == NULL) {
        android_wish_printf("in invokeError: java_epid is NULL");
        return;
    }
    char *epid_str =  (char*) (*env)->GetStringUTFChars(env, java_epid, NULL);
    char *msg_str = (char*) (*env)->GetStringUTFChars(env, java_msg, NULL);

    mist_invoke_error(model->mist_app, epid_str, request_id, code, msg_str);

    (*env)->ReleaseStringUTFChars(env, java_epid, epid_str);
    (*env)->ReleaseStringUTFChars(env, java_msg, msg_str);
}

/*
 * Class:     mist_node_MistNode
 * Method:    changed
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_mistNodeApi_node_MistNode_changed(JNIEnv *env, jobject java_this, jstring java_epid) {
    android_wish_printf("in changed");
    if (java_epid == NULL) {
        android_wish_printf("in changed: java_epid is NULL");
        return;
    }
    char *epid_str =  (char*) (*env)->GetStringUTFChars(env, java_epid, NULL);

    mist_value_changed(model->mist_app, epid_str);

    (*env)->ReleaseStringUTFChars(env, java_epid, epid_str);
}




wish_app_t *get_mist_node_app(void) {
    return app;
}

JavaVM *getJavaVM(void) {
    return javaVM;
}

jobject getMistNodeApiInstance() {
    return mistNodeInstance;
}


/*  Enter critical section
    Obtain the monitor (mutex) of the MistNode class instance.
 Returns 0 for a successful gaining of the monitor, or -1 for an error.
 */
static int enter_MistNode_monitor(void) {
    JNIEnv * my_env = NULL;
    bool did_attach = false;
    if (getJNIEnv(javaVM, &my_env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return -1;
    }

    if ((*my_env)->MonitorEnter(my_env, mistNodeInstance) != 0) {
        android_wish_printf("Error while entering monitor");
        return -1;
    }

    if (did_attach) {
        detachThread(javaVM);
    }
    return 0;
}

/*  Exit critical section:
    Release the monitor associated with the MistNode class instance.
    Returns 0 for a successful release of the monitor, or -1 for an error.
 */
static int exit_MistNode_monitor(void) {
    JNIEnv * my_env = NULL;
    bool did_attach = false;
    if (getJNIEnv(javaVM, &my_env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return -1;
    }

    if ((*my_env)->MonitorExit(my_env, mistNodeInstance) != 0) {
        android_wish_printf("Error while exiting monitor");
        return -1;
    }

    if (did_attach) {
        detachThread(javaVM);
    }
    return 0;
}

/** Linked list element of a RPC requests sent by us. It is used to keep track of callback objects and RPC ids. */
struct callback_list_elem {
    /** The RPC id associated with the callback entry */
    int request_id;
    /** The Java object (global) reference which will contain the callback methods: ack, sig, err... Remember to delete the global reference when callback is no longer needed! */
    jobject cb_obj;
    /** The next item in the list */
    struct callback_list_elem *next;
};

/** The linked list head of Mist RPC requests sent by us */
struct callback_list_elem *mist_cb_list_head = NULL;

/** The linked list head of Wish RPC requests sent by us */
struct callback_list_elem *wish_cb_list_head = NULL;



/**
 * This call-back method is invoked both for Mist and Wish RPC requests.  It will call methods of the callback object associated with the request id
 */
static void generic_callback(rpc_client_req *req, void *ctx, const uint8_t *payload, size_t payload_len) {
    //WISHDEBUG(LOG_CRITICAL, "Callback invoked!");
    bson_visit("generic_callback invoked!", payload);

    /* First decide if this callback is a Mist or Wish callback - this determines which callback list we will examine */
    struct callback_list_elem **cb_list_head = NULL;
    if (req->client == &(model->mist_app->protocol.rpc_client)) {
        //android_wish_printf("Response to Mist request");
        cb_list_head = &mist_cb_list_head;
    }
    else if (req->client == &(app->rpc_client)) {
        //android_wish_printf("Response to Wish request");
        cb_list_head = &wish_cb_list_head;
    }
    else {
        android_wish_printf("Could not determine the kind of request callback for rpc id: %i", req->id);
        return;
    }

    bool did_attach = false;
    JNIEnv * my_env = NULL;
    if (getJNIEnv(javaVM, &my_env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return;
    }

    int req_id = 0;

    bool is_ack = false;
    bool is_sig = false;
    bool is_err = false;

    bson_iterator it;
    bson_find_from_buffer(&it, payload, "ack");
    if (bson_iterator_type(&it) == BSON_INT) {
        req_id = bson_iterator_int(&it);
        if (req_id != 0) {
            is_ack = true;
        }
        WISHDEBUG(LOG_CRITICAL, "Request ack %i", req_id);
    }

    bson_find_from_buffer(&it, payload, "sig");
    if (bson_iterator_type(&it) == BSON_INT) {
        req_id = bson_iterator_int(&it);
        if (req_id != 0) {
            is_sig = true;
        }
        WISHDEBUG(LOG_CRITICAL, "Request sig %i", req_id);
    }

    bson_find_from_buffer(&it, payload, "err");
    if (bson_iterator_type(&it) == BSON_INT) {
        req_id = bson_iterator_int(&it);
        if (req_id != 0) {
            is_err = true;
        }
        WISHDEBUG(LOG_CRITICAL, "Request err %i", req_id);
    }

    if (!is_ack && !is_sig && !is_err) {
        WISHDEBUG(LOG_CRITICAL, "Not ack, sig or err!");
        return;
    }

    jobject cb_obj = NULL;
    struct callback_list_elem *elem = NULL;
    struct callback_list_elem *tmp = NULL;

    /* Enter Critical section to protect the cb lists from concurrent modification. Note that adding to the cb lists is protected by the same monitor, thanks to JNI interface being declared 'synchronous'. */
    if (enter_MistNode_monitor() == 0) {
        LL_FOREACH_SAFE(*cb_list_head, elem, tmp) {
            if (elem->request_id == req_id) {
                cb_obj = elem->cb_obj;
                if (is_ack || is_err) {
                    WISHDEBUG(LOG_CRITICAL, "Removing cb_list element for id %i", req_id);
                    LL_DELETE(*cb_list_head, elem);
                    free(elem);
                }
            }
        }
    }
    else {
        WISHDEBUG(LOG_CRITICAL, "There was a critical error while trying to enter critical section");
        return;
    }

    /* Exit critical section */
    if (exit_MistNode_monitor() != 0) {
        WISHDEBUG(LOG_CRITICAL, "There was a critical error while trying to exit critical section");
        return;
    }


    if (cb_obj == NULL) {
        WISHDEBUG(LOG_CRITICAL, "Could not find the request from cb_list");
        return;
    }

    /* cb_obj now has the call-up object reference */
    jclass cbClass = (*my_env)->GetObjectClass(my_env, cb_obj);
    if (cbClass == NULL) {
        WISHDEBUG(LOG_CRITICAL, "Cannot get Mist API callback class");
        return;
    }

    jbyteArray java_data = NULL;
    if (is_ack || is_sig) {
        bson bs;
        bson_init(&bs);

        /* Find the data element from payload */
        bson_find_from_buffer(&it, payload, "data");

        bson_type type = bson_iterator_type(&it);

        switch(type) {
        case BSON_BOOL:
            bson_append_bool(&bs, "data", bson_iterator_bool(&it));
            break;
        case BSON_INT:
            bson_append_int(&bs, "data", bson_iterator_int(&it));
            break;
        case BSON_DOUBLE:
            bson_append_double(&bs, "data", bson_iterator_double(&it));
            break;
        case BSON_STRING:
        case BSON_OBJECT:
        case BSON_ARRAY:
        case BSON_BINDATA:
            bson_append_element(&bs, "data", &it);
            break;
        case BSON_EOO:
            WISHDEBUG(LOG_CRITICAL, "Unexpected end of BSON, no data");
            break;
        default:
            WISHDEBUG(LOG_CRITICAL, "Unsupported bson type %i of data element", type);
        }
        bson_finish(&bs);

        uint8_t *data_doc = (uint8_t *) bson_data(&bs);
        size_t data_doc_len = bson_size(&bs);

        java_data = (*my_env)->NewByteArray(my_env, data_doc_len);
        if (java_data == NULL) {
            WISHDEBUG(LOG_CRITICAL, "Failed creating java buffer for ack or sig data");
            return;
        }
        (*my_env)->SetByteArrayRegion(my_env, java_data, 0, data_doc_len, (const jbyte *) data_doc);
    }

    if (is_ack) {
        /* Invoke "ack" method of MistNode.RequestCb
            public void ack(byte[]);
              descriptor: ([B)V */
        jmethodID ackMethodId = (*my_env)->GetMethodID(my_env, cbClass, "ack", "([B)V");
        if (ackMethodId == NULL) {
            WISHDEBUG(LOG_CRITICAL, "Cannot get ack method");
            return;
        }
        (*my_env)->CallVoidMethod(my_env, cb_obj, ackMethodId, java_data);
    } else if (is_sig) {
        /* Invoke "sig" method of MistNode.RequestCb:
          public void sig(byte[]);
            descriptor: ([B)V
        */
        jmethodID sigMethodId = (*my_env)->GetMethodID(my_env, cbClass, "sig", "([B)V");
        if (sigMethodId == NULL) {
            WISHDEBUG(LOG_CRITICAL, "Cannot get sig method");
            return;
        }
        (*my_env)->CallVoidMethod(my_env, cb_obj, sigMethodId, java_data);

    } else if (is_err) {
        int code = 0;
        jobject java_msg = NULL;
        const char *msg = NULL;

        /* The response was an err. Get the code and msg fields from BSON */
        if (bson_find_from_buffer(&it, payload, "data") == BSON_OBJECT) {
            bson_iterator sit;
            bson bs;
            bson_init_with_data(&bs, payload);

            bson_iterator_subiterator(&it, &sit);
            if (bson_find_fieldpath_value("code", &sit) == BSON_INT) {
                code = bson_iterator_int(&sit);
            }
            bson_iterator_subiterator(&it, &sit);
            if (bson_find_fieldpath_value("msg", &sit) == BSON_STRING) {
                WISHDEBUG(LOG_CRITICAL, "msg is STRING");
                msg = bson_iterator_string(&sit);
            }
        }
        else {
            bson_find_from_buffer(&it, payload, "code");

            if (bson_iterator_type(&it) == BSON_INT) {
                code = bson_iterator_int(&it);
            } else {
                WISHDEBUG(LOG_CRITICAL, "err code is not of int type!");
            }


            bson_find_from_buffer(&it, payload, "msg");
            if (bson_iterator_type(&it) == BSON_STRING) {
                msg = bson_iterator_string(&it);
            }
        }

        if (msg != NULL) {
            /* Create a Java string from the msg */
            java_msg = (*my_env)->NewStringUTF(my_env, msg);
            if (java_msg == NULL) {
                WISHDEBUG(LOG_CRITICAL, "Failed to create err msg Java String!");
            }
        }

        WISHDEBUG(LOG_CRITICAL, "Request err code %i msg %s", code, msg);
        /* Invoke "err" method of MistNode.RequestCb:
           public abstract void err(int, java.lang.String);
             descriptor: (ILjava/lang/String;)V
         */
        jmethodID errMethodId = (*my_env)->GetMethodID(my_env, cbClass, "err", "(ILjava/lang/String;)V");
        if (errMethodId == NULL) {
            WISHDEBUG(LOG_CRITICAL, "Cannot get sig method");
            return;
        }
        (*my_env)->CallVoidMethod(my_env, cb_obj, errMethodId, code, java_msg);
        if (java_msg != NULL) {
            (*my_env)->DeleteLocalRef(my_env, java_msg);
        }
    }

    /* Check if the any of the methods calls above threw an exception */
    check_and_report_exception(my_env);

    if (java_data != NULL) {
        (*my_env)->DeleteLocalRef(my_env, java_data);
    }

    if (is_ack || is_err) {
        (*my_env)->DeleteGlobalRef(my_env, cb_obj);
    }

    if (did_attach) {
        detachThread(javaVM);
    }
}

/** Build the complete RPC request BSON from the Java arguments, and save the callback object. At this point it does not matter if we are making a Mist or Wish request */
static bool save_request(JNIEnv *env, struct callback_list_elem **cb_list_head, int request_id, jobject java_callback) {
    android_wish_printf("in save_request, req id %i", request_id);
    /* Create Linked list entry */
    struct callback_list_elem *elem = calloc(sizeof (struct callback_list_elem), 1);
    if (elem == NULL) {
        WISHDEBUG(LOG_CRITICAL, "Cannot allocate memory");
        return false;
    }
    WISHDEBUG(LOG_CRITICAL, "Registering RPC id %i", request_id);
    /* Create a global reference of the callback object and save the object in the linked list of requests */
    elem->cb_obj = (*env)->NewGlobalRef(env, java_callback);
    elem->request_id = request_id;
    /* Note: No need to enter critical section here, because we are already owner of the RawApi monitor, because we have
    entered via a "synchronized" native method. */
    LL_APPEND(*cb_list_head, elem);
    //android_wish_printf("queue %p; %p %p", cb_list_head, mist_cb_list_head, wish_cb_list_head);
    return true;
}

/*
 * Class:     mist_node_MistNode
 * Method:    request
 * Signature: ([B[BLmist/node/MistNode/RequestCb;)I
 */
JNIEXPORT jint JNICALL Java_mistNodeApi_node_MistNode_request(JNIEnv *env, jobject java_this, jbyteArray java_peer, jbyteArray java_req, jobject callback_obj) {
    android_wish_printf("in request");
    /* Unmarshall the java_peer and java_req to normal arrays of bytes */
    size_t peer_bson_len = (*env)->GetArrayLength(env, java_peer);
    uint8_t *peer_bson = (uint8_t *) calloc(peer_bson_len, 1);
    if (peer_bson == NULL) {
        android_wish_printf("Malloc fails (peer)");
        return 0;
    }
    (*env)->GetByteArrayRegion(env, java_peer, 0, peer_bson_len, peer_bson);

    size_t req_bson_len = (*env)->GetArrayLength(env, java_req);
    uint8_t *req_bson = (uint8_t *) calloc(req_bson_len, 1);
    if (req_bson == NULL) {
        android_wish_printf("Malloc fails (req)");
        return 0;
    }
    (*env)->GetByteArrayRegion(env, java_req, 0, req_bson_len, req_bson);


    /* Resolve the BSON peer to a wish_protocol_peer: wish_protocol_peer_find_from_bson */
    wish_protocol_peer_t *peer = wish_protocol_peer_find_from_bson(&(model->mist_app->protocol), peer_bson);

    if (peer == NULL) {
        android_wish_printf("Could not find peer for request!");
        return 0;
    }

    bson req_bs;
    bson_init_with_data(&req_bs, req_bson);
    /* Send down the request, with our generic_cb callback, save the request id. */
    rpc_client_req *req = mist_app_request(model->mist_app, peer, &req_bs, generic_callback);

    int request_id = req->id;
    /* Save the mapping request_id -> callback object */
    save_request(env, &mist_cb_list_head, request_id, callback_obj);

    free(peer_bson);
    free(req_bson);
    android_wish_printf("in request (exit)");
    return request_id;
}

/*
 * Class:     mist_node_MistNode
 * Method:    wishRequest
 * Signature: ([BLmist/node/MistNode/RequestCb;)I
 */
JNIEXPORT jint JNICALL Java_mistNodeApi_node_MistNode_wishRequest(JNIEnv *env, jobject java_this, jbyteArray java_req, jobject callback_obj) {
    size_t req_bson_len = (*env)->GetArrayLength(env, java_req);
    uint8_t *req_bson = (uint8_t *) calloc(req_bson_len, 1);
    if (req_bson == NULL) {
        android_wish_printf("Malloc fails (req)");
        return 0;
    }
    (*env)->GetByteArrayRegion(env, java_req, 0, req_bson_len, req_bson);

    bson req_bs;
    bson_init_with_data(&req_bs, req_bson);
    rpc_client_req *req = wish_app_request(app, &req_bs, generic_callback, NULL);

    int request_id = req->id;
    save_request(env, &wish_cb_list_head, request_id, callback_obj);

    free(req_bson);

    return request_id;
}
