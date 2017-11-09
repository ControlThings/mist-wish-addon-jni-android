//
// Created by jan on 12/5/16.
//

#include <jni.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "wish_app.h"
#include "wish_fs.h"
#include "filesystem.h"
#include "jni_utils.h"
#include "mist_node_api_helper.h"
#include "addon.h"

/* To see the corresponding method signatures of class WishFile:

javap -s -classpath ../../../../MistApi/build/intermediates/classes/debug/:/home/jan/Android/Sdk/platforms/android-16/android.jar addon.WishFile

*/

/* This defines the maximum allowable length of the file name */
#define MAX_FILENAME_LENGTH 32

static jobject wish_file_instance;

void wish_file_init(jobject global_wish_file_ref) {
    wish_file_instance = global_wish_file_ref;
}

/*
 * This function is called by the Wish fs abstraction layer when a file is to be opened.
 */
wish_file_t my_fs_open(const char *filename) {
    bool did_attach = false;
    JNIEnv * my_env = NULL;
    JavaVM *javaVM = getJavaVM();

    if (getJNIEnv(javaVM, &my_env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return 0;
    }

    jclass serviceClass = (*my_env)->GetObjectClass(my_env, wish_file_instance);
    jmethodID openFileMethodId = (*my_env)->GetMethodID(my_env, serviceClass, "open", "(Ljava/lang/String;)I");
    if (openFileMethodId == NULL) {
        android_wish_printf("Method cannot be found");
    }

    jstring java_filename = (*my_env)->NewStringUTF(my_env, filename);
    if (java_filename == NULL) {
        android_wish_printf("Filename string creation error");
    }

    int file_id = (*my_env)->CallIntMethod(my_env, wish_file_instance, openFileMethodId, java_filename);

    (*my_env)->DeleteLocalRef(my_env, java_filename);

    if (did_attach) {
        detachThread(javaVM);
    }
    return file_id;
}

/*
 * This function is called by the Wish fs abstraction layer when a file is to be closed
 */
int32_t my_fs_close(wish_file_t fileId) {
    bool did_attach = false;
    JNIEnv * my_env = NULL;
    JavaVM *javaVM = getJavaVM();

    if (getJNIEnv(javaVM, &my_env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return -1;
    }

    jclass serviceClass = (*my_env)->GetObjectClass(my_env, wish_file_instance);
    jmethodID closeFileMethodId = (*my_env)->GetMethodID(my_env, serviceClass, "close", "(I)I");
    if (closeFileMethodId == NULL) {
        android_wish_printf("Method cannot be found");
    }

    int ret = (*my_env)->CallIntMethod(my_env, wish_file_instance, closeFileMethodId, fileId);

    if (did_attach) {
        detachThread(javaVM);
    }
    return ret;
}

/*
 * This function is called when reading a file from Wish
 */
int32_t my_fs_read(wish_file_t fileId, void* buf, size_t count) {
    android_wish_printf("in my fs read");
    bool did_attach = false;
    JNIEnv * my_env = NULL;
    JavaVM *javaVM = getJavaVM();

    if (getJNIEnv(javaVM, &my_env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return -1;
    }

    jclass serviceClass = (*my_env)->GetObjectClass(my_env, wish_file_instance);
    jmethodID readFileMethodId = (*my_env)->GetMethodID(my_env, serviceClass, "read", "(I[BI)I");
    if (readFileMethodId == NULL) {
        android_wish_printf("Method cannot be found");
    }

    /* Create the Java byte array from the memory area we get as 'buf' */
    jbyteArray java_buf = (*my_env)->NewByteArray(my_env, count);

    (*my_env)->SetByteArrayRegion(my_env, java_buf, 0, count, buf);

    int ret = (*my_env)->CallIntMethod(my_env, wish_file_instance, readFileMethodId, fileId, java_buf, count);

    if (ret > 0) {
        (*my_env)->GetByteArrayRegion(my_env, java_buf, 0, ret, buf);
    }

    (*my_env)->DeleteLocalRef(my_env, java_buf);
    (*my_env)->DeleteLocalRef(my_env, serviceClass);

    if (did_attach) {
        detachThread(javaVM);
    }
    android_wish_printf("exiting my fs read");
    return ret;

}

/*
 * This function is called when writing to a file from Wish
 */
int32_t my_fs_write(wish_file_t fileId, const void* buf, size_t count) {
    android_wish_printf("in my fs write");
    bool did_attach = false;
    JNIEnv * my_env = NULL;
    JavaVM *javaVM = getJavaVM();

    if (getJNIEnv(javaVM, &my_env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return -1;
    }

    jclass serviceClass = (*my_env)->GetObjectClass(my_env, wish_file_instance);
    jmethodID writeFileMethodId = (*my_env)->GetMethodID(my_env, serviceClass, "write", "(I[BI)I");
    if (writeFileMethodId == NULL) {
        android_wish_printf("Method cannot be found");
    }

    /* Create the Java byte array from the memory area we get as 'buf' */
    jbyteArray java_buf = (*my_env)->NewByteArray(my_env, count);
    (*my_env)->SetByteArrayRegion(my_env, java_buf, 0, count, buf);

    int ret = (*my_env)->CallIntMethod(my_env, wish_file_instance, writeFileMethodId, fileId, java_buf, count);


    (*my_env)->DeleteLocalRef(my_env, java_buf);
    (*my_env)->DeleteLocalRef(my_env, serviceClass);

    if (did_attach) {
        detachThread(javaVM);
    }
    android_wish_printf("exit my fs write");
    return ret;

}

/*
 * This function is called by Wish when it wishes to move around in a file
 */
int32_t my_fs_lseek(wish_file_t fileId, wish_offset_t offset, int whence) {
    bool did_attach = false;
    JNIEnv * my_env = NULL;
    JavaVM *javaVM = getJavaVM();

    if (getJNIEnv(javaVM, &my_env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return -1;
    }

    jclass serviceClass = (*my_env)->GetObjectClass(my_env, wish_file_instance);
    jmethodID seekFileMethodId = (*my_env)->GetMethodID(my_env, serviceClass, "seek", "(III)J");
    if (seekFileMethodId == NULL) {
        android_wish_printf("Method cannot be found");
    }

    /* FIXME: The return type of seek() should be long (int64_t) also inside the wish filesystem abstraction layer! */
    jlong ret = (*my_env)->CallLongMethod(my_env, wish_file_instance, seekFileMethodId, fileId, offset, whence);

    if (did_attach) {
        detachThread(javaVM);
    }
    return (int32_t) ret;
}

int32_t my_fs_rename(const char *oldname, const char *newname) {
    bool did_attach = false;
    JNIEnv * my_env = NULL;
    JavaVM *javaVM = getJavaVM();

    if (getJNIEnv(javaVM, &my_env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return 0;
    }

    jclass serviceClass = (*my_env)->GetObjectClass(my_env, wish_file_instance);
    jmethodID renameMethodId = (*my_env)->GetMethodID(my_env, serviceClass, "rename", "(Ljava/lang/String;Ljava/lang/String;)I");
    if (renameMethodId == NULL) {
        android_wish_printf("Method cannot be found");
    }

    jstring java_oldfilename = (*my_env)->NewStringUTF(my_env, oldname);
    if (java_oldfilename == NULL) {
        android_wish_printf("Old filename string creation error");
    }

    jstring java_newfilename = (*my_env)->NewStringUTF(my_env, newname);
    if (java_newfilename == NULL) {
        android_wish_printf("New filename string creation error");
    }

    int retval = (*my_env)->CallIntMethod(my_env, wish_file_instance, renameMethodId, java_oldfilename, java_newfilename);

    (*my_env)->DeleteLocalRef(my_env, java_oldfilename);
    (*my_env)->DeleteLocalRef(my_env, java_newfilename);

    if (did_attach) {
        detachThread(javaVM);
    }
    return retval;
}

int32_t my_fs_remove(const char *file_name) {
    bool did_attach = false;
    JNIEnv * my_env = NULL;
    JavaVM *javaVM = getJavaVM();

    if (getJNIEnv(javaVM, &my_env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return 0;
    }

    jclass serviceClass = (*my_env)->GetObjectClass(my_env, wish_file_instance);
    jmethodID removeMethodId = (*my_env)->GetMethodID(my_env, serviceClass, "remove", "(Ljava/lang/String;)I");
    if (removeMethodId == NULL) {
        android_wish_printf("Method cannot be found");
    }

    jstring java_filename = (*my_env)->NewStringUTF(my_env, file_name);
    if (java_filename == NULL) {
        android_wish_printf("Error creating string!");
    }

    int retval = (*my_env)->CallIntMethod(my_env, wish_file_instance, removeMethodId, java_filename);

    (*my_env)->DeleteLocalRef(my_env, java_filename);

    if (did_attach) {
        detachThread(javaVM);
    }
    return retval;
}

