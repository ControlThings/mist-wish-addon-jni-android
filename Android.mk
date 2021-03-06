LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
 
# Here we give our module name and source file(s)

WISH_MODULES = $(LOCAL_PATH)/mist-c99/deps/mbedtls-2.1.2/library $(LOCAL_PATH)/mist-c99/wish_app $(LOCAL_PATH)/mist-c99/deps/wish-rpc-c99/src $(LOCAL_PATH)/mist-c99/deps/ed25519/src $(LOCAL_PATH)/mist-c99/deps/bson $(LOCAL_PATH)/mist-c99/src

WISH_SRC := 
LOCAL_MODULE    := mist
LOCAL_SRC_FILES := addon.c wish_app_jni.c wish_bridge_jni.c jni_utils.c concurrency.c mist_api_jni_implementation.c jni_implementation.c mist-c99/wish_app_deps/wish_platform.c mist-c99/wish_app_deps/wish_debug.c mist-c99/wish_app_deps/wish_utils.c mist-c99/wish_app_deps/wish_fs.c filesystem.c $(foreach sdir,$(WISH_MODULES),$(wildcard $(sdir)/*.c))
LOCAL_C_INCLUDES := $(LOCAL_PATH)/mist-c99/deps/mbedtls-2.1.2/include $(LOCAL_PATH)/mist-c99/wish $(LOCAL_PATH)/mist-c99/deps/ed25519/src $(LOCAL_PATH)/mist-c99/deps/wish-rpc-c99/src $(LOCAL_PATH)/mist-c99/wish_app $(LOCAL_PATH)/mist-c99/wish_app_deps $(LOCAL_PATH)/mist-c99/port/unix $(LOCAL_PATH)/mist-c99/deps/bson/ $(LOCAL_PATH)/mist-c99/deps/uthash/include $(LOCAL_PATH)/mist-c99/src
LOCAL_LDLIBS := -llog

#LOCAL_CFLAGS := -g -O0 -Wall -Wno-pointer-sign -Werror -fvisibility=hidden -Wno-unused-variable -Wno-unused-function
LOCAL_CFLAGS := -Os -Wall -Wno-pointer-sign -Werror -fvisibility=hidden -Wno-unused-variable -Wno-unused-function
LOCAL_CFLAGS += -DMIST_API_VERSION_STRING=\"$(shell cd mist-c99; git describe --abbrev=4 --dirty --always --tags)\"
LOCAL_CFLAGS += -DRELEASE_BUILD

#Put each function in own section, so that linker can discard unused code
LOCAL_CFLAGS += -ffunction-sections -fdata-sections 
#instruct linker to discard unsused code:
LOCAL_LDFLAGS += -Wl,--gc-sections

NDK_LIBS_OUT=jniLibs

include $(BUILD_SHARED_LIBRARY)
