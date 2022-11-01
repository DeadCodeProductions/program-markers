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
void DCEMarker0_(void);
void DCEMarker1_(void);
int foo(int a) {
  if (a == 0) {
    DCEMarker1_();
    return 1;
  } else {
    DCEMarker0_();
    a = 5;
  }

  return a;
}
```
It is also possible to emit macros used for disabling parts of the code that have found to be dead:
```
./dead-instrument --emit-disable-macros test.c --


cat test.c
void DCEMarker0_(void);
void DCEMarker1_(void);
int foo(int a) {
#if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)
#if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)
    if (
#endif
        a == 0
#if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)
    )
#else
        ;
#endif
#ifndef DeleteBlockDCEMarker1_
    {

        DCEMarker1_();
        return 1;
    }
#endif
#if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)
    else

#endif

#ifndef DeleteBlockDCEMarker0_
    {
        DCEMarker0_();
        a = 5;
    }
#endif
#endif
    return a;
}


gcc -E -P -DDeleteBlockDCEMarker0_ test.c  | clang-format                                                                      disabled_dead_code_macros_squashed
void DCEMarker0_(void);
void DCEMarker1_(void);
int foo(int a) {
  a == 0;
  {
    DCEMarker1_();
    return 1;
  }
  return a;
}
```

Passing  `--ignore-functions-with-macros` to `dead-instrument` will cause it to ignore any functions that contain macro expansions.


#### Python wrapper

`pip install dead-instrumenter`


To use the instrumenter in python import `from dead_instrumenter.instrumenter import instrument_program`: `instrument_program(program: diopter.SourceProgram, emit_disable_macros: bool, ignore_functions_with_macros: bool) -> InstrumentedProgram`. 


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
