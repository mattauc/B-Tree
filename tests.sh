#!/bin/bash

echo "##########################"
echo "### Running e2e tests! ###"
echo "##########################"
count=0 # number of test cases run so far


for folder in 'my_tests/'*; do 
    name=$(basename "$folder")

    
    echo Running test: $name

    arg_file=my_tests/$name/$name.args
    expected_file=my_tests/$name/$name.out

    xargs -a $arg_file ./tests.o | diff - $expected_file

    count=$((count+1))
done

echo "
Finished running $count tests!"