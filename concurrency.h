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
