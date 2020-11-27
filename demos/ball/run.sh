#!/usr/bin/env bash
set -e
trap 'echo SIGINT unblocks this script, but it is recommended to let it finish' SIGINT
game_name=${PWD##*/} # basename $PWD
NAGF_PATH=~/_/dev/nagf

echo '---- compile libsrr ----'
pushd $NAGF_PATH/libsrr
gcc -fPIC -shared -o libsrr.so srr.c backend_shm.c -lrt -pthread
popd

echo '---- compile servers ----'
pushd $NAGF_PATH/servers
xxd -i < gfx-glfw.font.vert > font.vert.xxd
xxd -i < gfx-glfw.font.frag > font.frag.xxd
xxd -i < gfx-glfw.img.vert > img.vert.xxd
xxd -i < gfx-glfw.img.frag > img.frag.xxd
xxd -i < gfx-glfw.fill.vert > fill.vert.xxd
xxd -i < gfx-glfw.fill.frag > fill.frag.xxd
gcc -o gfx-glfw gfx-glfw.c gfx-glfw_freetype-gl/*.c ../utils/*.c -L../libsrr -I../libsrr -I../utils -lsrr $(pkg-config --libs --cflags x11 glesv2 glfw3 glew freetype2 MagickWand) -lpthread -lm
rm *.xxd
popd

echo '---- compile game ----'

echo '---- launch servers ----'
set +e
PYTHONPATH=$NAGF_PATH LD_LIBRARY_PATH=$NAGF_PATH/libsrr python3 -B $NAGF_PATH/servers/snd-gstreamer.py &
snd_pid=$!
#$NAGF_PATH/tools/fetch_controllerdb.sh > $NAGF_PATH/gamecontrollerdb.txt
LD_LIBRARY_PATH=$NAGF_PATH/libsrr $NAGF_PATH/servers/gfx-glfw /$game_name-gfx /$game_name-evt $NAGF_PATH/gamecontrollerdb.txt &
gfx_pid=$!
evt_pid=$gfx_pid
sleep 1

echo '---- launch game ----'
PYTHONPATH=$NAGF_PATH LD_LIBRARY_PATH=$NAGF_PATH/libsrr python3 -B ./main.py &
game_pid=$!

echo '---- wait until any of the launched process terminates ----'
echo "snd $snd_pid"
echo "evt $evt_pid"
echo "gfx $gfx_pid"
echo "game $game_pid"
wait -n
echo '---- kill servers ----'
kill $snd_pid 2> /dev/null
kill $evt_pid 2> /dev/null
kill $gfx_pid 2> /dev/null
if kill -0 $game_pid 2> /dev/null; then
  echo '---- kill game (in 5 seconds) ----'
  t=5
  while : ; do
      sleep 1
      let t--
      if ! kill -0 $game_pid 2> /dev/null; then
        echo 'it stopped by itself'
        break
      fi
      if [ "$t" -eq "0" ]; then
        echo 'killing it'
        kill $game_pid 2> /dev/null
        break
      fi
  done
fi

echo '---- cleanup ----'
rm *.fifo
rm /dev/shm/$game_name*
pushd $NAGF_PATH
find . -name "*.class" -type f -delete
find . -name "*.so" -type f -delete
popd
trap SIGINT
