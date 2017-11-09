//
// Created by jan on 11/9/17.
//

#ifndef MIST_API_ANDROID_ADDON_H
#define MIST_API_ANDROID_ADDON_H

#include <jni.h>

#include "wish_app.h"
#include "mist_app.h"

JavaVM *addon_get_java_vm(void);

void addon_set_java_vm(JavaVM *);

void addon_init_wish_app(char *app_name);

void addon_init_mist_app(char *app_name);

wish_app_t *addon_get_wish_app(void);

mist_app_t *addon_get_mist_app(void);

mist_model *get_mist_model(void);

#endif //MIST_API_ANDROID_ADDON_H
