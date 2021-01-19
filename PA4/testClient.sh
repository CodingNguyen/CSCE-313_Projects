#!/bin/bash

echo "Varying w, ./client -p 10 -n 15000 with"

for num in 50 75 100 125 150 175 200 225 250 500 750 1000 1250 1500
do
    printf "$num worker channels: "
    ./client -p 10 -n 15000 -w $num | grep Took
done

echo " "
echo "Varying b, ./client -p 10 -n 15000 with"
for num in 50 100 150 200 250 500 600 700 800 900 1000 1100 1200
do
    printf "$num size buffer: "
    ./client -p 10 -n 15000 -b $num | grep Took
done

echo " "
echo "Varying n, ./client -p 10 with"
for num in 50 100 200 300 400 500 1000 1500 2000 3000 5000 7000 9000 10000 12000 15000
do
    printf "$num data points: "
    ./client -p 10 -n $num | grep Took
done
