#include <catch2/catch.hpp>

#include <ValueRangeInstrumenter.h>

#include "test_tool.h"

TEST_CASE("VRMarkers statement with two variables", "[vr]") {
  auto Code = std::string{R"code(int foo(int a, int b){
        return a+b;
        })code"};

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(1) +
                      "// MARKERS END\n" +
                      R"code(int foo(int a, int b){
                         VRMARKERMACRO1_(b,"int")
                         VRMARKERMACRO0_(a,"int")
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
                      markers::ValueRangeInstrumenter::makeMarkerMacros(1) +
                      "// MARKERS END\n" +
                      R"code(int foo(int a){
                         VRMARKERMACRO0_(a,"int")
                         if ( a > 0)
                           return a+1;
                         VRMARKERMACRO1_(a,"int")
                         return a+2; })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runVRInstrumenterOnCode(Code, false));
}

TEST_CASE("VRMarkers if-else compound", "[vr,if]") {
  auto Code = std::string{R"code(int foo(long a){
        if (a > 0) {
          return a+1;
        }
        else {
          return a+2; 
        }
        })code"};

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(1) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(2) +
                      "// MARKERS END\n" +
                      R"code(int foo(long a){
                         VRMARKERMACRO0_(a,"long")
                         if ( a > 0) {
                           VRMARKERMACRO1_(a,"long")
                           return a+1;
                         } else {
                           VRMARKERMACRO2_(a,"long")
                           return a+2; 
                         }
                         })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runVRInstrumenterOnCode(Code, false));
}

TEST_CASE("VRMarkers for loop", "[vr,for]") {
  auto Code = std::string{R"code(int foo(int a){
        long s = 0;
        for(int i = 0; i < a; i++)
            s+=1;
        return s;
        })code"};

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(1) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(2) +
                      "// MARKERS END\n" +
                      R"code(int foo(int a){
                              long s = 0;
                              VRMARKERMACRO1_(s,"long")
                              VRMARKERMACRO0_(a,"int")
                              for(int i = 0; i < a; i++)
                                  s+=1;
                              VRMARKERMACRO2_(s,"long")
                              return s;
                              })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runVRInstrumenterOnCode(Code, false));
}

TEST_CASE("VRMarkers do while", "[vr,do]") {
  auto Code = std::string{R"code(int foo(unsigned int a){
        do{
        --a;
        }while(a);
        return a;
        })code"};

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(1) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(2) +
                      "// MARKERS END\n" +
                      R"code(int foo(unsigned int a){
                              VRMARKERMACRO0_(a,"unsigned int")
                              do{
                              VRMARKERMACRO2_(a,"unsigned int")
                              --a;
                              }while(a);
                              VRMARKERMACRO1_(a,"unsigned int")
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
                      markers::ValueRangeInstrumenter::makeMarkerMacros(1) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(2) +
                      "// MARKERS END\n" +
                      R"code(int foo(int a){
                              VRMARKERMACRO0_(a,"int")
                              switch(a){
                                  case 1:
                                      VRMARKERMACRO1_(a,"int")
                                      return a;
                                  default: {
                                      VRMARKERMACRO2_(a,"int")
                                      return a+1;
                                  }
                              }
                              })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runVRInstrumenterOnCode(Code, false));
}

TEST_CASE("VRMarkers unitialized", "[vr,switch]") {
  auto Code = std::string{R"code(int foo(){
        int a;
        a = 0;
        return a;
        })code"};

  CAPTURE(Code);
  compare_code(formatCode(Code), runVRInstrumenterOnCode(Code, false));
}

TEST_CASE("VRMarkers Variable in Macro", "[vr]") {
  auto Code = std::string{R"code(
  #define MACRO(a) a*2

  int foo(int a){
    int b = MACRO(a);
    return b;
  })code"};

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(0) +
                      markers::ValueRangeInstrumenter::makeMarkerMacros(1) +
                      "// MARKERS END\n" +
                      R"code(
                      #define MACRO(a) a*2

                      int foo(int a){
                        VRMARKERMACRO0_(a,"int")
                        int b = MACRO(a);
                        VRMARKERMACRO1_(b,"int")
                        return b;
                      })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runVRInstrumenterOnCode(Code, false));
}

TEST_CASE("VRMarkers enums", "[vr]") {
  auto Code = std::string{R"code(
        enum E {A,B,C};

        class C {
        public:
            enum E2 {A,B};
            E2 test() const {
                return E2::A;
            }
        };
        using E2 = C::E2;
        int foo(E e, const class C c){
        const E2 e2 = c.test();
        return e + e2;
        })code"};

  CAPTURE(Code);
  compare_code(formatCode(Code), runVRInstrumenterOnCode(Code, false));
}
