// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// javac msgmgr.java -h .
// gcc -fPIC -shared -I/usr/lib/jvm/default/include/ -I/usr/lib/jvm/default/include/linux -o libmsgmgr.jni.so msgmgr.jni.c
public class msgmgr {

  static { System.loadLibrary("msgmgr.jni"); }

}
