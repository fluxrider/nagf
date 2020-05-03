// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// javac client.java -h .
// gcc -shared -I/usr/lib/jvm/default/include/ -I/usr/lib/jvm/default/include/linux -o libjava.jni.so java.jni.c -lrt -pthread
// java -Djava.library.path=$(pwd) client
public class client {

  public native static Object msgmgr_connect(String name, long length); // if Object is a String, it's an error message
  public native static java.nio.ByteBuffer msgmgr_get_mem(Object msgmgr);
  public native static String msgmgr_disconnect(Object msgmgr);
  public native static String msgmgr_lock(Object msgmgr);
  public native static String msgmgr_unlock(Object msgmgr);
  public native static String msgmgr_post(Object msgmgr);
  public native static String msgmgr_wait(Object msgmgr);

  public static void main(String [] args) {
    System.loadLibrary("java.jni");
    String error;

    Object msgmgr = msgmgr_connect("/prototype-server", 1024); if(msgmgr instanceof String) throw new RuntimeException((String)msgmgr);
    java.nio.ByteBuffer msg = msgmgr_get_mem(msgmgr);
    
    if((error = msgmgr_lock(msgmgr)) != null) throw new RuntimeException(error);
    msg.put(0, (byte)42);
    if((error = msgmgr_post(msgmgr)) != null) throw new RuntimeException(error);
    if((error = msgmgr_wait(msgmgr)) != null) throw new RuntimeException(error);
    System.out.println("Received: " + msg.get(0));
    if((error = msgmgr_unlock(msgmgr)) != null) throw new RuntimeException(error);
    
    if((error = msgmgr_disconnect(msgmgr)) != null) throw new RuntimeException(error);
  }

}
