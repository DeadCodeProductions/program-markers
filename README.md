`dead-instrument` is the instrumenter used in [DEAD](https://github.com/DeadCodeProductions/dead).


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


cat test.c
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

Individual markers can be disabled or tunred into unreachables (useful for helping the compiler optimize parts known to be dead):

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

Passing  `--ignore-functions-with-macros` to `dead-instrument` will cause it to ignore any functions that contain macro expansions.


#### Python wrapper

`pip install dead-instrumenter`


To use the instrumenter in python import `from dead_instrumenter.instrumenter import instrument_program`: `instrument_program(program: diopter.SourceProgram, ignore_functions_with_macros: bool) -> InstrumentedProgram`. 


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
