`dead-instrument` is the instrumenter used in [DEAD](https://github.com/DeadCodeProductions/dead_instrumenter).


#### Build

Prerequisites: `cmake`, `make`, `clang/llvm` 13.

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
void DCEMarker2_(void);
int foo(int a) {
  if (a == 0) {
    DCEMarker0_();
    return 1;
  } else {
    DCEMarker1_();
    a = 5;
  }
  DCEMarker2_();

  return a;
}
```



#### Python wrapper

`pip install dead-instrumenter`


To use the instrumenter in python import `from dead_instrumenter.instrumenter import instrument_program`. 
Calling `instrument_program(filename:str) -> None` will instrument `filename`.
