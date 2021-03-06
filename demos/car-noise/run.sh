#!/usr/bin/env bash
# usage: if there is a $1, then use gfx-swing.java/evt-libevdev.py instead of combo-gfx-evt-glfw.c
set -e
trap 'echo SIGINT unblocks this script, but it is recommended to let it finish' SIGINT
demo_name=${PWD##*/} # basename $PWD

echo '---- compile libsrr ----'
pushd ../../libsrr
gcc -fPIC -shared -o libsrr.so srr.c backend_shm.c -lrt -pthread
if [[ -n $1 ]]; then
  gcc -fPIC -shared -I/usr/lib/jvm/default/include/ -I/usr/lib/jvm/default/include/linux -o libsrrjni.so srr.jni.c -lrt -pthread -L. -lsrr
  javac -classpath .. srr.java
fi
popd

echo '---- compile servers ----'
pushd ../../servers
if [[ -n $1 ]]; then
  javac -classpath .. gfx-swing.java
else
  xxd -i < gfx-glfw.font.vert > font.vert.xxd
  xxd -i < gfx-glfw.font.frag > font.frag.xxd
  xxd -i < gfx-glfw.img.vert > img.vert.xxd
  xxd -i < gfx-glfw.img.frag > img.frag.xxd
  xxd -i < gfx-glfw.fill.vert > fill.vert.xxd
  xxd -i < gfx-glfw.fill.frag > fill.frag.xxd
  gcc -o gfx-glfw gfx-glfw.c gfx-glfw_freetype-gl/*.c ../utils/*.c -L../libsrr -I../libsrr -I../utils -lsrr $(pkg-config --libs --cflags x11 opengl glfw3 glew freetype2 MagickWand) -lpthread -lm
  rm *.xxd
fi
popd

echo '---- launch servers ----'
set +e
PYTHONPATH=../.. LD_LIBRARY_PATH=../../libsrr python -B ../../servers/snd-gstreamer.py &
snd_pid=$!
if [[ -n $1 ]]; then
  PYTHONPATH=../.. LD_LIBRARY_PATH=../../libsrr python -B ../../servers/evt-libevdev.py /$demo_name-evt ../../example.map &
  evt_pid=$!
  mkfifo gfx.fifo
  LD_LIBRARY_PATH=../../libsrr java -cp ../..:../../servers -Djava.library.path=$(pwd)/../../libsrr gfx_swing /$demo_name-gfx &
  gfx_pid=$!
else
  ../../tools/fetch_controllerdb.sh > ../../gamecontrollerdb.txt
  LD_LIBRARY_PATH=../../libsrr ../../servers/gfx-glfw /$demo_name-gfx /$demo_name-evt ../../gamecontrollerdb.txt &
  gfx_pid=$!
  evt_pid=$gfx_pid
fi
sleep 1

echo '---- launch $demo_name ----'
PYTHONPATH=../.. LD_LIBRARY_PATH=../../libsrr python -B ./$demo_name.py &
demo_pid=$!

echo '---- wait until any of the launched process terminates ----'
echo "snd $snd_pid"
echo "evt $evt_pid"
echo "gfx $gfx_pid"
echo "demo $demo_pid"
wait -n
echo '---- kill servers ----'
kill $snd_pid 2> /dev/null
kill $evt_pid 2> /dev/null
kill $gfx_pid 2> /dev/null
if kill -0 $demo_pid 2> /dev/null; then
  echo '---- kill game (in 5 seconds) ----'
  t=5
  while : ; do
      sleep 1
      let t--
      if ! kill -0 $demo_pid 2> /dev/null; then
        echo 'it stopped by itself'
        break
      fi
      if [ "$t" -eq "0" ]; then
        echo 'killing it'
        kill $demo_pid 2> /dev/null
        break
      fi
  done
fi

echo '---- cleanup ----'
rm *.fifo
rm /dev/shm/$demo_name*
pushd ../..
find . -name "*.class" -type f -delete
find . -name "*.so" -type f -delete
popd
trap SIGINT
