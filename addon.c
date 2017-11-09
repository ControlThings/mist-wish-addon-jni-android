//
// Created by jan on 11/9/17.
//

#include <stdlib.h>

#include <jni.h>

#include "wish_fs.h"
#include "filesystem.h"
#include "addon.h"
#include "jni_utils.h"

static JavaVM *javaVM;

JavaVM *addon_get_java_vm(void) {
    return javaVM;
}

void addon_set_java_vm(JavaVM *vm) {
    javaVM = vm;
}

static wish_app_t *app;

wish_app_t *addon_get_wish_app(void) {
    return app;
}

void addon_init_wish_app(char *app_name) {
    /* Register the platform dependencies with Wish/Mist */

    wish_platform_set_malloc(malloc);
    wish_platform_set_realloc(realloc);
    wish_platform_set_free(free);
    srandom(time(NULL));
    wish_platform_set_rng(random);
    wish_platform_set_vprintf(android_wish_vprintf);
    wish_platform_set_vsprintf(vsprintf);

    /* File system functions are needed for Mist mappings! */
    wish_fs_set_open(my_fs_open);
    wish_fs_set_read(my_fs_read);
    wish_fs_set_write(my_fs_write);
    wish_fs_set_lseek(my_fs_lseek);
    wish_fs_set_close(my_fs_close);
    wish_fs_set_rename(my_fs_rename);
    wish_fs_set_remove(my_fs_remove);

    app = wish_app_create(app_name);
    if (app == NULL) {
        WISHDEBUG(LOG_CRITICAL, "Failed creating wish app!");
    }
}

static mist_model *model;

mist_model *get_mist_model(void) {
    return model;
}

static mist_app_t *mist_app;

mist_app_t *addon_get_mist_app(void) {
    return mist_app;
}

void addon_init_mist_app(char *app_name) {

    addon_init_wish_app(app_name);

    mist_app = start_mist_app();
    if (mist_app == NULL) {
        WISHDEBUG(LOG_CRITICAL, "Failed creating mist app!");
    }

    model = &mist_app->model;

    wish_app_add_protocol(app, &mist_app->protocol);
    mist_app->app = app;
}