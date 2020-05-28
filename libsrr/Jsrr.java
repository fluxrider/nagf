// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// javac Jsrr.java -h .
// gcc -shared -I/usr/lib/jvm/default/include/ -I/usr/lib/jvm/default/include/linux -o libsrrjni.so srr.jni.c -lrt -pthread -L. -lsrr

import java.nio.*;
import java.nio.charset.*;

public class Jsrr implements AutoCloseable {

  public ByteBuffer msg;

  private IntBuffer length;
  private Object opaque;
  private native static Object init(String name, int length, boolean is_server, boolean use_multi_client_lock, double timeout);
  private native static String close(Object opaque);
  private native static ByteBuffer get_msg_ptr(Object opaque);
  private native static ByteBuffer get_length_ptr(Object opaque);
  private native static String send(Object opaque, int length);
  private native static String receive(Object opaque);
  private native static String reply(Object opaque, int length);


  public Jsrr(String name, int length, boolean is_server, boolean use_multi_client_lock, double timeout) {
    //if(!is_server) send_dx_buffer = ByteBuffer.allocate(length);
    //this.direct = is_server || !use_multi_client_lock;
    this.opaque = Jsrr.init(name, length, is_server, use_multi_client_lock, timeout);
    if(this.opaque instanceof String) throw new RuntimeException(this.opaque.toString());
    this.msg = Jsrr.get_msg_ptr(this.opaque);
    this.length = Jsrr.get_length_ptr(this.opaque).asIntBuffer();
  }

  public void close() throws Exception {
    String error = Jsrr.close(this.opaque); if(error != null) throw new RuntimeException(error);
  }

  public int _send(int length) {
    // it is assumed that this.msg was filled up to length by caller
    String error = Jsrr.send(this.opaque, length); if(error != null) throw new RuntimeException(error);
    return this.length.get(0);
    // caller can now read this.msg up to the length return
  }

  public int _receive() {
    String error = Jsrr.receive(this.opaque); if(error != null) throw new RuntimeException(error);
    return this.length.get(0);
    // caller can now read this.msg up to the length return
  }

  public void _reply(int length) {
    // it is assumed that this.msg was filled up to length by caller
    String error = Jsrr.reply(this.opaque, length); if(error != null) throw new RuntimeException(error);
  }
 
  // convenience String method (that avoid intermediate buffers the best I can)
  public int send(CharSequence s) {
    this.write_string(s);
    return this._send(this.msg.position());
  }
  
  public void reply(CharSequence s) {
    this.write_string(s);
    this._reply(this.msg.position());
  }
  
  public CharSequence as_string(int length) {
    this.msg.position(0);
    this.msg.limit(length);
    CharSequence s = StandardCharsets.UTF_8.decode​(this.msg);
    this.msg.limit(this.msg.capacity());
    return s;
  }

  private static CharsetEncoder encoder = StandardCharsets.UTF_8.newEncoder();
  private void write_string(CharSequence s) {
    this.msg.position(0);
    Jsrr.encoder.encode(CharBuffer.wrap​(s), this.msg, true);
  }

}
