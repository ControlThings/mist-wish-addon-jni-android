/**
 * Copyright (C) 2020, ControlThings Oy Ab
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * @license Apache-2.0
 */
//
// Created by jan on 11/3/16.
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "mist_app.h"
#include "mist_model.h"
#include "mist_follow.h"

#include "addon.h"
#include "concurrency.h"
#include "wish_fs.h"
#include "filesystem.h"

/*
To re-create JNI interface:

 To Renew header file using javah

    javah -classpath ../../../../MistNode/build/intermediates/classes/debug/:/home/jan/Android/Sdk/platforms/android-16/android.jar -o mist_node_jni.h mist.node.MistNode

    ...and...

    javah -classpath ../../../../MistApi/build/intermediates/classes/debug/:/home/jan/Android/Sdk/platforms/android-16/android.jar -o wish_app_jni.h wish.WishApp

 To see a Java object's method signatures, use:
    javap -s -classpath ../../../../MistNode/build/intermediates/classes/debug/:/home/jan/Android/Sdk/platforms/android-16/android.jar mist.node.MistNode

*/
#include "mist_node_jni.h"
#include "wish_app_jni.h"
#include "jni_utils.h"
#include "mist_node_api_helper.h"


#include "utlist.h"
#include "bson_visit.h"

static jobject mistNodeInstance;

void setMistNodeInstance(jobject global_ref) {
    mistNodeInstance = global_ref;
}

jobject getMistNodeApiInstance() {
    return mistNodeInstance;
}

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

    JavaVM *javaVM = addon_get_java_vm();

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

    JavaVM *javaVM = addon_get_java_vm();

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

    JavaVM *javaVM = addon_get_java_vm();

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
JNIEXPORT jint JNICALL Java_mist_node_MistNode_startMistApp(JNIEnv *env, jobject java_this, jstring java_appName, jobject java_wish_file) {
    android_wish_printf("in startMistApp");

    if (addon_is_started()) {
        return mist_node_MistNode_MIST_NODE_ERROR_MULTIPLE_TIMES;
    }

    /* Register a refence to the JVM */
    JavaVM *javaVM;
    if ((*env)->GetJavaVM(env,&javaVM) < 0) {
        android_wish_printf("Failed to GetJavaVM");
        return mist_node_MistNode_MIST_NODE_ERROR_UNSPECIFIED;
    }
    addon_set_java_vm(javaVM);

    mistNodeInstance = (*env)->NewGlobalRef(env, java_this);
    monitor_init(javaVM, mistNodeInstance);

    jobject wish_file_global_ref = (*env)->NewGlobalRef(env, java_wish_file);
    wish_file_init(wish_file_global_ref);

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return mist_node_MistNode_MIST_NODE_ERROR_UNSPECIFIED;
    }
    char *name_str =  (char*) (*env)->GetStringUTFChars(env, java_appName, NULL);
    addon_init_mist_app(name_str);
    (*env)->ReleaseStringUTFChars(env, java_appName, name_str);
    monitor_exit();

    /* The app will login to core when the Bridge connects, this happens via the wish_app_connected(wish_app_t *app, bool connected) function */

    return mist_node_MistNode_MIST_NODE_SUCCESS;
}

/*
 * Class:     mist_node_MistNode
 * Method:    stopMistApp
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_mist_node_MistNode_stopMistApp(JNIEnv *env, jobject java_this) {
    android_wish_printf("stopMistApp not implemented!");
}


/*
 * Class:     mist_node_MistNode
 * Method:    addEndpoint
 * Signature: (Lmist/node/Endpoint;)V
 */
JNIEXPORT void JNICALL Java_mist_node_MistNode_addEndpoint(JNIEnv *env, jobject java_this, jobject java_Endpoint) {
    android_wish_printf("in addEndpoint");

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return;
    }

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

    if (MIST_NO_ERROR != mist_ep_add(get_mist_model(), parent_epid_str, ep)) {
        android_wish_printf("addEndpoint returned error");
    }

    LL_APPEND(endpoint_head, ep_data);

    if (parent_epid_str != NULL) {
        free(parent_epid_str);
    }

    monitor_exit();
}

/*
 * Class:     mist_node_MistNode
 * Method:    removeEndpoint
 * Signature: (Lmist/node/Endpoint;)V
 */
