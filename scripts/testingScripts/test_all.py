#!/usr/bin/python
# Run a LLVM bitcode (native execution)

import os, sys, string, commands;

benchmark_list = [
	# bench_name, func_name, loop_name_O0, loop_name_O1, loop_name_O2
	
	# Non-benchmarks
	#["toy", "walk", "bb1", "bb", "bb"], # OK
	#["ottoni", "walk", "bb4", "bb", "bb"], # OK
	#["ret", "walk", "bb3", "bb", "bb"], # OK
	
	# Small benchmarks
	["wc", "cnt", "bb51", "bb35", "bb35"], # needs to modify wc source to make -O1 work (because of buggy ExtractLoop)
	#["fir", "fir", "bb4", "bb", "bb"], # needs spiff
	["bmm", "matmult", "bb8", "bb5.preheader", "bb5.preheader"], # OK
	["olden_treeadd", "main", "bb1", "bb", "bb"], # OK
	
	# Real benchmarks
	["adpcmdec", "adpcm_decoder", "bb29", "bb", "bb"], # testing...
	# -O0: LE is OK, hangs for "mi_eval_input1"
	# -O1: LE is OK, wrong results for "", hangs for "mi_eval_input1"
	["adpcmenc", "adpcm_coder", "bb36", "bb", "bb"], # testing...
];

# Different execution mode for the input benchmark bitcode
EXECUTION_MODE_DIRECT = "direct";
EXECUTION_MODE_LOOP_EXTRACTION = "le";
EXECUTION_MODE_MTCG = "mtcg";

# Setup the parameters if arguments are OK
# Print help message if arguments are wrong
if(len(sys.argv) == 4):
	path = sys.argv[1];
	mode = sys.argv[2];
	opti = sys.argv[3];
else:
	print("Usage: test_all.py path mode opti");
	print("Example: test_all.py $HOME/temp mtcg -O0");
	sys.exit(1);
	
### Main routine ###
for benchmark in benchmark_list:
	bench_name = benchmark[0];
	func_name = benchmark[1];
	if(opti == "-O0"):
		loop_name = benchmark[2];
	elif(opti == "-O1"):
		loop_name = benchmark[3];
	elif(opti == "-O2"):
		loop_name = benchmark[4];
	cd_cmd = "cd " + path;
	cmd = cd_cmd + "; " + "test.py " + bench_name + " " + path + " " + mode + " " + func_name + " " + loop_name;
	print("Command: " + cmd);
	os.system(cmd);

