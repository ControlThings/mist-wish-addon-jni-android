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
// Created by jan on 6/7/16.
//

#include <string.h>
#include <jni.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <android/log.h>
#include "jni_utils.h"

#include "bson.h"


int android_wish_printf(const char *format, ...) {
    va_list arg_list;
    va_start(arg_list, format);
    int ret = __android_log_vprint(ANDROID_LOG_DEBUG, "native", format, arg_list);
    va_end(arg_list);
    return ret;
}

/* A version of the printf command that takes a variable length argument list as a parameter.
 * This is needed as the function which will be passed down to the Wish library. (the "normal" printf version above will not do) */
int android_wish_vprintf(const char *format, va_list arg_list) {
    int ret = __android_log_vprint(ANDROID_LOG_DEBUG, "native", format, arg_list);
    return ret;
}


/* This function gets the JNIEnv associated with the current thread of execution.
 * Returns 0 if the operation was successful, or a negative value for a failure. The function uses a global reference to the JavaVM which is stored in function register().
 *
 * The function saves information on wether the current thread had to attached or not to the parameter didAttach. If didAttach==true after calling this function, it means that the thread must be detatched from the
 * javaVM after interaction over JNI is no longer needed. This should apply only attempt is made to access java methods from a thread (process?) not originally instantiated by the JavaVM.
 *
 *
 *
 */
int getJNIEnv(JavaVM *javaVM, JNIEnv **result_env, bool * didAttach) {
    int retval = 0;
    int getEnvStat = (*javaVM)->GetEnv(javaVM, (void **)result_env, JNI_VERSION_1_6);
    if (getEnvStat == JNI_EDETACHED) {
        android_wish_printf("GetEnv: not attached. Trying to attach to current thread");
        if ((*javaVM)->AttachCurrentThread(javaVM, result_env, NULL) != 0) {
            /* Attaching did not work */
            android_wish_printf("Failed to attach");
            retval = -1;
        }
        else {
            /* Attaching was a success */
            *didAttach = true;
            retval = 0;
        }
    } else if (getEnvStat == JNI_OK) {
        if ((*result_env) == NULL) {
            android_wish_printf("GetEnv: fail");
            retval = -1;
        }
        else {
            retval = 0;
        }
    } else if (getEnvStat == JNI_EVERSION) {
        android_wish_printf("GetEnv: version not supported");
        retval = -1;
    }
    return retval;
}

/* This function detaches the current thread from the JavaVM.
 * Returns 0 if the call was successful */
int detachThread(JavaVM *javaVM) {
    int retval = 0;
    int detachStat = (*javaVM)->DetachCurrentThread(javaVM);
    if (detachStat != JNI_OK) {
        android_wish_printf("Could not detach thread");
        retval = -1;
    }
    return retval;
}


/* Uncomment this, if you do not wish that JNI code will call exit() when it detects an exception */
#define DIE_OF_EXCEPTION

/* To easily spot exceptions that are handled by this: ~/Android/Sdk/platform-tools/adb logcat | grep -A 3 -B 0 "Java exception" */
void check_and_report_exception(JNIEnv *env) {
    if ((*env)->ExceptionCheck(env)) {
        android_wish_printf("Detected a Java exception, output from ExceptionDescribe:");
        (*env)->ExceptionDescribe(env);
#ifdef DIE_OF_EXCEPTION
        const char* error_str = "Exiting because of exception detected in JNI";
        android_wish_printf(error_str);
        (*env)->FatalError(env, error_str);
#else
        /* Just "catch" the exception and do nothing about it */
        android_wish_printf("Just clearning the Java exception!");
        (*env)->ExceptionClear(env);
#endif
    }
}


jmethodID get_methodID(JNIEnv *env, jobject instance, char *method_name, char* signature) {

    if (instance == NULL) {
        android_wish_printf("get_methodID, instance is NULL!");
        return NULL;
    }

    jclass clazz = (*env)->GetObjectClass(env, instance);

    jmethodID method_id = (*env)->GetMethodID(env, clazz, method_name, signature);
    if (method_id == NULL) {
        android_wish_printf("get_methodID: Cannot get method %s with signature %s", method_name, signature);
        return NULL;
    }

    return method_id;
}

/*  Get String field from a Java object

    Note: Caller is responsible for releasing the pointer returned from this function! Use free(). */
