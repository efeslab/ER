* Artifact for submission "Reproducing Production Failures with Execution Reconstruction"

Thank you for evaluating the software artifact of ER. This artifact is designed for the ACM Artifacts Available and Functional Badge.

* Getting Started Guide

This artifact is packaged by docker. We have tested the artifact using docker 19.03 but the image should work with other versions of docker.
You should provide this artifact at least 5GB of disk and 8GB of memory.
Throughout this README, all commands should be executed in a terminal will be encapsulated in Markdown-style ```bash ```.

** Setup

First, please download our artifact package paper618.tar.gz using the link provided on HotCrp and load it into docker.
E.g. on linux, run
```bash
gzip -c -d paper618.tar.gz | docker load
```
Alternatively, if permitted to use external files from dockerhub, you can also simply pull our artifact image:
```bash
docker pull alkaid/er-pldi2021-artifact:latest
```

List all docker images, you should be able to find a new image called "alkaid/er-pldi2021-artifact:latest". E.g.
```bash
docker images
```

Then let us create a new container called "er", which is an instance of our artifact image, and keep it running in the background.
```bash
docker run --name "er" -itd alkaid/er-pldi2021-artifact:latest /bin/bash
```

To get a bash terminal access inside the container "er", run:
```bash
docker exec -ti er /bin/bash
```
If you are able to see a new terminal prompt like "root@${container id}:...", then great the docker image is successfully setup.
If necessary, you can get as many bash terminal accesses as needed by just rerunning the above command.

** An automated example

Let us first take a look at one sqlite bug.
```bash
cd /ER/third-party/bugbasev2/sqlite-7be932d
../utils/run-er.sh
```
You should be able to see the following sequences of messages in stdout:

1. "Build Everything"
  This is to compile the application source code to generate LLVM IR bitcode and native binary.
  Since we provide pre-built LLVM bitcodes and binaries, this step does nothing.

2. Iterative messages: "Start iteration 1/2/3..."
  This is the core of ER, which exercises all components we described in the paper (section 4, Implementation): KLEE runtime, key data values selections/recording, shepherded symbolic execution and symbolic execution stall detection.
  In this sqlite bug, there will be two iterations, which take about 6 mins to finish.

3. "Fully replayed, abort"
  This message should appear right after the third iteration. It means two iterations already fully replayed the failure trace so there is no need for a third iteration. Then, ER will run klee again to verify the genereated inputs indeed reproduce the origin failure.

4. "Checking whether ER reproduces the same control flow"
  This message reports the failure reproduction result.
  If it says "Same control flow verified", then ER successfully reproduces the failure.

5. "ER Finished"
  Congratulations! You have successfully run ER on one sqlite bug. Script will report the total number of iterations and time spent in offline symbolic execution.

* Step-by-step Instructions

This artifact allows you to reproduce the following bugs, which is a subset of Table 1 in the paper:
```bash
cd /ER/third-party/bugbasev2/sqlite-7be932d
cd /ER/third-party/bugbasev2/sqlite-787fa71
cd /ER/third-party/bugbasev2/sqlite-4e8e485
cd /ER/third-party/bugbasev2/php-74194
cd /ER/third-party/bugbasev2/python-2018-1000030
```
You can refer to the Table 1 for expected number of iterations (the "#Occur" column) and time cost (the "Symbex Time" column).

** Availability

Our tool is open-sourced and available at https://github.com/efeslab/ER
This artifact also contains a copy of the full repository at `/ER`
If you want to build our tool from scratch, please refer to `/ER/artifact/install.sh`
If you want to build this artifact from scratch, please refer to `/ER/artifact/Dockerfile`

** Functionality

Our artifact is automated using Makefile, the `run-er.sh` used above is a simple wrapper invoking various make targets.
For each application, there is an application-specific Makefile at `/ER/third-party/bugbasev2/${APPLICATION}/Makefile` and a shared Makefile rules at `/ER/third-party/bugbasev2/utils/Makefile.klee.rules`.
You can run `make help` to see a quick reference of all make targets in the terminal.

Plese make sure you are located in an application directory (e.g. `cd /ER/third-party/bugbasev2/sqlite-7be932d`) before going forward.
Now let us take a closer look at each step and explain the corresponding make targets.
--------------------------------------------------------------------------------
*** Step 1: build the application

We provided pre-built application binaries and LLVM bitcodes.
But if interested, you can compile each application from scratch using:
```bash
make build-clean
make all-bc
```
Target "build-clean" will delete all cached binaries and LLVM bitcodes.
Target "all-bc" will fetch application source code from either ER git repo or official website (e.g. www.sqlite.org). This will not relveal reviewer identities.
The compilation process uses `wllvm` to first compile source code to elf binary (with LLVM IR embedded), then extract the LLVM bitcode of the whole program from a binary.
--------------------------------------------------------------------------------
*** Step 2: record a failure execution using oracle input

To evaluate whether ER can reproduce a failure in an appliation, we first find "oracle inputs", which are known to trigger the failure.
The oracle inputs are defined by the "INPUT_FILES" variable in each Makefile.
Then, we simulate "tracing a failure happened in production" by triggering the same failure in klee and collect execution trace.
Execution trace consists of
    (1) an always-on control-flow tracing and
    (2) a key data values recording depends on previous replay analysis results
