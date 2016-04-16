#!/usr/bin/python
# Run DSWP/MTCG, link, and execute a benchmark.
# Note: This script calls the scripts gen_profile.py, link_llvm, and run.
# Originally written by Jack
# Modifications by Kevin:
# - report compile and run times
# - can specify an input
# - generate profile info

import os, sys, string, commands, subprocess;

# Different execution mode for the input benchmark bitcode
EXECUTION_MODE_DIRECT = "direct";
EXECUTION_MODE_LOOP_EXTRACTION = "le";
EXECUTION_MODE_MTCG = "mtcg";
num_threads = 8;
input_name = "all";

# Setup the parameters if arguments are OK
# Print help message if arguments are wrong
error = 0;
if (len(sys.argv) < 5 or len(sys.argv) > 7):
	error = 1;
else:
	bench_name = sys.argv[1];
	path = sys.argv[2];
	mode = sys.argv[3];
	input_name = sys.argv[4];
	func_name = "";
	loop_name = "";
	if (len(sys.argv) >= 6):
		func_name = sys.argv[5];
	if (len(sys.argv) == 7):
		loop_name = sys.argv[6];
	if (mode != EXECUTION_MODE_DIRECT and mode != EXECUTION_MODE_MTCG):
		error = 1;
if (error):
	print("Usage: test.py bench path mode input [func_name [loop_name]]");
	print("Example: test.py test $HOME/temp mtcg input1 walk bb1");
	sys.exit(1);

# Setup the benchmarks to be run
if(bench_name == "all"):
	pass;
else:
	benchmark = [bench_name, ];
	benchmark_list = {}

### Main routine ###
cd_cmd = "cd " + path;
print(mode);
if(mode == EXECUTION_MODE_DIRECT):
	executable_name = bench_name + ".exe";
	link_cmd = cd_cmd + "; " + "link_llvm_smtx " + bench_name
	print("Link Command: " + link_cmd);
	os.system(link_cmd);
	if(input_name == "all"):
		run_cmd = cd_cmd + "; run_bench.sh " + bench_name
	else:
		run_cmd = cd_cmd + "; run_bench.sh " + bench_name + " " + executable_name + " -input " + input_name
	print("Run Command: " + run_cmd);
	os.system(run_cmd);
	time = subprocess.Popen(["cat", path + "/" + executable_name + ".time"], stdout=subprocess.PIPE).communicate()[0]
	print "single_thread_time: " + time

else:
	bitcode_name1 = bench_name + ".linked.bc";
	bitcode_name2 = bench_name + ".mtcg.bc";
	executable_name2 = bench_name + ".mtcg.exe";
	prof_arg = "";
	prof_pass = "";
	
	rm_cmd = cd_cmd + "; " + "rm -f " + bench_name + "/" + bitcode_name2 + " " + bench_name + "/" + executable_name2;
	#print("Remove Command: " + rm_cmd);
	os.system(rm_cmd);
	
	if(mode == EXECUTION_MODE_LOOP_EXTRACTION):
		pass_name = "-le";
		pass_options = " -le-func-name " + func_name + " -le-loop-name " + loop_name;
	else:
		# Do a profile run first.
		if (input_name == "all"):
			print("WARNING: Skipping profiling step because no input specified.")
		else:
			prof_cmd = "%s; gen_profile.py %s %s %s %s > prof_out 2>&1" % (cd_cmd, bench_name, bench_name, bitcode_name1, input_name)
			print("Profile Command: " + prof_cmd)
			os.system(prof_cmd)
			if (os.path.exists("%s/impactbenchsrc/impacttest_%s/llvmprof.out" % (path, input_name))):
				os.system("cp %s/impactbenchsrc/impacttest_%s/llvmprof.out %s" % (path, input_name, path))
				prof_arg = "-profile-info-file=llvmprof.out"
				prof_pass = "-profile-loader"
			else:
				print("WARNING: Could not find llvmprof.out.")
		
		pass_name = "-globalsmodref-aa %s -tripcounts -loopsimplify -minmax-reduce -dswp -psdswp -mtcg -mem2reg -stats" % (prof_pass);
		if (func_name == ""):
			func_arg = "-dswp-all";
			loop_arg = "";
		else:
			func_arg = "-dswp-target-fcn=" + func_name;
			if (loop_name == ""):
				loop_arg = "";
			else:
				loop_arg = "-dswp-target-loop=" + loop_name;
		pass_options = "%s %s -dswp-threads=%d %s" % (func_arg, loop_arg, num_threads, prof_arg)
	
	# Run opt
	opt_cmd = "%s; /usr/bin/time -f \"opt_time: %%E\" opt -load $CORELAB_LIB_DIR/libUtil.so -load $CORELAB_LIB_DIR/libExclusions.so -load $CORELAB_LIB_DIR/libPDG.so -load $CORELAB_LIB_DIR/libTarget.so -load $CORELAB_LIB_DIR/libClangAnnotationExtractor.so -load $CORELAB_LIB_DIR/libPartition.so -load $CORELAB_LIB_DIR/libDependenceRemover.so -load $CORELAB_LIB_DIR/libMTCG.so %s %s %s/%s -f -o %s/%s -debug-only=mtcg 2> log" % (cd_cmd, pass_name, pass_options, bench_name, bitcode_name1, bench_name, bitcode_name2)
	print("Opt Command: " + opt_cmd)
	os.system(opt_cmd)
	
	# Link
	link_cmd = "%s; link_llvm_smtx %s %s %s" % (cd_cmd, bench_name, bitcode_name2, executable_name2)
	print("Link Command: " + link_cmd)
	os.system(link_cmd)
	
	# Run
	if(input_name == "all"):
		run_cmd = cd_cmd + "; run_bench.sh " + bench_name + " " + executable_name2
	else:
		run_cmd = cd_cmd + "; run_bench.sh " + bench_name + " " + executable_name2 + " -input " + input_name
	print("Run Command: " + run_cmd)
	os.system(run_cmd)
	time = subprocess.Popen(["cat", path + "/" + executable_name2 + ".time"], stdout=subprocess.PIPE).communicate()[0]
	print "run_time: " + time

