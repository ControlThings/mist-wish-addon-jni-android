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
// Created by jan on 11/9/17.
//

#ifndef MIST_API_ANDROID_ADDON_H
#define MIST_API_ANDROID_ADDON_H

#include <jni.h>

#include <stdbool.h>
#include "wish_app.h"
#include "mist_app.h"

bool addon_is_started(void);

JavaVM *addon_get_java_vm(void);

void addon_set_java_vm(JavaVM *);

void addon_init_wish_app(char *app_name);

void addon_init_mist_app(char *app_name);

wish_app_t *addon_get_wish_app(void);

mist_app_t *addon_get_mist_app(void);

mist_model *get_mist_model(void);

#endif //MIST_API_ANDROID_ADDON_H
