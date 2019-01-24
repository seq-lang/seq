#!/usr/bin/env bash

export LD_LIBRARY_PATH=. 
export SEQ_PATH=../stdlib 
unset SEQ_DEBUG
# export SEQ_DEBUG=1

for PORT in 8000 8001 8002 8003 8004
do
	p=$(lsof -t -i :${PORT}) 
	if [ ! -z "$p" ] 
	then
		echo "clearing process $p occupying port ${PORT}..."
		kill -kill "$p"
	fi
done

(SEQ_MPC_CP=1 ./main.exe $1 2>&1 | sed "s/^/[CP1] /" | sed "s,.*,$(tput setaf 1)&$(tput sgr0),") &
pid1=$!
(SEQ_MPC_CP=2 ./main.exe $1 2>&1 | sed "s/^/[CP2] /" | sed "s,.*,$(tput setaf 2)&$(tput sgr0),") &
pid2=$!

# SEQ_MPC_CP=0 lldb -o run ./main.exe -- $1
SEQ_MPC_CP=0 ./main.exe $1 2>&1 | sed "s/^/[CP0] /"
echo "Done! $?" 

trap "kill ${pid1} ${pid2}; exit 1" INT
wait