char *get_string_from_obj_field(JNIEnv *env, jobject java_obj, char *field_name) {
    android_wish_printf("in get_string_from_obj_field");
    jclass java_class = (*env)->GetObjectClass(env, java_obj);
    if (java_class == NULL) {
        android_wish_printf("Could not resolve endpoint class while getting value for field name %s", field_name);
        return NULL;
    }

    /* Get "id" field, a String */
    jfieldID field_id = (*env)->GetFieldID(env, java_class, field_name, "Ljava/lang/String;");
    if (field_id == NULL) {
        android_wish_printf("Could not get field id for %s", field_name);
        return NULL;
    }

    jobject java_string = (*env)->GetObjectField(env, java_obj, field_id);
    if (java_string == NULL) {
        android_wish_printf("Could not get String for %s", field_name);
        return NULL;
    }

    char *str =  (char*) (*env)->GetStringUTFChars(env, java_string, NULL);
    if (str == NULL) {
        android_wish_printf("Could not GetStringUTF of field %s", field_name);
        return NULL;
    }

    char *copy = strdup(str);
    (*env)->ReleaseStringUTFChars(env, java_string, str);

    return copy;
}

/*
 * This function traverses the bson_doc given as argument, and calls the
 * visitor_func for every element encountered. Document and
 * array elements be recursively handled in the same way. */
void log_bson_visit_inner(const uint8_t* data, uint8_t depth, char *log_tag) {

    bson_iterator i;
    const char *key;
    bson_timestamp_t ts;
    BSON_ITERATOR_FROM_BUFFER(&i, data);

    while (bson_iterator_next(&i)) {
        bson_type t = BSON_ITERATOR_TYPE(&i);
        if (t == 0) { break; }
        key = BSON_ITERATOR_KEY(&i);

        char indent[33];
        memset(indent, ' ', 32);
        indent[(depth+1)*4] = 0;

        const char* elem = NULL;

        switch (t) {
            case BSON_STRING:
                //WISHDEBUG(LOG_CRITICAL, "%s" AC_WHITE_STRING ": " ANSI_COLOR_YELLOW "'%s'" ANSI_COLOR_RESET, indent, key, bson_iterator_string(&i));
                __android_log_print(ANDROID_LOG_DEBUG, log_tag, "%s%s: '%s'", indent, key, bson_iterator_string(&i));
                break;
            case BSON_BINDATA:
                elem = bson_iterator_bin_data(&i);
                /*
                WISHDEBUG(LOG_CRITICAL, "%s" AC_WHITE_STRING ": Buffer(" ANSI_COLOR_CYAN "0x%02x %02x %02x %02x" ANSI_COLOR_RESET " ...)",
                        indent, key, elem[0] & 0xff, elem[1] & 0xff, elem[2] & 0xff, elem[3] & 0xff);
                        */
                __android_log_print(ANDROID_LOG_DEBUG, log_tag, "%s%s: Buffer(0x%02x %02x %02x %02x ...)", indent, key, elem[0], elem[1], elem[2], elem[3]);
                break;
            case BSON_INT:
            case BSON_LONG:
            case BSON_DOUBLE:
                //WISHDEBUG(LOG_CRITICAL, "%s" AC_WHITE_STRING ": " ANSI_COLOR_GREEN "%d" ANSI_COLOR_RESET, indent, key, bson_iterator_int(&i));
                __android_log_print(ANDROID_LOG_DEBUG, log_tag, "%s%s: %d", indent, key, bson_iterator_int(&i));
                break;
            case BSON_BOOL:
                //WISHDEBUG(LOG_CRITICAL, "%s" AC_WHITE_STRING ": " ANSI_COLOR_BLUE "%s" ANSI_COLOR_RESET, indent, key, bson_iterator_bool(&i) ? "false" : "true" );
                __android_log_print(ANDROID_LOG_DEBUG, log_tag, "%s%s: %s", indent, key, bson_iterator_bool(&i) ? "true" : "false" );
                break;
            case BSON_OBJECT:
                //WISHDEBUG(LOG_CRITICAL, "%s" AC_WHITE_STRING ": {", indent, key);
                __android_log_print(ANDROID_LOG_DEBUG, log_tag, "%s%s: {", indent, key);
                log_bson_visit_inner(bson_iterator_value(&i), depth + 1, log_tag);
                break;
            case BSON_ARRAY:
                //WISHDEBUG(LOG_CRITICAL, "%s" AC_WHITE_STRING ": [", indent, key);
                __android_log_print(ANDROID_LOG_DEBUG, log_tag, "%s%s [", indent, key);
                log_bson_visit_inner(bson_iterator_value(&i), depth + 1, log_tag);
                break;
            case BSON_TIMESTAMP:
            case BSON_SYMBOL:
            case BSON_OID:
            case BSON_DATE:
            case BSON_UNDEFINED:
            case BSON_NULL:
            case BSON_REGEX:
            case BSON_CODE:
            case BSON_CODEWSCOPE:
            default:
                __android_log_print(ANDROID_LOG_DEBUG, log_tag, "%s%s: <unsupported BSON data, type %d>", indent, key, t);
                //WISHDEBUG(LOG_CRITICAL, "%s" AC_WHITE_STRING ": ", indent, key);
        }
    }

}