//
// Created by jan on 8/18/16.
//

/* This file implements the JNI functions defined in WishCoreApiJni.java
 * /
 *
 * The headerfile can be regenerated like this:
 * javah -classpath ../../../build/intermediates/classes/debug:/home/jan/Android/Sdk/platforms/android-16/android.jar -o wish_core_api.h fi.ct.mist.api.wish.WishCoreApiJni
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "wish_app.h"
#include "wish_core_api.h"
#include "app_service_ipc.h"
#include "bson.h"

#include "jni_utils.h"
#include "mist_node_api_helper.h"


extern wish_app_t *app; /* Created by start_mist_app in android_mist.c */

static JavaVM * javaVM;
static jobject wishApiJniInstance;

/*
 * Class:     fi_ct_mist_api_wish_WishCoreApiJni
 * Method:    register
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_fi_ct_mist_api_wish_WishCoreApiJni_register
        (JNIEnv *env, jobject jthis) {
    /* Register a global reference the WishCoreApiJni instance so we can call functions its methods from C */

    wishApiJniInstance = (*env)->NewGlobalRef(env, jthis);
    if (wishApiJniInstance == NULL) {
        android_wish_printf("Out of memory!");
        return;
    }

    /* Register a refence to the JVM */
    if ((*env)->GetJavaVM(env,&javaVM) < 0) {
        android_wish_printf("Failed to GetJavaVM");
        return;
    }




    /* FIXME deregister function for removing the global reference! */
}
