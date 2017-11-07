//
// Created by jan on 11/7/17.
//

#include <jni.h>
#include "concurrency.h"
#include "jni_utils.h"

static jobject monitor_instance = NULL;
static JavaVM *java_vm = NULL;

void monitor_init(JavaVM *vm, jobject monitor_obj_global_ref) {
    monitor_instance = monitor_obj_global_ref;
    java_vm = vm;
}

/* Obtain (possibly blocking) the monitor (mutex) of the RawApi class instance.
 Returns 0 for a successful gaining of the monitor, or -1 for an error.
 */
int monitor_enter(void) {
    JNIEnv * my_env = NULL;
    bool did_attach = false;
    if (getJNIEnv(java_vm, &my_env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return -1;
    }

    if ((*my_env)->MonitorEnter(my_env, monitor_instance) != 0) {
        android_wish_printf("Error while entering monitor");
        return -1;
    }

    if (did_attach) {
        detachThread(java_vm);
    }
    return 0;
}

/* Release the monitor associated with the RawApi class instance.
    Returns 0 for a successful release of the monitor, or -1 for an error.
 */
int monitor_exit(void) {
    JNIEnv * my_env = NULL;
    bool did_attach = false;
    if (getJNIEnv(java_vm, &my_env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");
        return -1;
    }

    if ((*my_env)->MonitorExit(my_env, monitor_instance) != 0) {
        android_wish_printf("Error while exiting monitor");
        return -1;
    }

    if (did_attach) {
        detachThread(java_vm);
    }
    return 0;
}
