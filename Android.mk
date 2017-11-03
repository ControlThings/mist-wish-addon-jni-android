LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
 
# Here we give our module name and source file(s)


WISH_MODULES = mist-c99/deps/mbedtls-2.1.2/library mist-c99/wish_app mist-c99/deps/wish-rpc-c99/src mist-c99/deps/ed25519/src mist-c99/deps/bson mist-c99/src

WISH_SRC := 
LOCAL_MODULE    := mist
LOCAL_SRC_FILES := wish_bridge_jni.c jni_utils.c wish_core_api.c mist_node_api_jni.c mist-c99/wish_app_deps/wish_platform.c mist-c99/wish_app_deps/wish_debug.c mist-c99/wish_app_deps/wish_utils.c mist-c99/wish_app_deps/wish_fs.c filesystem.c $(foreach sdir,$(WISH_MODULES),$(wildcard $(sdir)/*.c))
LOCAL_C_INCLUDES := mist-c99/deps/mbedtls-2.1.2/include mist-c99/wish mist-c99/deps/ed25519/src mist-c99/deps/wish-rpc-c99/src mist-c99/wish_app mist-c99/wish_app_deps mist-c99/port/unix mist-c99/deps/bson/ mist-c99/deps/uthash/include mist-c99/src
LOCAL_LDLIBS := -llog

#LOCAL_CFLAGS := -g -O0 -Wall -Wno-pointer-sign -Werror -fvisibility=hidden -Wno-unused-variable -Wno-unused-function
LOCAL_CFLAGS := -Os -Wall -Wno-pointer-sign -Werror -fvisibility=hidden -Wno-unused-variable -Wno-unused-function
#LOCAL_CFLAGS += -DRELEASE_BUILD

#Put each function in own section, so that linker can discard unused code
LOCAL_CFLAGS += -ffunction-sections -fdata-sections 
#instruct linker to discard unsused code:
LOCAL_LDFLAGS += -Wl,--gc-sections

NDK_LIBS_OUT=jniLibs

include $(BUILD_SHARED_LIBRARY)
