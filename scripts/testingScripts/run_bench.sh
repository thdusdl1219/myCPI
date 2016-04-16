#!/bin/sh
# Jack Hung
# 2008/11/26 (one day before Thanksgiving)
# Input: Executable file
# Run this executable and check the results
# Usage: run hello [hello.lbc.exe]

# Set up environment with default values
FIND_BENCH_DIR=1;
READ_PATHS="";
CLEAN=1;
INPUT_OPTION="";
INPUT_ARG="";

# Flags for detected problems during test
LIB_WARNINGS=0;
CHECK_WARNINGS=0;
WILDCARD_WARNINGS=0;
NOT_CLEAN_WARNINGS=0;

# Assume arguments valid
VALID_ARGS=1;

# Jack: bench_name and executable_name can be different (example: test.bc.exe, test.lbc.exe, test.x.exe are all valid bitcode names)
# Get fixed argument(s)
if [ $# -ge 1 ]; then
	if [ $# -eq 1 ]; then
	    BENCHMARK="$1";
	    EXECUTABLE="$1.exe";
	    # skip the 1 set argument
	    shift 1;
	else
	    BENCHMARK="$1";
	    EXECUTABLE="$2";
	    # skip the 2 set argument
	    shift 2;
	fi
else
    VALID_ARGS=0;
fi

# get options after fixed arguments
while [ $# -gt 0 -a $VALID_ARGS -eq 1 ]
do

# get the next option specified
    OPTION="$1"
    shift

    case $OPTION in

        # Allow different projects to be used
	-project)
	    if [ $# -eq 0 ]; then
               echo "Error: test_bench_info expects a name after -project"
               exit 1;
            fi
	    READ_PATHS="$READ_PATHS -project $1"
            shift;;

        # Allow an benchmark dir be specified
        -bench_dir|-path)
            if [ $# -eq 0 ]; then
               echo "Error: test_bench_info expects a name after -bench_dir"
               exit 1;
            fi
            BENCH_DIR="$1";
            # Make sure specified path exists
            if [ ! -d $BENCH_DIR ]; then
               echo "Error: Invalid directory specified with -bench_dir option:"
               echo "       '${BENCH_DIR}'"
               exit 1;
            fi
            # Explicitly specify bench dir
            READ_PATHS="-bench_dir ${BENCH_DIR} $READ_PATHS";
            FIND_BENCH_DIR=0;
            shift;;

        -noclean)
            echo "> Will not clean up temp files and directories (prevents some checks!)"
            CLEAN=0;;

        # Support multiple input specifying options
        -input|-prefix)
            if [ "$INPUT_OPTION" != "" ]; then
               echo "Error: test_bench_info does not expect both '$INPUT_OPTION' and '$OPTION'"
               exit 1;
            fi
            INPUT_OPTION="$OPTION"
            if [ $# -eq 0 ]; then
               echo "Error: test_bench_info expects a argument after $OPTION"
               exit 1;
            fi
            INPUT_ARG="$1";
            shift;;

        *)
            echo "Error: Unknown option '${OPTION}'"
            VALID_ARGS=0;;
    esac
done


if [ $VALID_ARGS -eq 0 ]; then
    echo ' ';
    echo '> Usage: run hello [hello.lbc.exe]';
    echo '> ';
    echo '> Options (zero or more of the the following may be specified):';
    echo '>   -input "name(s)"   use the inputs listed in "name(s)" (default: all inputs)';
    echo '>   -prefix "prefix"   use inputs named "prefix*" (default all inputs, prefix="")';
    echo '>   -noclean           saves test directories, etc. (prevents some checks!).';
    exit 1;
fi;


############################################################################
#       Find the benchmark info
############################################################################

  # Find the benchmark dir if not user specified
  if [ $FIND_BENCH_DIR -eq 1 ]; then
    echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%";
    echo "> Finding the info for ${BENCHMARK} using find_bench_dir";
    echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%";
    echo " ";
    BENCH_DIR=`find_bench_dir ${BENCHMARK}`
    if test "$?" != 0; then
      echo " "
      echo "> Exiting: Could not find '${BENCHMARK}' using find_bench_dir!"
      echo "> Error message returned by find_bench_dir:"
      echo "$BENCH_DIR";
      exit 1;
    fi
    # Explicitly specify bench dir
    READ_PATHS="-bench_dir ${BENCH_DIR} $READ_PATHS";
  fi

  echo "> Testing the info for ${BENCHMARK} in:"
  echo ">   $BENCH_DIR"


############################################################################
#       Determine inputs to test and make sure valid
############################################################################
  # Handle default (all input) case
  if [ "$INPUT_OPTION" = "" ]; then 
      INPUT_LIST="`(find_bench_inputs ${BENCHMARK} $READ_PATHS -all) 2>&1`";
      INPUT_ERROR_CODE="$?";

  # Handle -prefix case 
  elif [ "$INPUT_OPTION" = "-prefix" ]; then 
    INPUT_LIST="`(find_bench_inputs ${BENCHMARK} $READ_PATHS -prefix \"$INPUT_ARG\") 2>&1`";
    INPUT_ERROR_CODE="$?";

  elif [ "$INPUT_OPTION" = "-input" ]; then 
      INPUT_LIST="`(find_bench_inputs ${BENCHMARK} $READ_PATHS -input \"$INPUT_ARG\") 2>&1`";
      INPUT_ERROR_CODE="$?";

  else
    echo " "
    echo "> Error: test_bench_info unexpectedly doesn't know how to handle '$INPUT_OPTION'"
    exit 1;
  fi

  if test "$INPUT_ERROR_CODE" != 0; then
     echo " "
     echo "> Exiting test_bench_info, find_bench_inputs returned this error message:"
     echo "$INPUT_LIST";
     exit 1;
   fi

# Jack (2008/11/26): Remove the invocations to "read_platform_info" (for my desktop)

# Jack's cheating
HOST_PLATFORM=x86lin
HOST_COMPILER=llvm

# Jack (2008/11/26): Setup something for execution
EXEC_NAME="${BENCHMARK}/${EXECUTABLE}";
LN_COMMAND="ln -s";

# Jack: make and enter directory "impactbenchsrc"
rm -rf impactbenchsrc
mkdir impactbenchsrc
cd impactbenchsrc

############################################################################
#       Running executable using the specified inputs
############################################################################
  # Other scripts use CHECK_DESC to identify what is being tested (e.g.,
  # O_im, S_im, HS_im, etc.)  Use HOST_COMPILER to identify test.
  CHECK_DESC="$HOST_COMPILER";

  for INPUT in $INPUT_LIST
  do  
    RESULT_FILE="${BENCHMARK}_${CHECK_DESC}_${INPUT}.result";
    STDERR_FILE="${BENCHMARK}_${CHECK_DESC}_${INPUT}.stderr";
    echo " "
    echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%";
    echo "> Reading $INPUT info for ${BENCHMARK} using read_exec_info";
    echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%";

    # Read in all the variables, don't expect any errors since worked above
    SETUP=`read_exec_info ${BENCHMARK} $READ_PATHS -input $INPUT -setup`;
    if test "$?" != 0; then echo "Unexpected Exit (ARGS)!: non-zero exit code";echo "$SETUP";exit 1;fi

    PREFIX=`read_exec_info ${BENCHMARK} $READ_PATHS -input $INPUT -prefix`;
    if test "$?" != 0; then echo "Unexpected Exit (PREFIX)!: non-zero exit code";echo "$PREFIX";exit 1;fi

    ARGS=`read_exec_info ${BENCHMARK} $READ_PATHS -input $INPUT -args`;
    if test "$?" != 0; then echo "Unexpected Exit (ARGS)!: non-zero exit code";echo "$ARGS";exit 1;fi

    CHECK=`read_exec_info ${BENCHMARK} $READ_PATHS -input $INPUT -check $RESULT_FILE $STDERR_FILE`;
    if test "$?" != 0; then echo "Unexpected Exit (CHECK)!: non-zero exit code";echo "$CHECK";exit 1;fi

    CLEANUP=`read_exec_info ${BENCHMARK} $READ_PATHS -input $INPUT -cleanup`;
    if test "$?" != 0; then echo "Unexpected Exit (CLEANUP)!: non-zero exit code";echo "$CLEANUP";exit 1;fi

    SKIP=`read_exec_info ${BENCHMARK} $READ_PATHS -input $INPUT -skip`;
    if test "$?" != 0; then echo "Unexpected Exit (SKIP)!: non-zero exit code";echo "$SKIP";exit 1;fi

    echo " "
    echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%";
    echo "> Testing $INPUT info for ${BENCHMARK}";
    echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%";
    echo " "

    EXEC_DIR="impacttest_${INPUT}"

    # Most, but not all, scripts make subdirectory, so make sure this works!
    echo "> Making directory '$EXEC_DIR' and changing directories into it";
    rm -rf $EXEC_DIR;
    mkdir $EXEC_DIR;
    if test "$?" != 0; then 
       echo "Exiting: 'mkdir $EXEC_DIR' returned non-zero exit code";
       exit 1;
    fi
     
    cd $EXEC_DIR;

    ${LN_COMMAND} -f ../$EXEC_NAME .

    # Do we have a setup command?
    if [ "$SETUP" != "" ]; then
      # Shell out to do setup so expresssions evaluated properly
      echo " "    
      echo "> Setting up execution (via /bin/sh):"
      echo "> $SETUP"
      /bin/sh -c "$SETUP"
    else
      echo " "
      echo "> Skipping setup, no SETUP command(s) specified";
    fi

    # Write elapsed time to a file
    TIMENAME="../../$EXECUTABLE.time"

    echo
    echo "> Starting Execution (via /bin/sh) in `pwd`"
    echo ">   ($PREFIX /usr/bin/time -f "%E" -o $TIMENAME nice ../../$EXEC_NAME $ARGS) > $RESULT_FILE 2> $STDERR_FILE"
    /bin/sh -c "($PREFIX /usr/bin/time -f "%E" -o $TIMENAME nice ../../$EXEC_NAME $ARGS) > $RESULT_FILE 2> $STDERR_FILE"

    CHECK_FILE="${BENCHMARK}_${CHECK_DESC}_${INPUT}.check";
    if [ "$CHECK" != "" ]; then
      TRUE_CHECK="($CHECK) > $CHECK_FILE 2>&1"
    else
      echo "> Warning: no check specified for $BENCHMARK/$INPUT.  Using 'cat $RESULT_FILE'"
      TRUE_CHECK="(cat $RESULT_FILE) > $CHECK_FILE 2>&1"
      CHECK_WARNINGS=1;
    fi 
    echo " "
    echo "> Checking results (via /bin/sh) in `pwd`"
    echo ">   ${TRUE_CHECK}"
    echo ">"
    echo "> RESULT CHECK BEGIN FOR ${BENCHMARK}/${CHECK_DESC}/${INPUT}";
    # Shell out to do check so expresssions evaulated properly
    /bin/sh -c "$TRUE_CHECK"

    # Print warning if CHECK_FILE not empty
    if [ -s $CHECK_FILE ]; then
      echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
      echo "> Warning: check failed for ${BENCHMARK}/${CHECK_DESC}/${INPUT}"
      echo "> Check output size (via wc): '`wc $CHECK_FILE`'"
      echo "> Output shown below via 'head -n 30 $CHECK_FILE'"
      echo "%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%"
      head -n 30 $CHECK_FILE
      CHECK_WARNINGS=1;
      CMESG="(MISMATCH)"
    else
      CMESG="(PASSED)"
    fi
    echo "> RESULT CHECK END FOR ${BENCHMARK}/${CHECK_DESC}/${INPUT} $CMESG";
    echo ">"

    # If specified -noclean, leave things in post-run state, 
    # which prevents many of the checks normally done!
    if [ $CLEAN -eq 0 ]; then
      echo "> Warning: -noclean mode prevents normal CLEANUP checks!"
      echo " "
      echo "> Changing directories to '..' but keeping '$EXEC_DIR'"
      cd ..

    # Otherwise, do the normal cleaning and checks
    else
      if [ -s $CHECK_FILE ]; then
        echo "> Moving $CHECK_FILE to '..' for inspection"
        mv -f $CHECK_FILE ..
        echo "> Moving $RESULT_FILE to '..' for inspection"
        mv -f $RESULT_FILE ..
        echo "> Moving $STDERR_FILE to '..' for inspection"
        mv -f $STDERR_FILE ..
      else
        echo "> Removing $CHECK_FILE";
        rm -f $CHECK_FILE;   
        echo "> Removing $RESULT_FILE";
        rm -f $RESULT_FILE;   
        echo "> Removing $STDERR_FILE";
        rm -f $STDERR_FILE;   
      fi

      echo "> Removing link to $EXEC_NAME"
      rm -f $EXEC_NAME;

      if [ "$CLEANUP" != "" ]; then
        echo "> Doing rest of cleanup using (via /bin/sh):"
        echo ">   $CLEANUP"
        /bin/sh -c "$CLEANUP";
      else
        echo "> Skipping rest of cleanup, no CLEANUP command(s) specified";
      fi

      # Make sure they haven't specified a 'rm *.out', etc. in their cleanup, 
      # since may run cleanup in current directory with files the user would 
      # like to keep.  To detect, use sed to replace any lines with '*' in
      # it with '(BAD)'.
      WILDCARD_TEST="`echo \"$CLEANUP\" | sed 's/^.*\*.*/(BAD)/'`";
      if [ "$WILDCARD_TEST" = "(BAD)" ]; then
        echo " "
        echo "> Warning: '*' in CLEANUP command for ${BENCHMARK}/${CHECK_DESC}/${INPUT}"
        echo ">          CLEANUP may be run in user's original directory and should not"
        echo ">          delete files not created by SETUP or running the benchmark!"
        # This is commented out by Jack. Some benchmarks just want to clean up the mess. What can you do?
        #WILDCARD_WARNINGS=1;
      fi


      # The directory should be empty at this point, if cleanup is properly
      # specified.  Make sure it is, since may run benchmark in directory
      # with other important stuff and don't want random files accumulating.
      REMAINING_FILES="`ls -A`"    
      if [ "$REMAINING_FILES" != "" ]; then
        echo " "
        echo "> Warning: CLEANUP missed files for ${BENCHMARK}/${CHECK_DESC}/${INPUT}"
        echo ">          The following files were found after CLEANUP executed:"
        echo "$REMAINING_FILES"
        # This is commented out by Jack. Well, some benchmarks want to make a total cleanup. What can you do?
        #NOT_CLEAN_WARNINGS=1;
      fi

      echo " "
      echo "> Changing directories to '..' and removing '$EXEC_DIR'"
      cd ..
      rm -rf $EXEC_DIR;
    fi

  done

  # Remove executable unless -noclean specified
  if [ $CLEAN -eq 1 ]; then
    echo " "
    if [ "$LIB_REQUIREMENTS" != "STATIC_REQUIRED" ]; then
      echo "> Final cleanup, removing ${BENCHMARK}.shared"
      rm -f ${BENCHMARK}.shared
    fi

    if [ "$LIB_REQUIREMENTS" != "SHARED_REQUIRED" ]; then
       echo "> Final cleanup, removing ${BENCHMARK}.static"
       rm -f ${BENCHMARK}.static
    fi
  fi
  
  # Remove dir "impactbenchsrc"
  echo "> Removing impactbenchsrc"
  rm -rf impactbenchsrc
  
  echo " ";
  WARNINGS_ISSUED=0;
  if [ $LIB_WARNINGS -eq 1 ]; then
     echo "> Warning: User overrode bench_info's LIB_REQUIREMENTS for $BENCHMARK"
     WARNINGS_ISSUED=1;
  fi
  if [ $CHECK_WARNINGS -eq 1 ]; then
     echo "> Warning: One or more output checks failed for $BENCHMARK"
     WARNINGS_ISSUED=1;
  fi
  if [ $WILDCARD_WARNINGS -eq 1 ]; then
     echo "> Warning: One or more CLEANUP commands contain '*' for $BENCHMARK"
     WARNINGS_ISSUED=1;
  fi
  if [ $NOT_CLEAN_WARNINGS -eq 1 ]; then
     echo "> Warning: One or more CLEANUP commands missed files for $BENCHMARK"
     WARNINGS_ISSUED=1;
  fi
  
  if [ $CLEAN -eq 0 ]; then
    echo "> Warning: -noclean mode prevents normal CLEANUP checks for $BENCHMARK"
    echo "> Done, test_bench_info '-noclean' mode (see above warnings) for $BENCHMARK"
  else
    if [ $WARNINGS_ISSUED -eq 1 ]; then
      echo "> Done, test_bench_info *failed* (see above warnings) for $BENCHMARK"
    else
      echo "> Done, test_bench_info *passed* for $BENCHMARK"
    fi
  fi

