//
// Created by jan on 11/9/17.
//

#include <stdlib.h>

#include <jni.h>
#include <android/log.h>

#include "wish_app_jni.h"

#include "jni_utils.h"
#include "mist_node_api_helper.h"
#include "addon.h"

/*
 * Class:     wish_WishApp
 * Method:    startWishApp
 * Signature: (Ljava/lang/String;Laddon/WishFile;)I
 */
JNIEXPORT jint JNICALL Java_wish_WishApp_startWishApp(JNIEnv *env, jobject java_this, jstring java_app_name, jobject java_wish_file) {
    if (addon_is_started()) {
        return wish_WishApp_WISH_APP_ERROR_MULTIPLE_TIMES;
    }

    /* Using WishApp as an independent module is not supported yet. */
    android_wish_printf("Using WishApp as an independent module is not supported yet.");
    return wish_WishApp_WISH_APP_ERROR_UNSPECIFIED;
}


/*
 * Class:     wish_WishApp
 * Method:    bsonConsolePrettyPrinter
 * Signature: (Ljava/lang/String;[B)V
 */
JNIEXPORT void JNICALL Java_wishApp_WishApp_bsonConsolePrettyPrinter(JNIEnv *env, jclass java_this, jstring java_log_tag, jbyteArray java_bson) {
    char *log_tag =  (char*) (*env)->GetStringUTFChars(env, java_log_tag, NULL);
    uint8_t *bson_doc = NULL;
    if (java_bson != NULL) {
        /* Extract the byte array containing the RPC args */
        int array_length = (*env)->GetArrayLength(env, java_bson);
        uint8_t *array = (uint8_t*) calloc(array_length, 1);
        if (array == NULL) {
            __android_log_print(ANDROID_LOG_DEBUG, log_tag, "Out of memory in bsonConsolePrettyPrinter");
            return;
        }
        (*env)->GetByteArrayRegion(env, java_bson, 0, array_length, array);
        bson_doc = array;
    } else {
        /* java args bson is null - fill in an empty array */
        __android_log_print(ANDROID_LOG_DEBUG, log_tag, "the byte array is null!");
        return;
    }

    uint8_t depth = 0;
    __android_log_print(ANDROID_LOG_DEBUG, log_tag,  "BSON document:");
    log_bson_visit_inner(bson_doc, depth, log_tag);

    (*env)->ReleaseStringUTFChars(env, java_log_tag, log_tag);
    free(bson_doc);
}