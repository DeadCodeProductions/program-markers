#include "test_tool.h"
#include <catch2/catch.hpp>

TEST_CASE("MakeGlobalsStatic single global") {
  auto Code = R"code(int a;
    )code";

  auto ExpectedCode = R"code(static int a;
    )code";

  CAPTURE(Code);
  REQUIRE(formatCode(ExpectedCode) == runMakeGlobalsStaticOnCode(Code));
}

TEST_CASE("MakeGlobalsStatic two globals") {
  auto Code = R"code(int a;
    int b;
    )code";

  auto ExpectedCode = R"code(static int a;
    static int b;
    )code";

  CAPTURE(Code);
  REQUIRE(formatCode(ExpectedCode) == runMakeGlobalsStaticOnCode(Code));
}

TEST_CASE("MakeGlobalsStatic two globals already static") {
  auto Code = R"code(static int a;
    static int b;
    )code";

  auto ExpectedCode = R"code(static int a;
    static int b;
    )code";

  CAPTURE(Code);
  REQUIRE(formatCode(ExpectedCode) == runMakeGlobalsStaticOnCode(Code));
}

TEST_CASE("MakeGlobalsStatic extern") {
  auto Code = R"code(static int a;
    extern int b;
    )code";

  auto ExpectedCode = R"code(static int a;
    extern int b;
    )code";

  CAPTURE(Code);
  REQUIRE(formatCode(ExpectedCode) == runMakeGlobalsStaticOnCode(Code));
}

TEST_CASE("MakeGlobalsStatic two globals one already static") {
  auto Code = R"code(int a;
    static int b;
    )code";

  auto ExpectedCode = R"code(static int a;
    static int b;
    )code";

  CAPTURE(Code);
  REQUIRE(formatCode(ExpectedCode) == runMakeGlobalsStaticOnCode(Code));
}

TEST_CASE("MakeGlobalsStatic functions") {
  auto Code = R"code(int main() { return 0;}
    int foo(){ return 42;}
    static int bar(){ return 42;}
    )code";

  auto ExpectedCode = R"code(int main() { return 0;}
    static int foo(){ return 42;}
    static int bar(){ return 42;}
    )code";

  CAPTURE(Code);
  REQUIRE(formatCode(ExpectedCode) == runMakeGlobalsStaticOnCode(Code));
}

TEST_CASE("MakeGlobalsStatic functions and function declarations") {
  auto Code = R"code(int main() { return 0;}
    int foo(){ return 42;}
    static int bar(){ return 42;}
    int baz();
    )code";

  auto ExpectedCode = R"code(int main() { return 0;}
    static int foo(){ return 42;}
    static int bar(){ return 42;}
    int baz();
    )code";

  CAPTURE(Code);
  REQUIRE(formatCode(ExpectedCode) == runMakeGlobalsStaticOnCode(Code));
}

TEST_CASE("MakeGlobalsStatic functions and global variables") {
  auto Code = R"code(int a;
    static int b;
    int c;
    int main() { return 0;}
    int foo(){ return 42;}
    static int bar(){ return 42;}
    )code";

  auto ExpectedCode = R"code(static int a;
    static int b;
    static int c;
    int main() { return 0;}
    static int foo(){ return 42;}
    static int bar(){ return 42;}
    )code";

  CAPTURE(Code);
  REQUIRE(formatCode(ExpectedCode) == runMakeGlobalsStaticOnCode(Code));
}

TEST_CASE("MakeGlobalsStatic function with definition and declaration") {
  auto Code = R"code(
    int foo();
    int foo(){ return 42;}
    )code";

  auto ExpectedCode = R"code(
    static int foo();
    static int foo(){ return 42;}
    )code";

  CAPTURE(Code);
  REQUIRE(formatCode(ExpectedCode) == runMakeGlobalsStaticOnCode(Code));
}

TEST_CASE("MakeGlobalsStatic extern function") {
  auto Code = R"code(
    extern int foo(){ return 42;}
    )code";

  auto ExpectedCode = R"code(
    extern int foo(){ return 42;}
    )code";

  CAPTURE(Code);
  REQUIRE(formatCode(ExpectedCode) == runMakeGlobalsStaticOnCode(Code));
}
