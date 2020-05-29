// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// javac srr.java -h gen_header && mv gen_header/libsrr_srr.h srr.jni.h && rm -Rf gen_header
// gcc -fPIC -shared -I/usr/lib/jvm/default/include/ -I/usr/lib/jvm/default/include/linux -o libsrrjni.so srr.jni.c -lrt -pthread -L. -lsrr

package libsrr;
import java.nio.*;
import java.nio.charset.*;

public class srr implements AutoCloseable {

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
  static { System.loadLibrary("srrjni"); }

  public srr(String name, int length, boolean is_server, boolean use_multi_client_lock, double timeout) {
    //if(!is_server) send_dx_buffer = ByteBuffer.allocate(length);
    //this.direct = is_server || !use_multi_client_lock;
    this.opaque = srr.init(name, length, is_server, use_multi_client_lock, timeout);
    if(this.opaque instanceof String) throw new RuntimeException(this.opaque.toString());
    this.msg = srr.get_msg_ptr(this.opaque);
    this.length = srr.get_length_ptr(this.opaque).order(ByteOrder.LITTLE_ENDIAN).asIntBuffer();
  }

  public void close() {
    String error = srr.close(this.opaque); if(error != null) throw new RuntimeException(error);
  }

  public int send(int length) {
    // it is assumed that this.msg was filled up to length by caller
    String error = srr.send(this.opaque, length); if(error != null) throw new RuntimeException(error);
    return this.length.get(0);
    // caller can now read this.msg up to the length return
  }

  public int receive() {
    String error = srr.receive(this.opaque); if(error != null) throw new RuntimeException(error);
    return this.length.get(0);
    // caller can now read this.msg up to the length return
  }

  public void reply(int length) {
    // it is assumed that this.msg was filled up to length by caller
    String error = srr.reply(this.opaque, length); if(error != null) throw new RuntimeException(error);
  }
 
  // convenience String method (that avoid intermediate buffers the best I can)
  public int send(CharSequence s) {
    this.write_string(s);
    return this.send(this.msg.position());
  }
  
  public void reply(CharSequence s) {
    this.write_string(s);
    this.reply(this.msg.position());
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
    srr.encoder.encode(CharBuffer.wrap​(s), this.msg, true);
  }

}
