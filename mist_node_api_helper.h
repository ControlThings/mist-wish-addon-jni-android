//
// Created by jan on 11/4/16.
//

#ifndef REFERENCE_MIST_NODE_API_HELPER_H
#define REFERENCE_MIST_NODE_API_HELPER_H
#include <jni.h>

wish_app_t *get_mist_node_app(void);

JavaVM *getJavaVM(void);

jobject getMistNodeApiInstance();

#endif //REFERENCE_MIST_NODE_API_HELPER_H
