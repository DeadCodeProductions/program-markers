`program-markers` inserts markers (as function calls) in C or C++ source code to enable [differential testing for missed optimizations](https://thetheodor.github.io/papers/asplos22-1010.pdf):
1. Instrument a program.
2. Compile it with two or more compilers (or compiler versions, or optimization levels, etc).
3. For each output: the markers whose corresponding calls are still present in the
   generated assembly are _alive_, the remaining are _dead_.
4. Differential testing: compare the sets of _alive_ and _dead_ markers across
   outputs.

There are two kinds of markers supported: 
- DCE (Dead Code Elimination) Markers
- VR (Value Range) Markers

A __DCEMarker__ tests if a compiler dead code eliminates a piece of code (basic block). For example, 
```
if (Cond){
  DCEMarker0_();
  STMT0;
  STMT1;
  //...
}
```
If `call DCEMarker0_();` is not present in the generated assembly code then the
compiler determined that `Cond` must always be false and therefore the body of
this if statement is dead.

A __VRMarker__ tests if a compiler can determine subsets of the value ranges of
variables, for example:
```
if (!(LB <= a and a <= UB))
    VRMarker0_();
```
If `call VRMarker0_();` is not present in the generated assembly code then the
compiler determined that `a`'s value is always `LB <= a <= UB`. Currently only
integer variables are instrumented. 

#### To build just the clang tool

Prerequisites: `cmake`, `make`, `clang/llvm` 13/14.

```
mkdir build
cd build
cmake .. 
cmake --build . [--parallel]
cmake --install . --prefix=/where/to/install/
```

#### Usage
```
cat test.c
int foo(int a) {
    if (a == 0)
        return 1;
    else {
        a = 5;
    }

    return a;
}


dead-instrument test.c --


cat test.c | clang-format
#if defined DisableDCEMarker0_
#define DCEMARKERMACRO0_ ;
#elif defined UnreachableDCEMarker0_
#define DCEMARKERMACRO0_ __builtin_unreachable();
#else
#define DCEMARKERMACRO0_ DCEMarker0_();
void DCEMarker0_(void);
#endif
#if defined DisableDCEMarker1_
#define DCEMARKERMACRO1_ ;
#elif defined UnreachableDCEMarker1_
#define DCEMARKERMACRO1_ __builtin_unreachable();
#else
#define DCEMARKERMACRO1_ DCEMarker1_();
void DCEMarker1_(void);
#endif
int foo(int a) {
  if (a == 0)
  {
    DCEMARKERMACRO1_
    return 1;
  }
  else {
    DCEMARKERMACRO0_
    a = 5;
  }

  return a;
}
```

Individual markers can be disabled or turned into unreachables (useful for helping the compiler optimize parts known to be dead):

```
gcc -E -P -DDisableDCEMarker0_ -DUnreachableDCEMarker1_ test.c | clang-format
int foo(int a) {
  if (a == 0) {
    __builtin_unreachable();
    return 1;
  } else {
    ;
    a = 5;
  }
  return a;
}
```

Passing  `--ignore-functions-with-macros` to `program-markers` will cause it to ignore any functions that contain macro expansions.


Value range markers can be emitted instead by using `--mode=vr`: 
```
cat test.c
int foo(int a) {
  if (a == 0)
    return 1;
  return 0;
}

program-markers --mode=vr test.c --

cat test.c | clang-format 
#if defined DisableVRMarker0_
#define VRMARKERMACRO0_(VAR, TYPE)
#elif defined UnreachableVRMarker0_
#define VRMARKERMACRO0_(VAR, TYPE)                                                   \
  if (!(VRMarkerLowerBound0_ <= (VAR) && (VAR) <= VRMarkerUpperBound0_))       \
    __builtin_unreachable();
#else
#define VRMARKERMACRO0_(VAR, TYPE)                                                   \
  if (!(VRMarkerLowerBound0_ <= (VAR) && (VAR) <= VRMarkerUpperBound0_))       \
    VRMarker0_();
void VRMarker0_(void);
#endif
#ifndef VRMarkerLowerBound0_
#define VRMarkerLowerBound0_ 0
#endif
#ifndef VRMarkerUpperBound0_
#define VRMarkerUpperBound0_ 0
#endif
int foo(int a) {
  VRMARKERMACRO0_(a,"int")
  if (a == 0)
    return 1;
  return 0;
}
```


The ranges that each marker test can be adjucted via macros:

```
gcc -E -P -DVRMarkerLowerBound0_=-4 test.c | clang-format
void VRMarker0_(void);
int foo(int a) {
  if (!(-4 <= (a) && (a) <= 0))
    VRMarker0_();
  if (a == 0)
    return 1;
  return 0;
}
```

#### Python wrapper

`pip install program-markers`


To use the instrumenter in python import `from program_markers.instrumenter import instrument_program`: `instrument_program(program: diopter.SourceProgram, ignore_functions_with_macros: bool) -> InstrumentedProgram`. 


#### Building the python wrapper

##### Local build

```
./build_python_wheel_local.sh #this will build the current branch
pip install .
```

#### Docker based build

```
docker run --rm -e REVISION=REV -v `pwd`:/io theodort/manylinux-with-llvm:latest /io/build_python_wheel_docker.sh
```

This will build multiple wheels for `REV` with multiple python versions.
The output is stored in the `wheelhouse` directory.
The docker image is based on https://github.com/thetheodor/manylinux-with-llvm.
