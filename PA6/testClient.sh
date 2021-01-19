#!/bin/bash

echo "Varying w, ./client -p 10 -n 15000 with"

for num in 50 75 100 125 150 175 200 225 250 500 750 1000 1250 1500
do
    printf "$num worker channels: "
    ./client -h 192.168.1.15 -r 9890 -p 10 -n 15000 -w $num | grep Took
done
