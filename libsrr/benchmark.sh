#!/usr/bin/env bash
set -e
gcc -fPIC -shared -o libsrr.so srr.c backend_shm.c -lrt -pthread
gcc -o benchmark_server benchmark_server.c -L. -lsrr
gcc -o benchmark_client benchmark_client.c -L. -lsrr
gcc -fPIC -shared -I/usr/lib/jvm/default/include/ -I/usr/lib/jvm/default/include/linux -o libsrrjni.so srr.jni.c -lrt -pthread -L. -lsrr
javac benchmark_server.java
javac benchmark_client.java

duration=10
echo 'Each test will take' $duration 'seconds.'
echo 'The higher numbers the better.'

# C server
echo 'Launch C server'
LD_LIBRARY_PATH=. ./benchmark_server &
server_pid=$!
sleep .4
echo -n 'C                       : '
LD_LIBRARY_PATH=. ./benchmark_client $duration
echo -n 'python                  : '
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration
echo -n 'java                    : '
LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) benchmark_client $duration
echo -n 'C      w/ mutex         : '
LD_LIBRARY_PATH=. ./benchmark_client $duration multi
echo -n 'python w/ mutex         : '
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration multi
echo -n 'java   w/ mutex         : '
LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) benchmark_client $duration multi
echo -n 'C      w/ mutex, bigmsg : '
LD_LIBRARY_PATH=. ./benchmark_client $duration multi bigmsg
echo -n 'python w/ mutex, bigmsg : '
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration multi bigmsg
echo -n 'java   w/ mutex, bigmsg : '
LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) benchmark_client $duration multi bigmsg
echo -n 'python w/ bigmsg        : '
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration bigmsg
echo -n 'java   w/ bigmsg        : '
LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) benchmark_client $duration bigmsg
echo -n '3 x C  w/ mutex         : '
LD_LIBRARY_PATH=. ./benchmark_client $duration multi &
client_pid=$!
LD_LIBRARY_PATH=. ./benchmark_client $duration multi &
client_pid_2=$!
LD_LIBRARY_PATH=. ./benchmark_client $duration multi &
client_pid_3=$!
wait $client_pid $client_pid_2 $client_pid_3
set +e
wait $server_pid
set -e

# python server
echo 'Launch python server'
LD_LIBRARY_PATH=. python -B benchmark_server.py &
server_pid=$!
sleep .4
echo -n 'C                       : '
LD_LIBRARY_PATH=. ./benchmark_client $duration
echo -n 'python                  : '
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration
echo -n 'java                    : '
LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) benchmark_client $duration
echo -n 'C      w/ mutex         : '
LD_LIBRARY_PATH=. ./benchmark_client $duration multi
echo -n 'python w/ mutex         : '
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration multi
echo -n 'java   w/ mutex         : '
LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) benchmark_client $duration multi
echo -n 'C      w/ mutex, bigmsg : '
LD_LIBRARY_PATH=. ./benchmark_client $duration multi bigmsg
echo -n 'python w/ mutex, bigmsg : '
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration multi bigmsg
echo -n 'java   w/ mutex, bigmsg : '
LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) benchmark_client $duration multi bigmsg
echo -n 'python w/ bigmsg        : '
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration bigmsg
echo -n 'java   w/ bigmsg        : '
LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) benchmark_client $duration bigmsg
echo -n '3 x C  w/ mutex         : '
LD_LIBRARY_PATH=. ./benchmark_client $duration multi &
client_pid=$!
LD_LIBRARY_PATH=. ./benchmark_client $duration multi &
client_pid_2=$!
LD_LIBRARY_PATH=. ./benchmark_client $duration multi &
client_pid_3=$!
wait $client_pid $client_pid_2 $client_pid_3
set +e
wait $server_pid
set -e

# java server
echo 'Launch java server'
LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) benchmark_server &
server_pid=$!
sleep .4
echo -n 'C                       : '
LD_LIBRARY_PATH=. ./benchmark_client $duration
echo -n 'python                  : '
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration
echo -n 'java                    : '
LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) benchmark_client $duration
echo -n 'C      w/ mutex         : '
LD_LIBRARY_PATH=. ./benchmark_client $duration multi
echo -n 'python w/ mutex         : '
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration multi
echo -n 'java   w/ mutex         : '
LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) benchmark_client $duration multi
echo -n 'C      w/ mutex, bigmsg : '
LD_LIBRARY_PATH=. ./benchmark_client $duration multi bigmsg
echo -n 'python w/ mutex, bigmsg : '
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration multi bigmsg
echo -n 'java   w/ mutex, bigmsg : '
LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) benchmark_client $duration multi bigmsg
echo -n 'python w/ bigmsg        : '
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration bigmsg
echo -n 'java   w/ bigmsg        : '
LD_LIBRARY_PATH=. java -Djava.library.path=$(pwd) benchmark_client $duration bigmsg
echo -n '3 x C  w/ mutex         : '
LD_LIBRARY_PATH=. ./benchmark_client $duration multi &
client_pid=$!
LD_LIBRARY_PATH=. ./benchmark_client $duration multi &
client_pid_2=$!
LD_LIBRARY_PATH=. ./benchmark_client $duration multi &
client_pid_3=$!
wait $client_pid $client_pid_2 $client_pid_3
set +e
wait $server_pid
set -e

# cleanup
rm benchmark_client
rm benchmark_server
rm benchmark_client.class
rm benchmark_server.class

# computer details
uname -a
lscpu
