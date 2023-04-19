#include <catch2/catch.hpp>

#include <ValueRangeInstrumenter.h>

#include "test_tool.h"

TEST_CASE("VRMarkers statement with two variables", "[vr]") {
  auto Code = std::string{R"code(int foo(int a, int b){
        return a+b;
        })code"};

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(2) +
                      "// MARKERS END\n" +
                      R"code(int foo(int a, int b){
                         VRMARKERMACROLE2_(b)
                         VRMARKERMACROGE3_(b)
                         VRMARKERMACROLE0_(a)
                         VRMARKERMACROGE1_(a)
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

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(2) +
                      "// MARKERS END\n" +
                      R"code(int foo(int a){
                         VRMARKERMACROLE0_(a)
                         VRMARKERMACROGE1_(a)
                         if ( a > 0)
                           return a+1;
                         VRMARKERMACROLE2_(a)
                         VRMARKERMACROGE3_(a)
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

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(2) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(4) +
                      "// MARKERS END\n" +
                      R"code(int foo(int a){
                         VRMARKERMACROLE0_(a)
                         VRMARKERMACROGE1_(a)
                         if ( a > 0) {
                           VRMARKERMACROLE2_(a)
                           VRMARKERMACROGE3_(a)
                           return a+1;
                         } else {
                           VRMARKERMACROLE4_(a)
                           VRMARKERMACROGE5_(a)
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

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(2) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(4) +
                      "// MARKERS END\n" +
                      R"code(int foo(int a){
                              int s = 0;
                              VRMARKERMACROLE2_(s)
                              VRMARKERMACROGE3_(s)
                              VRMARKERMACROLE0_(a)
                              VRMARKERMACROGE1_(a)
                              for(int i = 0; i < a; i++)
                                  s+=1;
                              VRMARKERMACROLE4_(s)
                              VRMARKERMACROGE5_(s)
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

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(2) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(4) +
                      "// MARKERS END\n" +
                      R"code(int foo(int a){
                              VRMARKERMACROLE0_(a)
                              VRMARKERMACROGE1_(a)
                              do{
                              VRMARKERMACROLE4_(a)
                              VRMARKERMACROGE5_(a)
                              --a;
                              }while(a);
                              VRMARKERMACROLE2_(a)
                              VRMARKERMACROGE3_(a)
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

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(2) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(4) +
                      "// MARKERS END\n" +
                      R"code(int foo(int a){
                              VRMARKERMACROLE0_(a)
                              VRMARKERMACROGE1_(a)
                              switch(a){
                                  case 1:
                                      VRMARKERMACROLE2_(a)
                                      VRMARKERMACROGE3_(a)
                                      return a;
                                  default: {
                                      VRMARKERMACROLE4_(a)
                                      VRMARKERMACROGE5_(a)
                                      return a+1;
                                  }
                              }
                              })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runVRInstrumenterOnCode(Code, false));
}
