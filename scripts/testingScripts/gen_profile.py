#!/usr/bin/python
# Jack Hung
# Generate the profiles (of basic block profiling) for the application

import os, sys, string, commands;

# Setup the parameters if arguments are OK
# Print help message if arguments are wrong
if((len(sys.argv) == 4) or (len(sys.argv) == 5)):
	bench_name = sys.argv[1];
	base_path = sys.argv[2];
	bench_bc = sys.argv[3];
	bench_prof_bc = bench_bc + ".prof";
	bench_prof_asm = bench_bc + ".prof.s";
	bench_prof_exe = bench_bc + ".prof.exe";
	if(len(sys.argv) == 5):
		prof_run_input = sys.argv[4];
	else:
		prof_run_input = "all";
else:
	print("Usage: gen_profile.py bench_name base_path bench_bitcode prof_run_input");
	print("Example: gen_profile.py wc $HOME/temp/wc wc.lbc [spec_train1]");
	sys.exit(1);

# Setup the benchmarks to be run
#if(bench_name == "all"):
#	pass;
#else:
#	benchmark = [bench_name, ];
#	benchmark_list = {}

### Main routine ###
cd_cmd = "cd " + base_path;

# Step 0: Remove previous profiling results (if any)
rm_cmd = cd_cmd + "; " + "rm -f llvmprof.out";
print("=> " + rm_cmd);
os.system(rm_cmd);

# Step 1: Insert code for block profiling
# Note: block profiling no longer exists, use edge profiling now
opt_cmd = cd_cmd + "; " + "opt -insert-edge-profiling -f " + bench_bc + " -o " + bench_prof_bc;
print("=> " + opt_cmd);
os.system(opt_cmd);

# Step 2: Generate native assembly
llc_cmd = cd_cmd + "; " + "llc -f " + bench_prof_bc + " -o " + bench_prof_asm;
print("=> " + llc_cmd);
os.system(llc_cmd);

# Step 3: Generate native executable
# Note: use special script for SPEC programs
# FIXME: should use jack_link_llvm
gcc_cmd = cd_cmd + "; " + "gcc " + bench_prof_asm + " $CORELAB/src/lib/libprofile_rt.so -lm -o " + bench_prof_exe;
#gcc_cmd = cd_cmd + "; " + "gcc -lm CommonProfiling.o BlockProfiling.o " + bench_prof_asm + " -o " + bench_prof_exe;
print("=> " + gcc_cmd);
os.system(gcc_cmd);

# Step 4: Do the profiling run
# Note: use special script for SPEC programs
#run_cmd = cd_cmd + "; " + "./" + bench_name + ".exe 1>/dev/null 2>/dev/null";
# The "run" script to change to the correct directory itself
# Added "-noclean" otherwise llvmprof.out gets deleted. -KF 1/9/2009
if(prof_run_input == "all"):
	run_cmd = "run_bench.sh " + bench_name + " " + bench_prof_exe + " -noclean";
else:
	run_cmd = "run_bench.sh " + bench_name + " " + bench_prof_exe + " -input " + prof_run_input + " -noclean";
print("=> " + run_cmd);
os.system(run_cmd);

# Step 5: Dump the profiling results
#prof_cmd = cd_cmd + "; " + "llvm-prof " + bench_name + " llvmprof.out";
#print("=> " + prof_cmd);
#os.system(prof_cmd);

print("");
print("Profiling results are in llvmprof.out");

