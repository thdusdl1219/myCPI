#!/usr/bin/perl
#
# Update llvm-corelab from SVN, build, and run on a number of benchmarks.
# This script is intended to be run as a cron job.
#
# Usage: nightly_test.pl [options] > test_out
#   no options yet
# Or, a cron entry: 10 4 * * * /path/to/nightly_test.pl > /path/to/output
#
# A file named "report.YYYY_MM_DD_HHMM" will be generated with the results.
#
use File::Basename;

# some parameters
$cur_date = `date +"%Y_%m_%d_%H%M"`;
chomp $cur_date;
$report_file = "/u/loc/autotest/test/report.$cur_date";
$working_dir = "/u/loc/autotest/test";
$web_dir = "/corelab/www/MTCG-test";
$min_opti = 1;
$max_opti = 2;
$clean = 0;
$run_only = 0;

$benchmark_dir = "/corelab/benchmarks.ext";
$llvm_install_dir = "/u/loc/autotest/llvm/install";
$llvm_corelab_dir = "/u/loc/autotest/svn/trunk/corelab/src/llvm-corelab";
$smtx_dir = "/u/loc/autotest/svn/trunk/corelab/src/smtx";
$smtx_compile_cmd = "make clean && make sw_queue.bc";
$impact_dir = "/u/strianta/impact";
$scripts_dir = "$llvm_corelab_dir/scripts/testingScripts";
$compile_cmd = "$scripts_dir/compile_llvm";
$test_cmd = "$scripts_dir/test.py";

# the benchmark list
@benchmarks = (
# benchmark name, function name, -O0 loop name, -O1 loop name, -O2 loop name
#   ["wc", "cnt", "bb37", "bb35", "bb35"],
#   ["fir", "fir", "bb4", "bb2.thread", "bb2.thread"],
#   ["bmm", "matmult", "bb7", "bb5.preheader", "bb17.preheader"],
#   ["adpcmdec", "adpcm_decoder", "bb21", "bb", "bb"],
#   ["adpcmenc", "adpcm_coder", "bb26", "bb", "bb"],
#   ["ks", "FindMaxGpAndSwap", "bb6", "bb4.preheader", "bb24.preheader"],
#   ["mpeg2enc", "dist1", "bb36", "bb67.preheader", "bb67.preheader"],
#   ["otter", "find_lightest_geo_child", "bb5", "bb", "bb"],
#   ["h264enc", "encode_sequence", "bb", "bb", "bb"],
#   ["052.alvinn", "main", "xxx", "bb8.thread", "bb24.outer.outer"],
#   ["175.vpr", "try_swap", "xxx", "bb104", "bb111"],
#   ["177.mesa", "general_textured_triangle", "bb88", "bb28", "bb28"],
#   ["179.art", "match", "bb61", "bb", "bb11.outer"],
#   ["181.mcf", "refresh_potential", "bb14", "bb7.preheader", "bb7.preheader"],
#   ["183.equake", "smvp", "bb4", "bb", "bb"],
#   ["188.ammp", "mm_fv_update_nonbon", "bb116", "bb78", "bb78"],
#   ["197.parser", "batch_process", "xxx", "bb2", "bb58"],
#   ["256.bzip2", "compressStream", "xxx", "bb2", "bb84.thread"],
#   ["300.twolf", "new_dbox_a", "bb31", "bb", "bb"],
#   ["435.gromacs", "inl1130", "xxx", "bb", "bb"],
#   ["456.hmmer", "P7Viterbi", "xxx", "bb3", "bb3"],
#   ["458.sjeng", "std_eval", "xxx", "bb263", "bb263"],
  ["wc", "-", "-", "-", "-"],
  ["fir", "-", "-", "-", "-"],
  ["bmm", "-", "-", "-", "-"],
  ["adpcmdec", "-", "-", "-", "-"],
  ["adpcmenc", "-", "-", "-", "-"],
  ["ks", "-", "-", "-", "-"],
  ["mpeg2enc", "-", "-", "-", "-"],
  ["otter", "-", "-", "-", "-"],
#  ["h264enc", "-", "-", "-", "-"],
  ["052.alvinn", "-", "-", "-", "-"],
  ["175.vpr", "-", "-", "-", "-"],
  ["177.mesa", "-", "-", "-", "-"],
  ["179.art", "-", "-", "-", "-"],
  ["181.mcf", "-", "-", "-", "-"],
  ["183.equake", "-", "-", "-", "-"],
  ["188.ammp", "-", "-", "-", "-"],
  ["197.parser", "-", "-", "-", "-"],
  ["256.bzip2", "-", "-", "-", "-"],
  ["300.twolf", "-", "-", "-", "-"],
  ["435.gromacs", "-", "-", "-", "-"],
  ["456.hmmer", "-", "-", "-", "-"],
  ["458.sjeng", "-", "-", "-", "-"],
);

