#include <catch2/catch.hpp>

#include <ValueRangeInstrumenter.h>

#include "test_tool.h"

TEST_CASE("VRMarkers statement with two variables", "[vr]") {
  auto Code = std::string{R"code(int foo(int a, int b){
        return a+b;
        })code"};

  auto ExpectedCode = dead::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      dead::ValueRangeInstrumenter::makeMarkerMacros(1) +
                      R"code(int foo(int a, int b){
                         VRMARKERMACROLE1_(b)
                         VRMARKERMACROGE1_(b)
                         VRMARKERMACROLE0_(a)
                         VRMARKERMACROGE0_(a)
                         return a+b; })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runVRInstrumenterOnCode(Code, false));
}

TEST_CASE("VRMarkers if no compound", "[vr,if]") {
  auto Code = std::string{R"code(int foo(int a){
        if (a > 0)
          return a+1;
        return a+2; 
        })code"};

  auto ExpectedCode = dead::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      dead::ValueRangeInstrumenter::makeMarkerMacros(1) +
                      R"code(int foo(int a){
                         VRMARKERMACROLE0_(a)
                         VRMARKERMACROGE0_(a)
                         if ( a > 0)
                           return a+1;
                         VRMARKERMACROLE1_(a)
                         VRMARKERMACROGE1_(a)
                         return a+2; })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runVRInstrumenterOnCode(Code, false));
}

TEST_CASE("VRMarkers if-else compound", "[vr,if]") {
  auto Code = std::string{R"code(int foo(int a){
        if (a > 0) {
          return a+1;
        }
        else {
          return a+2; 
        }
        })code"};

  auto ExpectedCode = dead::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      dead::ValueRangeInstrumenter::makeMarkerMacros(1) +
                      dead::ValueRangeInstrumenter::makeMarkerMacros(2) +
                      R"code(int foo(int a){
                         VRMARKERMACROLE0_(a)
                         VRMARKERMACROGE0_(a)
                         if ( a > 0) {
                           VRMARKERMACROLE1_(a)
                           VRMARKERMACROGE1_(a)
                           return a+1;
                         } else {
                           VRMARKERMACROLE2_(a)
                           VRMARKERMACROGE2_(a)
                           return a+2; 
                         }
                         })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runVRInstrumenterOnCode(Code, false));
}

TEST_CASE("VRMarkers for loop", "[vr,for]") {
  auto Code = std::string{R"code(int foo(int a){
        int s = 0;
        for(int i = 0; i < a; i++)
            s+=1;
        return s;
        })code"};

  auto ExpectedCode = dead::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      dead::ValueRangeInstrumenter::makeMarkerMacros(1) +
                      dead::ValueRangeInstrumenter::makeMarkerMacros(2) +
                      R"code(int foo(int a){
                              int s = 0;
                              VRMARKERMACROLE1_(s)
                              VRMARKERMACROGE1_(s)
                              VRMARKERMACROLE0_(a)
                              VRMARKERMACROGE0_(a)
                              for(int i = 0; i < a; i++)
                                  s+=1;
                              VRMARKERMACROLE2_(s)
                              VRMARKERMACROGE2_(s)
                              return s;
                              })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runVRInstrumenterOnCode(Code, false));
}

TEST_CASE("VRMarkers do while", "[vr,do]") {
  auto Code = std::string{R"code(int foo(int a){
        do{
        --a;
        }while(a);
        return a;
        })code"};

  auto ExpectedCode = dead::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      dead::ValueRangeInstrumenter::makeMarkerMacros(1) +
                      dead::ValueRangeInstrumenter::makeMarkerMacros(2) +
                      R"code(int foo(int a){
                              VRMARKERMACROLE0_(a)
                              VRMARKERMACROGE0_(a)
                              do{
                              VRMARKERMACROLE2_(a)
                              VRMARKERMACROGE2_(a)
                              --a;
                              }while(a);
                              VRMARKERMACROLE1_(a)
                              VRMARKERMACROGE1_(a)
                              return a;
                              })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runVRInstrumenterOnCode(Code, false));
}

TEST_CASE("VRMarkers switch", "[vr,switch]") {
  auto Code = std::string{R"code(int foo(int a){
        switch(a){
            case 1:
                return a;
            default: {
                return a+1;
            }
        }
        })code"};

  auto ExpectedCode = dead::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      dead::ValueRangeInstrumenter::makeMarkerMacros(1) +
                      dead::ValueRangeInstrumenter::makeMarkerMacros(2) +
                      R"code(int foo(int a){
                              VRMARKERMACROLE0_(a)
                              VRMARKERMACROGE0_(a)
                              switch(a){
                                  case 1:
                                      VRMARKERMACROLE1_(a)
                                      VRMARKERMACROGE1_(a)
                                      return a;
                                  default: {
                                      VRMARKERMACROLE2_(a)
                                      VRMARKERMACROGE2_(a)
                                      return a+1;
                                  }
                              }
                              })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runVRInstrumenterOnCode(Code, false));
}

TEST_CASE("VRMarkers unitialized", "[v]") {
  auto Code = std::string{R"code(int foo(){
          int b;
          b = 1;
          return b;
        })code"};

  CAPTURE(Code);
  compare_code(formatCode(Code), runVRInstrumenterOnCode(Code, false));
}

TEST_CASE("VRMarkers unitialized 2", "[v]") {
  auto Code = std::string{R"code(int foo(){
          int b;
          if (1)
            b = 1;
          return 2;
        })code"};

  CAPTURE(Code);
  compare_code(formatCode(Code), runVRInstrumenterOnCode(Code, false));
}
