//
// Created by jan on 11/28/16.
//

/* To Re-generate device_api.h JNI linkage file from method definitions in DeviceApiJni:
 * javah -classpath ../../../build/intermediates/classes/debug:/media/akaustel/Data/android-sdk-linux/platforms/android-16/android.jar -o raw_api.h mist.raw.RawApi
 */


#include <stdlib.h>

#include "mist_api_jni.h"
#include "jni_utils.h"

#include "wish_app.h"
#include "mist_app.h"
#include "wish_debug.h"
#include "mist_api.h"

#include "wish_fs.h"
#include "filesystem.h"

#include "bson.h"
#include "utlist.h"
#include "bson_visit.h"

#include "concurrency.h"
#include "mist_node_api_helper.h"
#include "addon.h"

static mist_api_t* mist_api;

/*
 * Class:     fi_ct_mist_mistlib_raw_RawApi
 * Method:    startMistApp
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT jint JNICALL Java_mist_api_MistApi_startMistApi(JNIEnv *env, jobject jthis, jstring java_appName, jobject java_mistNode, jobject java_wish_file) {
    android_wish_printf("in startMistApi!");

     if (addon_is_started()) {
        return mist_api_MistApi_MIST_API_ERROR_MULTIPLE_TIMES;
     }

    JavaVM *javaVM;
    if ((*env)->GetJavaVM(env,&javaVM) < 0) {
        android_wish_printf("Failed to GetJavaVM");
        return mist_api_MistApi_MIST_API_ERROR_UNSPECIFIED;
    }
    addon_set_java_vm(javaVM);

    jobject wish_file_global_ref = (*env)->NewGlobalRef(env, java_wish_file);
    wish_file_init(wish_file_global_ref);

    jobject node_global_ref = (*env)->NewGlobalRef(env, java_mistNode);
    monitor_init(javaVM, node_global_ref);
    setMistNodeInstance(node_global_ref);

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return mist_api_MistApi_MIST_API_ERROR_UNSPECIFIED;
    }

    /* Register a refence to the JVM */

    char *name_str =  (char*) (*env)->GetStringUTFChars(env, java_appName, NULL);
    addon_init_mist_app(name_str);
    (*env)->ReleaseStringUTFChars(env, java_appName, name_str);


    mist_api = mist_api_init(addon_get_mist_app());

    monitor_exit();
    /* The app will login to core when the Bridge connects, this happens via the wish_app_connected(wish_app_t *app, bool connected) function in fi_ct_mist_mistlib_WishBridgeJni:connected */

    return mist_api_MistApi_MIST_API_SUCCESS;
}



/** Linked list element of Mist API RPC requests sent by us. It is used to keep track of callback objects and RPC ids. */
struct callback_list_elem {
    /** The RPC id associated with the callback entry */
    int id;
    /** The Java object (global) reference which will contain the callback methods: ack, sig, err... Remember to delete the global reference when callback is no longer needed! */
    jobject cb_obj;
    /** The next item in the list */
    struct callback_list_elem *next;
};

/** The linked list head of all the RPC requests sent by us */
struct callback_list_elem *cb_list_head = NULL;

/**
 * This call-back method is invoked both for Mist and Wish RPC requests. It will call methods of the callback object associated with the request id
 */
