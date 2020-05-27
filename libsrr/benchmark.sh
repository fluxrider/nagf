#!/usr/bin/env bash
set -e
gcc -o benchmark_server benchmark_server.c -L. -lsrr
gcc -o benchmark_client benchmark_client.c -L. -lsrr

# C server
echo 'Launch C server'
LD_LIBRARY_PATH=. ./benchmark_server &
server_pid=$!
sleep .4
echo 'Launch C client (ignore this one)'
LD_LIBRARY_PATH=. ./benchmark_client
echo 'Launch C client'
LD_LIBRARY_PATH=. ./benchmark_client
echo 'Launch python client'
LD_LIBRARY_PATH=. python -B benchmark_client.py
echo 'Launch C client with mutex'
LD_LIBRARY_PATH=. ./benchmark_client multi
echo 'Launch 3 C clients with mutex'
LD_LIBRARY_PATH=. ./benchmark_client multi &
client_pid=$!
LD_LIBRARY_PATH=. ./benchmark_client multi &
client_pid_2=$!
LD_LIBRARY_PATH=. ./benchmark_client multi &
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
LD_LIBRARY_PATH=. ./benchmark_client
echo 'Launch C client'
LD_LIBRARY_PATH=. ./benchmark_client
echo 'Launch python client'
LD_LIBRARY_PATH=. python -B benchmark_client.py
echo 'Launch C client with mutex'
LD_LIBRARY_PATH=. ./benchmark_client multi
echo 'Launch 3 C clients with mutex'
LD_LIBRARY_PATH=. ./benchmark_client multi &
client_pid=$!
LD_LIBRARY_PATH=. ./benchmark_client multi &
client_pid_2=$!
LD_LIBRARY_PATH=. ./benchmark_client multi &
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
