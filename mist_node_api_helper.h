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
// Created by jan on 11/4/16.
//

#ifndef REFERENCE_MIST_NODE_API_HELPER_H
#define REFERENCE_MIST_NODE_API_HELPER_H
#include <jni.h>

jobject getMistNodeApiInstance();

void setMistNodeInstance(jobject global_ref);





#endif //REFERENCE_MIST_NODE_API_HELPER_H