static void generic_callback(struct wish_rpc_entry* req, void *ctx, const uint8_t *payload, size_t payload_len) {
    WISHDEBUG(LOG_CRITICAL, "in Mist Api generic_callback");
    bson_visit("Mist Api generic_callback payload", payload);

    /* Enter Critical section */
    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return;
    };

    JavaVM *javaVM = addon_get_java_vm();

    bool did_attach = false;
    JNIEnv * my_env = NULL;
    if (getJNIEnv(javaVM, &my_env, &did_attach)) {
        android_wish_printf("Method invocation failure, could not get JNI env");

        monitor_exit();

        return;
    }

    int req_id = 0;

    bool is_ack = false;
    bool is_sig = false;
    bool is_err = false;

    bson_iterator it;
    bson_find_from_buffer(&it, payload, "ack");
    if (bson_iterator_type(&it) == BSON_INT) {
        req_id = bson_iterator_int(&it);
        if (req_id != 0) {
            is_ack = true;
        }
        WISHDEBUG(LOG_CRITICAL, "Request ack %i", req_id);
    }

    bson_find_from_buffer(&it, payload, "sig");
    if (bson_iterator_type(&it) == BSON_INT) {
        req_id = bson_iterator_int(&it);
        if (req_id != 0) {
            is_sig = true;
        }
        WISHDEBUG(LOG_CRITICAL, "Request sig %i", req_id);
    }

    bson_find_from_buffer(&it, payload, "err");
    if (bson_iterator_type(&it) == BSON_INT) {
        req_id = bson_iterator_int(&it);
        if (req_id != 0) {
            is_err = true;
        }
        WISHDEBUG(LOG_CRITICAL, "Request err %i", req_id);
    }

    if (!is_ack && !is_sig && !is_err) {
        WISHDEBUG(LOG_CRITICAL, "Not ack, sig or err!");

        monitor_exit();

        return;
    }

    jobject cb_obj = NULL;
    struct callback_list_elem *elem = NULL;
    struct callback_list_elem *tmp = NULL;

    LL_FOREACH_SAFE(cb_list_head, elem, tmp) {
        if (elem->id == req_id) {
            cb_obj = elem->cb_obj;
            if (is_ack || is_err) {
                WISHDEBUG(LOG_CRITICAL, "Removing cb_list element for id %i", req_id);
                LL_DELETE(cb_list_head, elem);
                free(elem);
            }
        }
    }

    if (cb_obj == NULL) {
        WISHDEBUG(LOG_CRITICAL, "Could not find the request from cb_list");

        monitor_exit();

        return;
    }

    /* cb_obj now has the call-up object reference */
    jclass cbClass = (*my_env)->GetObjectClass(my_env, cb_obj);
    if (cbClass == NULL) {
        WISHDEBUG(LOG_CRITICAL, "Cannot get Mist API callback class");

        monitor_exit();

        return;
    }

    jbyteArray java_data = NULL;
    if (is_ack || is_sig) {
        bson bs;
        bson_init(&bs);

        /* Find the data element from payload */
        bson_find_from_buffer(&it, payload, "data");

        bson_type type = bson_iterator_type(&it);

        if (type == BSON_EOO) {
            WISHDEBUG(LOG_CRITICAL, "Unexpected end of BSON, no data");
            /* FIXME activate err callback here */

            monitor_exit();

            return;
        }
        else {
            bson_append_element(&bs, "data", &it);
        }

        bson_finish(&bs);

        uint8_t *data_doc = (uint8_t *) bson_data(&bs);
        size_t data_doc_len = bson_size(&bs);

        java_data = (*my_env)->NewByteArray(my_env, data_doc_len);
        if (java_data == NULL) {
            WISHDEBUG(LOG_CRITICAL, "Failed creating java buffer for ack or sig data");

            monitor_exit();

            return;
        }
        (*my_env)->SetByteArrayRegion(my_env, java_data, 0, data_doc_len, (const jbyte *) data_doc);
    }

    if (is_ack) {
        /* Invoke "ack" method */
        jmethodID ackMethodId = (*my_env)->GetMethodID(my_env, cbClass, "ack", "([B)V");
        if (ackMethodId == NULL) {
            WISHDEBUG(LOG_CRITICAL, "Cannot get ack method");

            monitor_exit();

            return;
        }
        /* FIXME: Should we release the monitor here, and re-acquire it after returning from the cb method?
         * Currently there should be no need, as the execution is serial due to WishBridge enqueuing execution of receive_core_to_app on the App's main thread. 
         * This applies to err and sig CBs also.
         * On the otherhand, if we release the monitor here, then we might improve the responsivess of the program..? */
        (*my_env)->CallVoidMethod(my_env, cb_obj, ackMethodId, java_data);
    } else if (is_sig) {
        /* Invoke "sig" method */
        jmethodID sigMethodId = (*my_env)->GetMethodID(my_env, cbClass, "sig", "([B)V");
        if (sigMethodId == NULL) {
            WISHDEBUG(LOG_CRITICAL, "Cannot get sig method");

            monitor_exit();

            return;
        }
        (*my_env)->CallVoidMethod(my_env, cb_obj, sigMethodId, java_data);

    } else if (is_err) {
        int code = 0;
        jobject java_msg = NULL;
        const char *msg = NULL;

        /* The response was an err. Get the code and msg fields from BSON */
        if (bson_find_from_buffer(&it, payload, "data") == BSON_OBJECT) {
            bson_iterator sit;
            bson bs;
            bson_init_with_data(&bs, payload);

            bson_iterator_subiterator(&it, &sit);
            if (bson_find_fieldpath_value("code", &sit) == BSON_INT) {
                code = bson_iterator_int(&sit);
            }
            bson_iterator_subiterator(&it, &sit);
            if (bson_find_fieldpath_value("msg", &sit) == BSON_STRING) {
                WISHDEBUG(LOG_CRITICAL, "msg is STRING");
                msg = bson_iterator_string(&sit);
            }
        }
        else {
            bson_find_from_buffer(&it, payload, "code");

            if (bson_iterator_type(&it) == BSON_INT) {
                code = bson_iterator_int(&it);
            } else {
                WISHDEBUG(LOG_CRITICAL, "err code is not of int type!");
            }


            bson_find_from_buffer(&it, payload, "msg");
            if (bson_iterator_type(&it) == BSON_STRING) {
                msg = bson_iterator_string(&it);
            }
        }

        if (msg != NULL) {
            /* Create a Java string from the msg */
            java_msg = (*my_env)->NewStringUTF(my_env, msg);
            if (java_msg == NULL) {
                WISHDEBUG(LOG_CRITICAL, "Failed to create err msg Java String!");
            }
        }

        WISHDEBUG(LOG_CRITICAL, "Request err code %i msg %s", code, msg);
        /* Invoke "err" method */
        jmethodID errMethodId = (*my_env)->GetMethodID(my_env, cbClass, "err", "(ILjava/lang/String;)V");
        if (errMethodId == NULL) {
            WISHDEBUG(LOG_CRITICAL, "Cannot get sig method");

            monitor_exit();

            return;
        }
        (*my_env)->CallVoidMethod(my_env, cb_obj, errMethodId, code, java_msg);
        if (java_msg != NULL) {
            (*my_env)->DeleteLocalRef(my_env, java_msg);
        }
    }

    /* Check if the any of the methods calls above threw an exception */
    check_and_report_exception(my_env);

    if (java_data != NULL) {
        (*my_env)->DeleteLocalRef(my_env, java_data);
    }

    if (is_ack || is_err) {
        (*my_env)->DeleteGlobalRef(my_env, cb_obj);
    }

    if (did_attach) {
        detachThread(javaVM);
    }


    monitor_exit();
}

