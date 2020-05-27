#!/usr/bin/env bash
set -e
gcc -fPIC -shared -o libsrr.so srr.c backend_shm.c -lrt -pthread
gcc -fPIC -shared -o libsrrshm.so backend_shm.c -lrt -pthread
gcc -o benchmark_server benchmark_server.c -L. -lsrr
gcc -o benchmark_client benchmark_client.c -L. -lsrr

duration=30

# C server
echo 'Launch C server'
LD_LIBRARY_PATH=. ./benchmark_server &
server_pid=$!
sleep .4
echo 'Launch C client (ignore this one)'
LD_LIBRARY_PATH=. ./benchmark_client $duration
echo 'Launch C client'
LD_LIBRARY_PATH=. ./benchmark_client $duration
echo 'Launch python client (cost of implementation overhead)'
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration
echo 'Launch python client with bigmsg (cost of implementation memcpy overhead)'
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration bigmsg
echo 'Launch C client with mutex (cost of locks by myself)'
LD_LIBRARY_PATH=. ./benchmark_client $duration multi
echo 'Launch C client with mutex/bigmsg (cost of memcpy)'
LD_LIBRARY_PATH=. ./benchmark_client $duration multi bigmsg
echo 'Launch 3 C clients with mutex (cost of locks for real)'
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
echo 'Launch C client (ignore this one)'
LD_LIBRARY_PATH=. ./benchmark_client $duration
echo 'Launch C client'
LD_LIBRARY_PATH=. ./benchmark_client $duration
echo 'Launch python client (cost of implementation overhead)'
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration
echo 'Launch python client with bigmsg (cost of implementation memcpy overhead)'
LD_LIBRARY_PATH=. python -B benchmark_client.py $duration bigmsg
echo 'Launch C client with mutex (cost of locks by myself)'
LD_LIBRARY_PATH=. ./benchmark_client $duration multi
echo 'Launch C client with mutex/bigmsg (cost of memcpy)'
LD_LIBRARY_PATH=. ./benchmark_client $duration multi bigmsg
echo 'Launch 3 C clients with mutex (cost of locks for real)'
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

# computer details
uname -a
cat /proc/cpuinfo
