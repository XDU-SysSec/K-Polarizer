#!/bin/bash

filename=${1:-index.html}
source wrkparam.sh

command -v wrk > /dev/null 2>&1 || { echo >&2 "needs wrk -- skipping"; exit 0; }

mkdir -p tmp

pidfile=./nginx/logs/nginx.pid

./nginx/sbin/nginx -c ../conf/nginx.conf >tmp/nginx.out 2>& 1 &

count=50
while [ ! -f $pidfile ]; do
  sleep 1;
  count=$((count-1))
  if [[ "$count" -eq 0 ]]; then
    echo "test failed";
    exit 1;
  fi
done
#cat $pidfile

wrk $wrkparam http://localhost:8000/$filename > tmp/nginx-wrk.out

kill -QUIT $( cat $pidfile )

count=50
while [ -f $pidfile ]; do
  sleep 1;
  if [[ "$count" -eq 0 ]]; then
    echo "test failed";
    exit 1;
  fi
done

#cat tmp/nginx-wrk.out
