# Non-Distinguishable Inconsistencies as a Deterministic Oracle for Detecting Security Bugs
We propose a novel approach for detecting security bugs based on a new concept called Non-Distinguishable Inconsistencies (**NDI**).
The insight is that if two code paths in a function exhibit inconsistent security states (such as being freed or initialized) that are *non-distinguishable from the external*, such as the callers, there is no way to recover from the inconsistency from the external, which results in a bug.

## How to use it

### Build LLVM
```sh 
	$ cd llvm 
	$ ./build-llvm.sh 
	# The installed LLVM is of version 15
```
Notice that NDI is still in developing process, therefore it should be compatible with the newest LLVM version.

### Build NDI
```sh 
	# Build the analysis pass of NDI 
	$ cd ../analyzer 
	$ make 
	# Now, you can find the executable, `kanalyzer`, in `build/lib/`
```

### Prepare LLVM bitcode files for targeting projects.
* Generate bitcode files in compiling process.
* One way to achieve that is by "-save-temps" options in Clang

### Run the NDI analyzer
```sh
	# To analyze a single bitcode file, say "test.bc", run:
	$ ./build/lib/kanalyzer test.bc
	# To analyze a list of bitcode files, put all of them in one dictionary, say "bc.dict", then run:
	$ ./build/lib/kalalyzer bc.dict
```

