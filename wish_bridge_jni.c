//
// Created by jan on 6/7/16.
//
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "wish_app.h"
#include "app_service_ipc.h"

/*
To re-create JNI interface:

 1. Renew header file using javah
      javah -classpath ../../../../mistnodeapi/build/intermediates/classes/debug/:/home/jan/Android/Sdk/platforms/android-16/android.jar -o wish_bridge_jni.h mist.WishBridgeJni
 2. Fix .c file function names to correspond to new .h file
 3. Fix all calls to java from c (if any)
      from:
        jfieldID invokeCallbackField = (*env)->GetFieldID(env, endpointClass, "invokeCallback", "Lfi/ct/mist/nodeApi/Endpoint$Invokable;");
      to:
        jfieldID invokeCallbackField = (*env)->GetFieldID(env, endpointClass, "invokeCallback", "Lmist/node/Endpoint$Invokable;");

*/

/* To re-generate the JNI Header file:  */
#include "wish_bridge_jni.h"
#include "jni_utils.h"
#include "mist_node_api_helper.h"

static JavaVM *javaVM;

static jobject wishAppBridgeInstance;

/*
* Class:     fi_ct_mist_WishAppJni
* Method:    register
* Signature: (Lfi/ct/mist/WishAppBridge;)V
*/
JNIEXPORT jobject JNICALL Java_mistNodeApi_WishBridgeJni_register(JNIEnv *env, jobject jthis, jobject wishAppBridge) {
    /* Register a refence to the JVM */
    if ((*env)->GetJavaVM(env,&javaVM) < 0) {
        android_wish_printf("Failed to GetJavaVM");
        return NULL;
    }

    /* Create a global reference to the WishAppBridge instance here */
    wishAppBridgeInstance = (*env)->NewGlobalRef(env, wishAppBridge);
    if (wishAppBridgeInstance == NULL) {
        android_wish_printf("Out of memory!");
        return NULL;
    }

    jbyteArray java_wsid = (*env)->NewByteArray(env, WISH_WSID_LEN);
    if (java_wsid == NULL) {
        android_wish_printf("Failed creating wsid byte array");
        return NULL;
    }
    (*env)->SetByteArrayRegion(env, java_wsid, 0, WISH_WSID_LEN, (const jbyte *) get_mist_node_app()->wsid);

    return java_wsid;
}

uint8_t my_wsid[WISH_WSID_LEN];

/** Implementation of send_app_to_core.
 * It will transform the arguments into Java objects and calls receiveAppToCore defined in
 */
void send_app_to_core(uint8_t *wsid, const uint8_t *data, size_t len) {
    android_wish_printf("Send app to core");

    if (data == NULL) {
        android_wish_printf("Send app to core: data is NULL!");
        return;
    }

    if (wsid == NULL) {
        android_wish_printf("send_app_to_core was supplied with NULL wsid, this indicates an error, giving up.");
        return;
    }

    memcpy(my_wsid, wsid, WISH_WSID_LEN);

    /* This will be set to true, if the thread of execution was not a JavaVM thread */
    bool did_attach = false;

    if (javaVM == NULL) {
        android_wish_printf("javaVM is NULL, this indicates wish_bridge_jni is not registered(), giving up.");
        return;
    }

    JNIEnv * my_env = NULL;
    if (getJNIEnv(javaVM, &my_env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return;
    }

    if (wishAppBridgeInstance == NULL) {
        android_wish_printf("wishAppBridgeInstance is null, this probably means that register() has not bee called. Giving up.");
        return;
    }

    /* For method signature strings, see:
     * http://docs.oracle.com/javase/1.5.0/docs/guide/jni/spec/types.html#wp16432 */
    jclass wishAppBridgeClass = (*my_env)->GetObjectClass(my_env, wishAppBridgeInstance);
    jmethodID receiveAppToCoreMethodId = (*my_env)->GetMethodID(my_env, wishAppBridgeClass, "receiveAppToCore", "([B[B)V");
    if (receiveAppToCoreMethodId == NULL) {
        android_wish_printf("Method cannot be found");
        return;
    }

    /* See JNI spec: http://docs.oracle.com/javase/6/docs/technotes/guides/jni/spec/functions.html#array_operations */
    jbyteArray java_buffer = (*my_env)->NewByteArray(my_env, len);
    jbyteArray java_wsid = (*my_env)->NewByteArray(my_env, WISH_WSID_LEN);
    /* See http://docs.oracle.com/javase/1.5.0/docs/guide/jni/spec/types.html for information on
     * how the "C" char and "JNI" jchar are the same thing! */
    (*my_env)->SetByteArrayRegion(my_env, java_buffer, 0, len, (const jbyte *) data);
    (*my_env)->SetByteArrayRegion(my_env, java_wsid, 0, WISH_WSID_LEN, (const jbyte *) wsid);

    (*my_env)->CallVoidMethod(my_env, wishAppBridgeInstance, receiveAppToCoreMethodId, java_wsid, java_buffer);

    if (did_attach) {
        detachThread(javaVM);
    }

    (*my_env)->DeleteLocalRef(my_env, java_buffer);
    (*my_env)->DeleteLocalRef(my_env, java_wsid);
}

JNIEXPORT void JNICALL Java_mistNodeApi_WishBridgeJni_receive_1core_1to_1app(JNIEnv *env, jobject jthis, jbyteArray java_data) {
    android_wish_printf("Receive core to app");

    if (java_data == NULL) {
        android_wish_printf("Receive core to app: java_data is null.");
        return;
    }

    size_t data_length = (*env)->GetArrayLength(env, java_data);
    uint8_t *data = (uint8_t *) calloc(data_length, 1);
    if (data == NULL) {
        android_wish_printf("Malloc fails");
        return;
    }
    (*env)->GetByteArrayRegion(env, java_data, 0, data_length, data);
    wish_app_t *dst_app = wish_app_find_by_wsid(my_wsid);
    if (dst_app != NULL) {
        android_wish_printf("Found app by wsid");
        wish_app_determine_handler(dst_app, data, data_length);
    }
    else {
        android_wish_printf("Cannot find app by wsid");
    }

    free(data);
}

/*
 * Class:     fi_ct_mist_mistnodeapi_WishBridgeJni
 * Method:    connected
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_mistNodeApi_WishBridgeJni_connected
  (JNIEnv *env, jobject jthis, jboolean connected) {
    /* Call wish_app_login on our app */
    wish_app_connected(get_mist_node_app(), connected);

}