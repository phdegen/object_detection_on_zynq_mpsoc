#!/bin/bash 

gcc -c load_model.c -o load_model.o

gcc -c image_utils.c -o image_utils.o
gcc -c run_network.c -o run_network.o
gcc -c eval_results.c -o eval_results.o
gcc -c finalise_output.c -o finalise_output.o

gcc run.c load_model.o image_utils.o run_network.o eval_results.o finalise_output.o -lm -o test.o