/** Build the complete RPC request BSON from the Java arguments, and save the callback object. At this point it does not matter if we are making a Mist or Wish request */
static bool save_request_cb(JNIEnv *env, jobject jthis, bson *req_bs, jobject java_callback,  int req_id) {

    /* Alter the rpc is in the BSON-encoded RPC request */

    bson_iterator it;
    bson_find(&it, req_bs, "id");

    if (bson_iterator_type(&it) == BSON_INT) {
        bson_inplace_set_long(&it, req_id);
    } else {
        WISHDEBUG(LOG_CRITICAL, "Bad type for id (not int or inexistent)");
        return false;
    }

    /* Create Linked list entry */
    struct callback_list_elem *elem = calloc(sizeof (struct callback_list_elem), 1);
    if (elem == NULL) {
        WISHDEBUG(LOG_CRITICAL, "Cannot allocate memory");
        return false;
    }
    WISHDEBUG(LOG_CRITICAL, "Registering RPC id %i", req_id);
    /* Create a global reference of the callback object and save the object in the linked list of requests */
    elem->cb_obj = (*env)->NewGlobalRef(env, java_callback);
    elem->id = req_id;
    /* Note: No need to enter critical section here, because we are already owner of the RawApi monitor, because we have
    entered via a "synchronized" native method. */
    LL_APPEND(cb_list_head, elem);
    return true;
}


