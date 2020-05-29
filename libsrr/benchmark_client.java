// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// javac benchmark_client.java
// LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) benchmark_client

import java.nio.*;
import java.util.*;

class benchmark_client {

  public static void main(String [] _args) {
    List<String> args = Arrays.asList(_args);
    int seconds = Integer.parseInt(_args[0]);
    boolean bigmsg = args.contains("bigmsg");
    boolean multi = args.contains("multi");
    Random random = new Random();

    try(Jsrr srr = new Jsrr("/benchmark-srr", 8192, false, multi, 3)) {
      IntBuffer i = srr.msg.order(ByteOrder.LITTLE_ENDIAN).asIntBuffer();
      long t0 = System.currentTimeMillis();
      int count = 0;
      while(System.currentTimeMillis() < t0 + seconds*1000) {
        int x = random.nextInt​(10000);
        i.put(0, x);
        srr.send(bigmsg? 8192 : 4);
        if(i.get(0) != x + 5) throw new RuntimeException("Bad answer");
        count++;
      }
      System.out.println(count / seconds);
    } catch(Exception e) { e.printStackTrace(); }
  }

}
