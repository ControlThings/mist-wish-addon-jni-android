//
// Created by jan on 11/4/16.
//

#ifndef REFERENCE_MIST_NODE_API_HELPER_H
#define REFERENCE_MIST_NODE_API_HELPER_H
#include <jni.h>
#include "wish_app.h"
#include "mist_app.h"

wish_app_t *get_wish_app(void);

mist_app_t *get_mist_app(void);

JavaVM *getJavaVM(void);

jobject getMistNodeApiInstance();

void setMistNodeInstance(jobject global_ref);

void init_common_mist_app(char *app_name);



#endif //REFERENCE_MIST_NODE_API_HELPER_H