static int next_id = 1;


/*
 * Class:     mistApi_MistApi
 * Method:    request
 * Signature: ([BLmistApi/MistApi/RequestCb;)I
 */
JNIEXPORT jint JNICALL Java_mist_api_MistApi_request(JNIEnv *env, jobject jthis, jbyteArray java_req_bson, jobject java_callback) {
    WISHDEBUG(LOG_CRITICAL, "in mistApiRequest JNI");

    if (mist_api == NULL) {
        android_wish_printf("MistAPI not initted - Request fails");
        return 0;
    }

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return 0;
    }

    size_t req_buf_length = 0;
    uint8_t *req_buf = 0;

    if (java_req_bson != NULL) {
        /* Extract the byte array containing the RPC args */
        req_buf_length = (*env)->GetArrayLength(env, java_req_bson);
        req_buf = (uint8_t *) calloc(req_buf_length, 1);
        if (req_buf == NULL) {
            android_wish_printf("Malloc fails");

            monitor_exit();

            return 0;
        }
        (*env)->GetByteArrayRegion(env, java_req_bson, 0, req_buf_length, req_buf);
    } else {
        /* java args bson is null - fill in an empty array */
        WISHDEBUG(LOG_CRITICAL, "RPC req is null!");

        monitor_exit();

        return 0;
    }

    /* Check RPC id validity */
    if (next_id == 0) {
        next_id++;
    }

    int id = next_id;

    bson bs;
    bson_init_with_data(&bs, req_buf);
    if (save_request_cb(env, jthis, &bs, java_callback, id) == false) {
        WISHDEBUG(LOG_CRITICAL, "Failed to create Mist request");

        monitor_exit();

        return 0;
    }
    else {
        /* Request successfully sent, increment the id */
        next_id++;
    }

    bson_visit("Here is the request: ", bson_data(&bs));
    /* Call the Mist RPC API */
    mist_api_request(mist_api, &bs, generic_callback);

    free(req_buf);

    monitor_exit();

    return id;
}

/*
 * Class:     mistApi_MistApi
 * Method:    requestCancel
 * Signature: (I)I
 */
JNIEXPORT void JNICALL Java_mist_api_MistApi_requestCancel(JNIEnv *env, jobject jthis, jint id) {

    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return;
    }

    mist_api_request_cancel(mist_api, id);
    
    struct callback_list_elem *elem = NULL;
    struct callback_list_elem *tmp = NULL;
    
    LL_FOREACH_SAFE(cb_list_head, elem, tmp) {
        if (elem->id == id) {
            LL_DELETE(cb_list_head, elem);
            
            /* Delete the global ref for cb object*/
            if (elem->cb_obj != NULL) {
                (*env)->DeleteGlobalRef(env, elem->cb_obj);
            }
            /* Free the memory for list element */
            free(elem);
        }
    }

    monitor_exit();
}

/*
 * Class:     mistApi_MistApi
 * Method:    sandboxedRequest
 * Signature: ([B[BLmistApi/MistApi/RequestCb;)I
 */
