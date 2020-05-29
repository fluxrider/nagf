// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// javac benchmark_server.java
// LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) benchmark_server

import java.nio.*;

class benchmark_server {

  public static void main(String [] args) {
    try(Jsrr srr = new Jsrr("/benchmark-srr", 8192, true, false, 2)) {
      IntBuffer i = srr.msg.order(ByteOrder.LITTLE_ENDIAN).asIntBuffer();
      while(true) {
        srr.receive();
        i.put(0, i.get(0) + 5);
        srr.reply(4);
      }
    } catch(Exception e) { e.printStackTrace(); }
  }

}
