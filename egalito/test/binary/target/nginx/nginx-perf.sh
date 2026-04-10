#!/bin/sh
ulimit

N=3
file=${1:-index.html}
echo "=== nginx.sh ==="
counter=0
while [ $counter -lt $N ]; do
  ./nginx.sh $file && cat tmp/nginx-wrk.out
  counter=$((counter+1))
  while $(netstat | grep -q 'TIME_WAIT'); do
    sleep 1
  done
  echo $(netstat | grep -q 'TIME_WAIT')
done

echo "=== nginx-thread.sh ==="
counter=0
while [ $counter -lt $N ]; do
  ./nginx-thread.sh $file && cat tmp/nginx-thread-wrk.out
  counter=$((counter+1))
  while $(netstat | grep -q 'TIME_WAIT'); do
    sleep 1
  done
  echo $(netstat | grep -q 'TIME_WAIT')
done
