#！/bin/bash

gnome-terminal -t “kvstore server2” -x bash -c “./kvstoresystem 2 localhost:8002;exec bash;”
gnome-terminal -t “kvstore server3” -x bash -c “./kvstoresystem 3 localhost:8003;exec bash;”
gnome-terminal -t “kvstore server4” -x bash -c “./kvstoresystem 4 localhost:8004;exec bash;”
sleep 1s
gnome-terminal -t “kvstore server1” -x bash -c “./kvstoresystem 1 localhost:8001 --config-file server.conf;exec bash;”
sleep 1s
gnome-terminal -t “client” -x bash -c “./client 4 server.conf;exec bash;”