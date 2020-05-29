// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// javac -classpath .. example_client.java
// LD_LIBRARY_PATH=. java -cp .:.. -Djava.library.path=$(pwd) example_client

import libsrr.*;

class example_client {

  public static void main(String [] args) {
    try(srr srr = new srr("/example-srr", 8192, false, false, 3)) {
      System.out.println(srr.as_string(srr.send("hello")));
    } catch(Exception e) { e.printStackTrace(); }
  }

}