# what input to run for each benchmark. if none specified, the script will use
# spec_train1 if it exists; otherwise it will use the first one in the directory.
%inputs = (
  "adpcmdec" => "input3",
  "adpcmenc" => "input3",
  "ks" => "input5",
  "mpeg2enc" => "input2",
);

# set up the environment
$ENV{'LLVM_INSTALL_DIR'} = $llvm_install_dir;
$ENV{'CORELAB_INCLUDE_DIR'} = "$llvm_corelab_dir/include";
$ENV{'CORELAB_LIB_DIR'} = "$llvm_install_dir/lib";
$ENV{'IMPACT_REL_PATH'} = $impact_dir;
$ENV{'USER_BENCH_PATH1'} = $benchmark_dir;
$ENV{'IMPACT_HOST_COMPILER'} = "gcc";
$ENV{'IMPACT_HOST_PLATFORM'} = "x86lin";
$ENV{'PATH'} = "$scripts_dir:$impact_dir/scripts:$llvm_install_dir/bin:/usr/local/bin:$ENV{'PATH'}";
$ENV{'TERM'} = "xterm";

for ($i = 0; $i < @benchmarks; $i++) {
  print "$i: $benchmarks[$i][0] $benchmarks[$i][1] $benchmarks[$i][2] $benchmarks[$i][3] $benchmarks[$i][4] \n";
  for ($o = $min_opti; $o <= $max_opti; $o++) {
    $status[$i][$o] = "DID NOT RUN";
    $ctime[$i][$o] = "-";
    $rtime[$i][$o] = "-";
    $stime[$i][$o] = "-";
  }
}

if ($run_only == 0) {
# SVN update
  chdir $llvm_corelab_dir;
  if (run_cmd("svn update > $working_dir/svn_out 2>&1")) {
    punt("SVN update failed.\n");
  }
  chdir $smtx_dir;
  if (run_cmd("svn update > $working_dir/smtx_svn_out 2>&1")) {
    punt("SVN update failed for smtx.\n");
  }

# build
  chdir $llvm_corelab_dir;
  if (run_cmd("make clean && make install > $working_dir/compile_out 2>&1")) {
    $errmsg = "Failed to build llvm-corelab libraries.\n";
    $errmsg .= "Last 50 lines of $working_dir/compile_out:\n\n";
    $errmsg .= `tail -n 50 $working_dir/compile_out`;
    $errmsg .= "\n";
    punt($errmsg);
  }
  chdir $smtx_dir;
  if (run_cmd("$smtx_compile_cmd && mv -f sw_queue.bc $llvm_install_dir/lib")) {
    punt("Failed to build smtx bitcode.\n");
  }
}

# run.  this means run opt, generate the native exe, and run all the inputs.
# this uses Jack's test.py script.

# start the script that kills processes that run too long
$watchdog_pid = fork;
if ($watchdog_pid == 0) {
  exec "$scripts_dir/watchdog";
}

run_cmd("mkdir -p $working_dir");
chdir $working_dir;

# create the output file and update it incrementally as benchmarks finish.
open RPT, ">$report_file";
print RPT "Benchmarks run on " . `date`;
print RPT "\n";
print RPT "Bench\tFunc\tLoopHdr\tOpt\tCompile\tRun\tSingleRun\tStatus\n";
print RPT "=====\t====\t=======\t===\t=======\t===\t=========\t======\n";
$num_success = 0;