JNIEXPORT void JNICALL Java_mist_node_MistNode_removeEndpoint(JNIEnv *env, jobject java_this, jobject java_endpoint) {
    android_wish_printf("removeEndpoint is unimplemented!");
}

/*
 * Class:     mist_node_MistNode
 * Method:    readResponse
 * Signature: (Ljava/lang/String;I[B)V
 */
JNIEXPORT void JNICALL Java_mist_node_MistNode_readResponse(JNIEnv *env, jobject java_this, jstring java_epid, jint request_id, jbyteArray java_data) {
    android_wish_printf("in readResponse");

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return;
    }

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
    mist_read_response(get_mist_model()->mist_app, epid_str, request_id, &bs);

    (*env)->ReleaseStringUTFChars(env, java_epid, epid_str);
    free(data);

    monitor_exit();
}

/*
 * Class:     mist_node_MistNode
 * Method:    readError
 * Signature: (Ljava/lang/String;IILjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_mist_node_MistNode_readError(JNIEnv *env, jobject java_this, jstring java_epid, jint request_id, jint code, jstring java_msg) {
    android_wish_printf("in readError");

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return;
    }

    char *epid_str =  (char*) (*env)->GetStringUTFChars(env, java_epid, NULL);
    char *msg_str = (char*) (*env)->GetStringUTFChars(env, java_msg, NULL);

    mist_read_error(get_mist_model()->mist_app, epid_str, request_id, code, msg_str);

    (*env)->ReleaseStringUTFChars(env, java_epid, epid_str);
    (*env)->ReleaseStringUTFChars(env, java_msg, msg_str);

    monitor_exit();
}

/*
 * Class:     mist_node_MistNode
 * Method:    writeResponse
 * Signature: (Ljava/lang/String;I)V
 */
JNIEXPORT void JNICALL Java_mist_node_MistNode_writeResponse(JNIEnv *env, jobject java_this, jstring java_epid, jint request_id) {
    android_wish_printf("in writeResponse");

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return;
    }

    if (java_epid == NULL) {
        android_wish_printf("in writeResponse: java_epid is NULL");
        return;
    }
    char *epid_str =  (char*) (*env)->GetStringUTFChars(env, java_epid, NULL);

    mist_write_response(get_mist_model()->mist_app, epid_str, request_id);

    (*env)->ReleaseStringUTFChars(env, java_epid, epid_str);

    monitor_exit();
}

/*
 * Class:     mist_node_MistNode
 * Method:    writeError
 * Signature: (Ljava/lang/String;IILjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_mist_node_MistNode_writeError(JNIEnv *env, jobject java_this, jstring java_epid, jint request_id, jint code, jstring java_msg) {
    android_wish_printf("in writeError");

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return;
    }

    if (java_epid == NULL) {
        android_wish_printf("in writeError: java_epid is NULL");
        return;
    }
    char *epid_str =  (char*) (*env)->GetStringUTFChars(env, java_epid, NULL);
    char *msg_str = (char*) (*env)->GetStringUTFChars(env, java_msg, NULL);

    mist_write_error(get_mist_model()->mist_app, epid_str, request_id, code, msg_str);

    (*env)->ReleaseStringUTFChars(env, java_epid, epid_str);
    (*env)->ReleaseStringUTFChars(env, java_msg, msg_str);

    monitor_exit();
}

/*
 * Class:     mist_node_MistNode
 * Method:    invokeResponse
 * Signature: (Ljava/lang/String;I[B)V
 */
JNIEXPORT void JNICALL Java_mist_node_MistNode_invokeResponse(JNIEnv *env, jobject java_this, jstring java_epid, jint request_id, jbyteArray java_data) {
    android_wish_printf("in invokeResponse");

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return;
    }

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
    mist_invoke_response(get_mist_model()->mist_app, epid_str, request_id, &bs);

    (*env)->ReleaseStringUTFChars(env, java_epid, epid_str);
    bson_destroy(&bs);
    free(data);

    monitor_exit();
}

