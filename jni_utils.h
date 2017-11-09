//
// Created by jan on 6/7/16.
//

#ifndef WISH_JNI_UTILS_H
#define WISH_JNI_UTILS_H

#include <stdbool.h>

int android_wish_printf(const char *format, ...);
int android_wish_vprintf(const char *format, va_list arg_list);

int getJNIEnv(JavaVM *vm, JNIEnv **result_env, bool * didAttach);
int detachThread();

void check_and_report_exception(JNIEnv *env);

jmethodID get_methodID(JNIEnv *env, jobject instance, char *method_name, char* signature);

/*  Get String field from a Java object
    Note: Caller is responsible for releasing the pointer returned from this function! */
char *get_string_from_obj_field(JNIEnv *env, jobject java_obj, char *field_name);

void log_bson_visit_inner(const uint8_t* data, uint8_t depth, char *log_tag);

#endif //WISH_JNI_UTILS_H