JNIEXPORT jint JNICALL Java_mist_api_MistApi_sandboxedRequest(JNIEnv *env, jobject jthis, jbyteArray sandbox, jbyteArray java_req_bson, jobject java_callback) {
    WISHDEBUG(LOG_CRITICAL, "in sandboxedApiRequest JNI");

    if (mist_api == NULL) {
        android_wish_printf("MistAPI not initted - Request fails");
        return 0;
    }


    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return 0;
    }

    size_t req_buf_length = 0;
    uint8_t *req_buf = 0;

    if (java_req_bson != NULL) {
        /* Extract the byte array containing the RPC args */
        req_buf_length = (*env)->GetArrayLength(env, java_req_bson);
        req_buf = (uint8_t *) calloc(req_buf_length, 1);
        if (req_buf == NULL) {
            android_wish_printf("Malloc fails");

            monitor_exit();

            return 0;
        }
        (*env)->GetByteArrayRegion(env, java_req_bson, 0, req_buf_length, req_buf);
    } else {
        /* java args bson is null - fill in an empty array */
        WISHDEBUG(LOG_CRITICAL, "RPC req is null!");

        monitor_exit();

        return 0;
    }

    /* Check RPC id validity */
    if (next_id == 0) {
        next_id++;
    }

    int id = next_id;

    bson bs;
    bson_init_with_data(&bs, req_buf);

    if (save_request_cb(env, jthis, &bs, java_callback, id) == false) {
        WISHDEBUG(LOG_CRITICAL, "Failed to create Mist request");

        monitor_exit();

        return 0;
    }

    uint8_t* args;

    if (sandbox != NULL) {
        /* Extract the byte array containing the RPC args */
        int args_length = (*env)->GetArrayLength(env, sandbox);

        if (args_length != 32) {
            /* sandbox id is not valid */
            WISHDEBUG(LOG_CRITICAL, "Invalid sandbox id, not 32 bytes long, but %i", args_length);

            monitor_exit();

            return 0;
        }

        args = (uint8_t *) calloc(args_length, 1);
        if (args == NULL) {
            android_wish_printf("Malloc fails");

            monitor_exit();

            return 0;
        }
        (*env)->GetByteArrayRegion(env, sandbox, 0, args_length, args);
    } else {
        /* java args bson is null - fill in an empty array */
        WISHDEBUG(LOG_CRITICAL, "RPC args is null!");

        monitor_exit();

        return 0;
    }

    /* Request successful, increment the id */
    next_id++;

    WISHDEBUG(LOG_CRITICAL, "Sandbox id in raw api:  %02x %02x %02x %02x", args[0], args[1], args[2], args[3]);
    /* Call the Mist RPC API */
    sandboxed_api_request_context(mist_api, args, &bs, generic_callback, NULL);

    /* Clean-up */
    bson_destroy(&bs);

    monitor_exit();

    return id;
}

/*
 * Class:     mistApi_MistApi
 * Method:    sandboxedRequestCancel
 * Signature: ([BI)I
 */
JNIEXPORT void JNICALL Java_mist_api_MistApi_sandboxedRequestCancel(JNIEnv *env, jobject jthis, jbyteArray sandbox, jint id) {
    
    if (monitor_enter() != 0) {
        android_wish_printf("Could not lock monitor");
        return;
    }

    sandboxed_api_request_cancel(mist_api, sandbox, id);
    
    struct callback_list_elem *elem = NULL;
    struct callback_list_elem *tmp = NULL;
    
    LL_FOREACH_SAFE(cb_list_head, elem, tmp) {
        if (elem->id == id) {
            LL_DELETE(cb_list_head, elem);
            
            /* Delete the global ref for cb object*/
            if (elem->cb_obj != NULL) {
                (*env)->DeleteGlobalRef(env, elem->cb_obj);
            }
            /* Free the memory for list element */
            free(elem);
        }
    }
 
    monitor_exit();
}

