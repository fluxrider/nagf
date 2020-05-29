// Copyright 2020 David Lareau. This program is free software under the terms of the GPL-3.0-or-later, no warranty.
// javac example_server.java
// LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) example_server

class example_server {

  public static void main(String [] args) {
    try(srr srr = new srr("/example-srr", 8192, true, false, 3)) {
      System.out.println(srr.as_string(srr.receive()));
      srr.reply("whatever");
    } catch(Exception e) { e.printStackTrace(); }
  }

}