/*
 * Class:     mist_node_MistNode
 * Method:    invokeError
 * Signature: (Ljava/lang/String;IILjava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_mist_node_MistNode_invokeError(JNIEnv *env, jobject java_this, jstring java_epid, jint request_id, jint code, jstring java_msg) {
    android_wish_printf("in invokeError");

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return;
    }

    if (java_epid == NULL) {
        android_wish_printf("in invokeError: java_epid is NULL");
        return;
    }
    char *epid_str =  (char*) (*env)->GetStringUTFChars(env, java_epid, NULL);
    char *msg_str = (char*) (*env)->GetStringUTFChars(env, java_msg, NULL);

    mist_invoke_error(get_mist_model()->mist_app, epid_str, request_id, code, msg_str);

    (*env)->ReleaseStringUTFChars(env, java_epid, epid_str);
    (*env)->ReleaseStringUTFChars(env, java_msg, msg_str);

    monitor_exit();
}

/*
 * Class:     mist_node_MistNode
 * Method:    changed
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_mist_node_MistNode_changed(JNIEnv *env, jobject java_this, jstring java_epid) {
    android_wish_printf("in changed");

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return;
    }

    if (java_epid == NULL) {
        android_wish_printf("in changed: java_epid is NULL");
        return;
    }
    char *epid_str =  (char*) (*env)->GetStringUTFChars(env, java_epid, NULL);

    mist_value_changed(get_mist_model()->mist_app, epid_str);

    (*env)->ReleaseStringUTFChars(env, java_epid, epid_str);

    monitor_exit();
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
    android_wish_printf("in Mist Node generic_callback");
    /* Enter Critical section to protect the cb lists from concurrent modification. */

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return;
    }

    wish_app_t *app = addon_get_wish_app();

    //WISHDEBUG(LOG_CRITICAL, "Callback invoked!");
    bson_visit("Mist Node generic_callback payload", payload);

    /* First decide if this callback is a Mist or Wish callback - this determines which callback list we will examine */
    struct callback_list_elem **cb_list_head = NULL;
    if (req->client == &(get_mist_model()->mist_app->protocol.rpc_client)) {
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

    JavaVM *javaVM = addon_get_java_vm();

    bool did_attach = false;
    JNIEnv * my_env = NULL;
    if (getJNIEnv(javaVM, &my_env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");

        monitor_exit();

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

        monitor_exit();

        return;
    }

    jobject cb_obj = NULL;
    struct callback_list_elem *elem = NULL;
    struct callback_list_elem *tmp = NULL;

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

    if (cb_obj == NULL) {
        WISHDEBUG(LOG_CRITICAL, "Could not find the request from cb_list");

        monitor_exit();

        return;
    }

    /* cb_obj now has the call-up object reference */
    jclass cbClass = (*my_env)->GetObjectClass(my_env, cb_obj);
    if (cbClass == NULL) {
        WISHDEBUG(LOG_CRITICAL, "Cannot get Mist API callback class");

        monitor_exit();

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

            monitor_exit();

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

            monitor_exit();

            return;
        }
        /* FIXME: Should we release the monitor here, and re-acquire it after returning from the cb method?
         Currently there should be no need, as the execution is serial due to WishBridge enqueuing execution of receive_core_to_app on the App's main thread. 
         This applies to err and sig CBs also. */
        (*my_env)->CallVoidMethod(my_env, cb_obj, ackMethodId, java_data);
    } else if (is_sig) {
        /* Invoke "sig" method of MistNode.RequestCb:
          public void sig(byte[]);
            descriptor: ([B)V
        */
        jmethodID sigMethodId = (*my_env)->GetMethodID(my_env, cbClass, "sig", "([B)V");
        if (sigMethodId == NULL) {
            WISHDEBUG(LOG_CRITICAL, "Cannot get sig method");

            monitor_exit();

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

            monitor_exit();

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

    /* Exit critical section */

    monitor_exit();
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
JNIEXPORT jint JNICALL Java_mist_node_MistNode_request(JNIEnv *env, jobject java_this, jbyteArray java_peer, jbyteArray java_req, jobject callback_obj) {
    android_wish_printf("in MistNode request");

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return 0;
    }

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


    wish_protocol_peer_t peer;
    /* Resolve the BSON peer to a wish_protocol_peer: wish_protocol_peer_find_from_bson */
    bool peer_resolve_success = wish_protocol_peer_populate_from_bson(&peer, peer_bson);

    if (!peer_resolve_success) {
        android_wish_printf("Could not find peer for request!");
        return 0;
    }

    bson req_bs;
    bson_init_with_data(&req_bs, req_bson);
    /* Send down the request, with our generic_cb callback, save the request id. */
    rpc_client_req *req = mist_app_request(get_mist_model()->mist_app, &peer, &req_bs, generic_callback);

    int request_id = req->id;
    /* Save the mapping request_id -> callback object */
    save_request(env, &mist_cb_list_head, request_id, callback_obj);

    free(peer_bson);
    free(req_bson);

    monitor_exit();

    android_wish_printf("in request (exit)");
    return request_id;
}

/*
 * Class:     mistNode_MistNode
 * Method:    requestCancel
 * Signature: (I)I
 */
JNIEXPORT void JNICALL Java_mist_node_MistNode_requestCancel(JNIEnv *env, jobject java_this, jint request_id) {
    android_wish_printf("in MistNode requestCancel");

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return;
    }

    rpc_client_req *req = rpc_client_find_req(&get_mist_model()->mist_app->protocol.rpc_client, request_id);

    if (req == NULL) {
        android_wish_printf("in MistNode requestCancel: req is null, this means no RPC with request_id %i found!", request_id);
    }
    else {

        mist_app_cancel(get_mist_model()->mist_app, req);
        /* FIXME: Should clean up the JNI request list also. */
        
        struct callback_list_elem *elem = NULL;
        struct callback_list_elem *tmp = NULL;

        LL_FOREACH_SAFE(mist_cb_list_head, elem, tmp) {
            if (elem->request_id == request_id) {
                /* Delete the elemen from list */
                LL_DELETE(mist_cb_list_head, elem);

                /* Delete the global ref for cb object*/
                if (elem->cb_obj != NULL) {
                    (*env)->DeleteGlobalRef(env, elem->cb_obj);
                }
                /* Free the memory for list element */
                free(elem);
            }
        }
        
    }

    monitor_exit();
}


/*
 * Class:     mist_node_MistNode
 * Method:    wishRequest
 * Signature: ([BLmist/node/MistNode/RequestCb;)I
 */
JNIEXPORT jint JNICALL Java_wish_WishApp_request(JNIEnv *env, jobject java_this, jbyteArray java_req, jobject callback_obj) {
    android_wish_printf("in WishApp request");

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return 0;
    }

    wish_app_t *app = addon_get_wish_app();

    size_t req_bson_len = (*env)->GetArrayLength(env, java_req);
    uint8_t *req_bson = (uint8_t *) calloc(req_bson_len, 1);
    if (req_bson == NULL) {
        android_wish_printf("Malloc fails (req)");
        monitor_exit();
        return 0;
    }
    (*env)->GetByteArrayRegion(env, java_req, 0, req_bson_len, req_bson);

    bson req_bs;
    bson_init_with_data(&req_bs, req_bson);
    rpc_client_req *req = wish_app_request(app, &req_bs, generic_callback, NULL);

    int request_id = req->id;
    save_request(env, &wish_cb_list_head, request_id, callback_obj);

    free(req_bson);

    monitor_exit();

    return request_id;
}   

/*
 * Class:     wishApp_WishApp
 * Method:    requestCancel
 * Signature: (I)I
 */
JNIEXPORT void JNICALL Java_wish_WishApp_requestCancel(JNIEnv *env, jobject java_this, jint request_id) {
    android_wish_printf("in WishApp requestCancel");

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return;
    }

    wish_app_t *app = addon_get_wish_app();

    rpc_client_req *req = rpc_client_find_req(&app->rpc_client, request_id);

    if (req == NULL) {
        android_wish_printf("in WishApp requestCancel: req is null, this means no RPC with request_id %i found!", request_id);
    }
    else {
        wish_app_cancel(app, req);
        
        /* Clean up the JNI request list also. */
        
        struct callback_list_elem *elem = NULL;
        struct callback_list_elem *tmp = NULL;

        LL_FOREACH_SAFE(wish_cb_list_head, elem, tmp) {
            if (elem->request_id == request_id) {
                LL_DELETE(wish_cb_list_head, elem);

                /* Delete the global ref for cb object*/
                if (elem->cb_obj != NULL) {
                    (*env)->DeleteGlobalRef(env, elem->cb_obj);
                }
                /* Free the memory for list element */
                free(elem);
            }
        }
        
    }

    monitor_exit();
}