To generate the data recording configuration, run
```bash
make config/datarec.1.cfg
````
Note that in the first iteration, which is the first time a failure occurs, we do not record any data values, thus `config/datarec.1.cfg` will always be an empty file.
With data recording configuration ready, you can not invoke klee to record a failure execution using oracle inputs.
```bash
make record.1.klee-out
```
The output of KLEE is messy, but you can ignore all KLEE warnings, messages and internal performance statistics.
KLEE will stop due to errors caused by the buggy application (e.g. out-of-bound memory access error).
The recorded trace locates in `record.1.klee-out/test000001.path`, you can use the following commands to examine a recorded trace:
```bash
# see a summary of recorded events like branches(FORK), data recording (DATAREC), etc.
pathviewer record.1.klee-out/test000001.path
# see a complete dump of the trace
pathviewer -dump recorded.1.klee-out/test000001.path | less
```

In later iterations, you should use iteration number as part of the make target. E.g.
```bash
# will use analysis results from replay.1.klee-out
make config/datarec.2.cfg
make record.2.klee-out
# etc.
```

NOTE: How to terminate a running klee instance?
You should send SIGQUIT to that process. E.g. `kill -SIGQUIT ${PID}` or `pkill -SIGQUIT klee`
Sending SIGQUIT once is a gentle request to stop klee as soon as possible.
Sending SIGQUIT twice is a force shutdown.
Sending SIGINT (i.e. ctrl+c) will request klee to print out internal execution states.
--------------------------------------------------------------------------------
*** Step 3: symbolically replay a recorded execution trace

Now you can guide klee along the trace recorded in Step 2 but assume the content of all inputs unknown.
To symbolically replay the recorded trace, you can
```bash
make replay.1.klee-out
```
If klee can fully replay the trace, then failure is reproduced and iteration will stop. Goto Step 4.
Otherwise, ER will select key data values to record in the next iteration. Goto Step 2.
--------------------------------------------------------------------------------
*** Step 4: verify the input generated by ER indeed reproduce the same failure

If the recorded failure trace can be symbolically replayed, ER is able to generate new inputs which are guaranteed to reproduce the same failure.
To manually compare oracle inputs with er-generated inputs, you can
```bash
make compare
```
Then take a look at different input files following the prompt.

To see how the application natively (i.e. use elf binary) fails with the oracle and generated inputs, you can
```bash
# run the native binary using oracle inputs
make elf-fail
# run the native binary using er-generated inputs
make elf-fail-er
```

To precisely determine that er-generated inputs lead to exactly the same control-flow trace as the failure caused by oracle inputs, you can perform a recording similar to Step 1 but use er-generated inputs:
```bash
make record.check.klee-out
```
Then you can use the tool `pathviewer -dump` to compare two control-flow traces event by event:
```bash
make verify
```

In the end, you can also see ER statistics (number of iterations and symbolic replay time):
```bash
make report
```
--------------------------------------------------------------------------------
*** Recap

The normal procedure to reproduce a failure of an application is:
```bash
make all-bc
make record.1.klee-out
make replay.1.klee-out
make record.2.klee-out
make replay.2.klee-out
...
...
# Fully replay the failure trace at replay.(N-1).klee-out
make replay.(N-1).klee-out
# Fully replayed detected during record.N.klee-out, stop iteration
make record.N.klee-out
make verify
make report
```

This is exactly what `/ER/third-party/bugbasev2/utils/run-er.sh` does.
--------------------------------------------------------------------------------


* List of claims from the paper supported by this artifact

1. At the end of Introduction: Shepherded Symbolic Execution plus Key Data Value Selection can overcome solver bottlenecks.
  Supported by ER can reproduce the failure with control-flow and selected data value. However, simply running KLEE, even with full control-flow trace but without data values, cannot reproduce the same failure.
  To compare ER with KLEE+control-flow trace, you need to disable the solver timeout detection so that you can symbolically replay the first iteration (which does not record data values) as long as possible.
  ```bash
  make replay.1.klee-out ENABLETIMEOUT=0
  ```
  For bugs requiring ER to selected key data values, KLEE+control-flow trace is expected to not reproduce the failure in a reasonable time.
2. Section 5.2: Effectiveness and accuracy of ER
  For effectiveness, we include a subset of the Table 1 in this artifact. These are real bugs from real-world applications. The length of those execution traces are all above 1 million X86_64 instructions.
  We use `perf stat` to count the number of instructions executed in the `make elf-fail` target. However, perf is hard to work with docker so we do not provide native instruction count in this artifact. Instead, you can find the number of LLVM IR instructions executed during Step 1 in the end of file `record.*.klee-out/info`. Clearly, ER is able to handle longer traces than previous work REPT (around 100K native instructions).
  For accuracy, We demonstrate that the inputs generated by ER can perfectly reproduce the same failure in Step 4 (`make verify`).

* List of claims from the paper NOT supported by this artifact

1. Runtime performance overhead (Figure 6) is not included in this artifact.
  Our implementation uses Intel's PTWRITE instruction. Even though Intel PT extension is available in commodity CPU, its PTWRITE feature is expected to not be supported by reviewers' CPU. We use a rare "Intel Pentium Silver J5005 CPU". So we decided to not include this experiment in the artifact.
2. Many other claims are not support in this artifact, e.g. the rest of Table 1, Figure 5, etc. We only target for the Functional badge.