for ($b = 0; $b < @benchmarks; $b++) {
  $bench_name = $benchmarks[$b][0];
  $func_name = $benchmarks[$b][1];

  for ($o = $min_opti; $o <= $max_opti; $o++) {
    print `date`;
    $clean_this = 1;
    $opti = "-O$o";
    $bb_name = $benchmarks[$b][2+$o];
    #$dir_name = "$bench_name.$func_name.$bb_name.o$o";
    $dir_name = "$bench_name.o$o";
    run_cmd("rm -rf $bench_name");

    # compile benchmark source files to bitcode
    if (run_cmd("$compile_cmd $bench_name $opti > $dir_name.compile_out 2>&1")) {
      $status[$b][$o] = "FAILED in compilation";
      $clean_this = 0;  # not that it matters
      next;
    }
    run_cmd("rm -rf $dir_name");
    run_cmd("mkdir $dir_name");
    run_cmd("mv $bench_name $dir_name");

    # find input name (we used to run all, but that took too long)
    if (defined $inputs{$bench_name}) {
      $input = $inputs{$bench_name};
    } elsif (-e "$benchmark_dir/$bench_name/exec_info_spec_train1") {
      $input = "spec_train1";
    } else {
      $input = `ls $benchmark_dir/$bench_name/exec_info_* | head -1`;
      chomp $input;
      $input =~ s/.*exec_info_(.*)/$1/;
    }

    # generate and run the single-threaded native executable

    run_cmd("$test_cmd $bench_name $dir_name direct $input > $dir_name.single_out 2>&1");

    $stime[$b][$o] = `grep '^single_thread_time' $dir_name.single_out | awk '{printf \$2 }'`;
    print "single_thread_time was $stime[$b][$o]\n";

    # check if the single-threaded run was successful; if not, don't bother
    # doing the rest
    if (run_cmd("grep \*passed\* $dir_name.single_out")) {
      $status[$b][$o] = "SINGLE THREAD FAILED (see $dir_name.single_out)";
      $clean_this = 0;
    } else {

      # run MTCG, generate and run native executable

      run_cmd("$test_cmd $bench_name $dir_name mtcg $input > $dir_name.test_out 2>&1");

      # get compile time
      $opt_time = `grep '^opt_time' $dir_name/log | awk '{ printf \$2 }'`;
      $ctime[$b][$o] = $opt_time if ($opt_time ne "");
      print "opt_time was $ctime[$b][$o]\n";

      if (run_cmd("grep \*passed\* $dir_name.test_out")) {
        $status[$b][$o] = "FAILED (see $dir_name/log)";
        $clean_this = 0;
      } else {
        $status[$b][$o] = "SUCCESS";

        # get run time
        $rtime[$b][$o] = `grep '^run_time' $dir_name.test_out | awk '{printf \$2 }'`;
        print "run_time was $rtime[$b][$o]\n";

        # see if MTCG actually ran.
        # just check for the presence of the mtcg stat - it is only printed
        # if it is nonzero.
        if (run_cmd("grep 'number of regions parallelized' $dir_name/log")) {
          $status[$b][$o] .= " but no non-trivial partitions found!";
          $clean_this = 0;
        }
      }
    }

    # write a row to the output file
    print RPT "$bench_name\t$func_name\t$bb_name\t-O$o\t$ctime[$b][$o]\t$rtime[$b][$o]\t$stime[$b][$o]\t$status[$b][$o]\n";
    if ($status[$b][$o] =~ /SUCCESS/) {
      $num_success++;
    }

    # delete files if benchmark was successful
    if ($clean != 0 && $clean_this != 0) {
      run_cmd("rm -rf $dir_name*");
    }
  }
}

# kill the watchdog
kill 9, $watchdog_pid;

# generate report file
#print_report($report_file);
#print "Generated report file $report_file\n";
print RPT "\nFinished on " . `date`;
print RPT "\n$num_success out of " . (@benchmarks * ($max_opti-$min_opti+1)) . " runs were successful.";
print RPT "\nBenchmarks were run on " . `hostname` . "in $working_dir\n";
print RPT "\n";
print_env(RPT);
close RPT;

# copy to website and diff with previous
publish();

# end

# for now, just print out a simple text file
# this function is no longer called; we print the report incrementally now.
sub print_report {
  my ($report_file) = @_;
  open RPT, ">$report_file";
  print RPT "Report generated " . `date`;
  print RPT "\n";
  print RPT "Bench\tFunc\tLoopHdr\tOpt\tCompile\tRun\tSingleRun\tStatus\n";
  print RPT "=====\t====\t=======\t===\t=======\t===\t=========\t======\n";
  my $num_success = 0;
  for ($b = 0; $b < @benchmarks; $b++) {
    $bench_name = $benchmarks[$b][0];
    $func_name = $benchmarks[$b][1];
    for ($o = $min_opti; $o <= $max_opti; $o++) {
      print RPT "$bench_name\t$func_name\t$benchmarks[$b][2+$o]\t-O$o\t$ctime[$b][$o]\t$rtime[$b][$o]\t$stime[$b][$o]\t$status[$b][$o]\n";
      if ($status[$b][$o] =~ /SUCCESS/) {
        $num_success++;
      }
    }
  }
  print RPT "\n$num_success out of " . (@benchmarks * ($max_opti-$min_opti+1)) . " runs were successful.";
  print RPT "\nBenchmarks were run on " . `hostname` . "in $working_dir\n";
  print RPT "\n";
  print_env(RPT);
  close RPT;
}

sub publish {
  chdir $web_dir;
  run_cmd("cp $report_file .");
  $basename = basename($report_file);
  $previous = readlink "latest.html";
  $previous =~ s/\.html//;
  run_cmd("$scripts_dir/htmlize.pl $previous $basename $basename.html");
  run_cmd("ln -sf $basename.html latest.html");
}

sub run_cmd {
  my ($cmd) = @_;
  print "$cmd\n";
  return system($cmd);
}

sub print_env {
  my ($file) = @_;
  print $file "================================\n";
  print $file "Dumping environment variables...\n";
  foreach (sort keys %ENV) {
    print $file "$_=$ENV{$_}\n";
  }
}

sub punt {
  my ($info) = @_;
  print $info;
  print_env(STDOUT);
  open RPT, ">$report_file";
  print RPT "Report generated " . `date`;
  print RPT "\n";
  print RPT $info;
  print_env(RPT);
  close RPT;
  publish();
  exit;
}
