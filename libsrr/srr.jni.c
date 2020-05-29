// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// gcc -fPIC -shared -I/usr/lib/jvm/default/include/ -I/usr/lib/jvm/default/include/linux -o libsrrjni.so srr.jni.c -lrt -pthread -L. -lsrr
#include "srr.h"
#include "srr.jni.h"
#include <stdbool.h>

struct opaque_data {
  struct srr srr;
  struct srr_direct * mem;
  bool is_indirect;
};

JNIEXPORT jobject JNICALL Java_libsrr_srr_init(JNIEnv * env, jclass _c, jstring jstring_name, jint length, jboolean is_server, jboolean use_multi_client_lock, jdouble timeout) {
  struct opaque_data * data = malloc(sizeof(struct opaque_data));
  const char * name = (*env)->GetStringUTFChars(env, jstring_name, NULL);
  const char * error = srr_init(&data->srr, name, length, is_server, use_multi_client_lock, timeout);
  (*env)->ReleaseStringUTFChars(env, jstring_name, name);
  if(error) return (*env)->NewStringUTF(env, error);
  data->is_indirect = !is_server && use_multi_client_lock;
  data->mem = data->is_indirect? malloc(sizeof(struct srr_direct) + length) : srr_direct(&data->srr);
  return (*env)->NewDirectByteBuffer(env, data, sizeof(struct opaque_data));
}

JNIEXPORT jstring JNICALL Java_libsrr_srr_close(JNIEnv * env, jclass _c, jobject o) {
  struct opaque_data * data = (*env)->GetDirectBufferAddress(env, o);
  const char * error = srr_disconnect(&data->srr);
  if(data->is_indirect) free(data->mem);
  return error? (*env)->NewStringUTF(env, error) : NULL;
}

JNIEXPORT jobject JNICALL Java_libsrr_srr_get_1msg_1ptr(JNIEnv * env, jclass _c, jobject o) {
  struct opaque_data * data = (*env)->GetDirectBufferAddress(env, o);
  return (*env)->NewDirectByteBuffer(env, data->mem->msg, data->srr.length);
}

JNIEXPORT jobject JNICALL Java_libsrr_srr_get_1length_1ptr(JNIEnv * env, jclass _c, jobject o) {
  struct opaque_data * data = (*env)->GetDirectBufferAddress(env, o);
  return (*env)->NewDirectByteBuffer(env, &data->mem->length, 4);
}

JNIEXPORT jstring JNICALL Java_libsrr_srr_send(JNIEnv * env, jclass _c, jobject o, jint length) {
  struct opaque_data * data = (*env)->GetDirectBufferAddress(env, o);
  const char * error = srr_send_dx(&data->srr, data->mem->msg, length, data->mem->msg, &data->mem->length);
  return error? (*env)->NewStringUTF(env, error) : NULL;
}

JNIEXPORT jstring JNICALL Java_libsrr_srr_receive(JNIEnv * env, jclass _c, jobject o) {
  struct opaque_data * data = (*env)->GetDirectBufferAddress(env, o);
  const char * error = srr_receive(&data->srr);
  return error? (*env)->NewStringUTF(env, error) : NULL;
}

JNIEXPORT jstring JNICALL Java_libsrr_srr_reply(JNIEnv * env, jclass _c, jobject o, jint length) {
  struct opaque_data * data = (*env)->GetDirectBufferAddress(env, o);
  const char * error = srr_reply(&data->srr, length);
  return error? (*env)->NewStringUTF(env, error) : NULL;
}
