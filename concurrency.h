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
// Created by jan on 11/7/17.
//

#ifndef MIST_API_ANDROID_CONCURRENCY_H
#define MIST_API_ANDROID_CONCURRENCY_H

#include <jni.h>

/*
 * Initialise concurrency control. Mutual exclusion will use the monitor of object given here as a parameter.
 */
void monitor_init(JavaVM *vm, jobject monitor_obj_global_ref);

/*  Acquire mutex.
 *  Returns 0 for a successful gaining of the monitor, or -1 for an error.
 */
int monitor_enter(void);

/* Release mutex.
 * Returns 0 for a successful release of the monitor, or -1 for an error.
 */
int monitor_exit(void);

#endif //MIST_API_ANDROID_CONCURRENCY_H
