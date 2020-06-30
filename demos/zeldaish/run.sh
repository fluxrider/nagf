#!/usr/bin/env bash
set -e
trap 'echo SIGINT unblocks this script, but it is recommended to let it finish' SIGINT
demo_name=${PWD##*/} # basename $PWD

echo '---- compile libsrr ----'
pushd ../../libsrr
gcc -fPIC -shared -o libsrr.so srr.c backend_shm.c -lrt -pthread
#gcc -fPIC -shared -I/usr/lib/jvm/default/include/ -I/usr/lib/jvm/default/include/linux -o libsrrjni.so srr.jni.c -lrt -pthread -L. -lsrr
#javac -classpath .. srr.java
popd

echo '---- compile $demo_name ----'
gcc -o $demo_name *.c ../../utils/*.c -L../../libsrr -I../../libsrr -I../../utils -lsrr $(pkg-config --libs --cflags libxml-2.0) -lm

echo '---- compile servers ----'
pushd ../../servers
#javac -classpath .. gfx-swing.java
xxd -i < gfx-glfw_freetype-gl/v3f-t2f-c4f.vert > text.vert.xxd && xxd -i < gfx-glfw_freetype-gl/v3f-t2f-c4f.frag > text.frag.xxd && xxd -i < gfx-glfw.img.vert > img.vert.xxd && xxd -i < gfx-glfw.img.frag > img.frag.xxd && gcc -o gfx-glfw gfx-glfw.c gfx-glfw_freetype-gl/*.c -L../libsrr -I../libsrr -I../utils -lsrr $(pkg-config --libs --cflags x11 opengl glfw3 glew freetype2 MagickWand) -lpthread -lm && rm *.xxd
popd

echo '---- launch servers ----'
set +e
PYTHONPATH=../.. LD_LIBRARY_PATH=../../libsrr python -B ../../servers/snd-gstreamer.py &
snd_pid=$!
PYTHONPATH=../.. LD_LIBRARY_PATH=../../libsrr python -B ../../servers/evt-libevdev.py /$demo_name-evt ../../example.map &
evt_pid=$!
#mkfifo gfx.fifo
#LD_LIBRARY_PATH=../../libsrr java -cp ../..:../../servers -Djava.library.path=$(pwd)/../../libsrr gfx_swing /$demo_name-gfx &
LD_LIBRARY_PATH=../../libsrr ../../servers/gfx-glfw /$demo_name-gfx &
gfx_pid=$!
sleep .5

echo '---- launch $demo_name ----'
LD_LIBRARY_PATH=../../libsrr ./$demo_name &
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
rm $demo_name
pushd ../..
find . -name "*.class" -type f -delete
find . -name "*.so" -type f -delete
popd
trap SIGINT
