#include "test_tool.hpp"
#include <catch2/catch.hpp>

TEST_CASE("BranchInstrumenter if without else", "[if]") {
    auto Code = std::string{R"code(int foo(int a){
        if (a > 0))code"};
    Code += GENERATE(
        R"code(
                return 1;)code",
        R"code({
        return 1; 
        })code");
    Code += R"code(       
     return 0;
    }
    )code";
    Code = formatCode(Code);

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    int foo(int a){
        #if !defined(DeleteDCEMarkerBlock0_) || !defined(DeleteDCEMarkerBlock1_)

        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        if (
        #endif
        a > 0
        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        )
        #else

            ;
        #endif

        #ifndef DeleteDCEMarkerBlock1_

        {
            DCEMarker1_();
            return 1;
        }
        #endif

        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        else {
            DCEMarker0_();
        }
        #endif
        #endif

        return 0;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if-else", "[if]") {
    auto Code = std::string{R"code(int foo(int a){
        if (a > 0))code"};

    Code += GENERATE(
        R"code({
        a = 1;
        }
        )code",
        R"code(
        a = 1;
        )code");

    Code += GENERATE(
        R"code(else{
        a = 0;
        }
        )code",
        R"code(else
        a = 0;
        )code");

    Code += R"code(
        return a;
    }
    )code";
    Code = formatCode(Code);

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    int foo(int a){
        #if !defined(DeleteDCEMarkerBlock0_) || !defined(DeleteDCEMarkerBlock1_)

        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        if (
        #endif
            a > 0
        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        )
        #else

            ;
        #endif

        #ifndef DeleteDCEMarkerBlock1_

        {
            DCEMarker1_();
            a = 1;
        } 
        #endif

        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        else

        #endif

        #ifndef DeleteDCEMarkerBlock0_

        {
            DCEMarker0_();
            a = 0;
        }
        #endif
        
        #endif

        return a;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if with return macro", "[if][macro]") {
    auto Code = std::string{R"code(#define R return

    int foo(int a){
        if (a > 0))code"};

    Code += GENERATE(R"code(
      R 0;
    )code",
                     R"code({
      R 0;
    }
    )code");
    Code +=
        R"code(       return a;
    }
    )code";
    Code = formatCode(Code);

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    #define R return

    int foo(int a){
        #if !defined(DeleteDCEMarkerBlock0_) || !defined(DeleteDCEMarkerBlock1_)

        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        if (
        #endif
        a > 0
        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

            )
        #else

            ;
        #endif

        #ifndef DeleteDCEMarkerBlock1_

        {
            DCEMarker1_();
            R 0;
        }
        #endif

        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        else {
            DCEMarker0_();
        }
        #endif
        #endif

        return a;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if with return macro 2", "[if][macro]") {
    auto Code = std::string{R"code(#define R return 0

    int foo(int a){
        if (a > 0))code"};

    Code += GENERATE(R"code(
      R;
    )code",
                     R"code({
      R;
    }
    )code");
    Code +=
        R"code(return a;
    }
    )code";
    Code = formatCode(Code);

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    #define R return 0

    int foo(int a){
        #if !defined(DeleteDCEMarkerBlock0_) || !defined(DeleteDCEMarkerBlock1_)

        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        if (
        #endif
        a > 0
        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

            )
        #else

            ;
        #endif

        #ifndef DeleteDCEMarkerBlock1_

        {
            DCEMarker1_();
            R;
        }
        #endif

        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        else {
            DCEMarker0_();
        }
        #endif
        #endif

        return a;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if with return macro 3", "[if][macro]") {
    auto Code = std::string{R"code(#define R return 0;

    int foo(int a){
        if (a > 0))code"};

    Code += GENERATE(R"code(
      R
    )code",
                     R"code({
      R
    }
    )code");
    Code +=
        R"code(
        return a;
    }
    )code";
    Code = formatCode(Code);

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    #define R return 0;

    int foo(int a){
        #if !defined(DeleteDCEMarkerBlock0_) || !defined(DeleteDCEMarkerBlock1_)

        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        if (
        #endif
        a > 0
        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

            )
        #else

            ;
        #endif

        #ifndef DeleteDCEMarkerBlock1_

        {
            DCEMarker1_();
            R
        }
        #endif

        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        else {
            DCEMarker0_();
        }
        #endif
        #endif

        return a;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter nested if with macro", "[if][nested][macro]") {
    auto Code = std::string{R"code(#define A a = 1;
    int foo(int a){
        if (a > 0)
        )code"};

    bool compoundThen = GENERATE(true, false);
    if (compoundThen)
        Code += R"code({ )code";

    Code += R"code(if (a==1) )code";

    Code += GENERATE(R"code(A)code", R"code({A})code");
    Code += R"code(
        else
        )code";
    Code += GENERATE(R"code(a = 2;)code", R"code({a = 2;})code");
    if (compoundThen)
        Code += R"code(} )code";
    Code += R"code(   
        return 0;
    }
    )code";

    Code = formatCode(Code);

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    void DCEMarker3_(void);
    #define A a = 1;
    int foo(int a){
        #if !defined(DeleteDCEMarkerBlock0_) || !defined(DeleteDCEMarkerBlock1_)

        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        if (
            #endif
        a > 0
        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        )
        #else

            ;
        #endif

    #ifndef DeleteDCEMarkerBlock1_

        {
            DCEMarker1_();
            #if !defined(DeleteDCEMarkerBlock2_) || !defined(DeleteDCEMarkerBlock3_)

            #if !defined(DeleteDCEMarkerBlock2_) && !defined(DeleteDCEMarkerBlock3_)

                if (
            #endif
                    a==1
            #if !defined(DeleteDCEMarkerBlock2_) && !defined(DeleteDCEMarkerBlock3_)

                ) 
        #else

            ;
            #endif

            #ifndef DeleteDCEMarkerBlock3_

                      {
                          DCEMarker3_();
                          A
                      }
            #endif
            
            #if !defined(DeleteDCEMarkerBlock2_) && !defined(DeleteDCEMarkerBlock3_)

                else 

            #endif

            #ifndef DeleteDCEMarkerBlock2_

                {
                    DCEMarker2_();
                    a = 2;
                }
            #endif

        #endif
        }
    #endif

    #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        else {
            DCEMarker0_();
        }
        #endif
        #endif

        return 0;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter nested if with return", "[if][return][nested]") {
    auto Code = std::string{R"code(int foo(int a){
        if (a >= 0)
        )code"};

    bool compoundThen = GENERATE(true, false);
    if (compoundThen)
        Code += R"code({ )code";

    Code += R"code(if (a>=0) 
    )code";

    Code += GENERATE(R"code(return 1;)code", R"code({return 1;})code");
    if (compoundThen)
        Code += R"code(} )code";
    Code += R"code(   
        return 0;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    void DCEMarker3_(void);
    int foo(int a){
      #if !defined(DeleteDCEMarkerBlock0_) || !defined(DeleteDCEMarkerBlock1_)

      #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        if (
            #endif
        a >= 0
        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        )
        #else

            ;
        #endif

        #ifndef DeleteDCEMarkerBlock1_

        {
            DCEMarker1_();
            #if !defined(DeleteDCEMarkerBlock2_) || !defined(DeleteDCEMarkerBlock3_)

            #if !defined(DeleteDCEMarkerBlock2_) && !defined(DeleteDCEMarkerBlock3_)

              if (
                  #endif
              a >= 0
              #if !defined(DeleteDCEMarkerBlock2_) && !defined(DeleteDCEMarkerBlock3_)

              )
            #else

            ;
              #endif

              #ifndef DeleteDCEMarkerBlock3_

              {
                DCEMarker3_();
                return 1;
              }
              #endif

            #if !defined(DeleteDCEMarkerBlock2_) && !defined(DeleteDCEMarkerBlock3_)

                else {
                    DCEMarker2_();
                }
            #endif
            #endif
                }
            #endif

    #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        else {
            DCEMarker0_();
        }
    #endif
    #endif

        return 0;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if return macro and comment",
          "[loop][if][macro][return]") {

    auto Code = R"code(#define X 0
    int foo() {
    if (1)
        return X /* comment */;
    })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    #define X 0
    int foo() {
     #if !defined(DeleteDCEMarkerBlock0_) || !defined(DeleteDCEMarkerBlock1_)

      #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        if (
            #endif
        1
        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        )
        #else

            ;
        #endif

        #ifndef DeleteDCEMarkerBlock1_

    {
        DCEMarker1_();
        return X /* comment */;
    }
        #endif

    #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        else {
            DCEMarker0_();
        }
    #endif
    #endif
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if return macro", "[loop][if][macro][return]") {

    auto Code = R"code(#define BUG
    void foo() {
    if (1)
        return BUG;
    })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    #define BUG
    void foo() {
     #if !defined(DeleteDCEMarkerBlock0_) || !defined(DeleteDCEMarkerBlock1_)

      #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        if (
            #endif
        1
        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        )
        #else

            ;
        #endif

        #ifndef DeleteDCEMarkerBlock1_

    {
        DCEMarker1_();
        return BUG;
    }
        #endif

    #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        else {
            DCEMarker0_();
        }
    #endif
    #endif
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if with semi return macro",
          "[loop][if][macro][return]") {

    auto Code = R"code(#define BUG ;
    void foo() {
    if (1)
        return BUG
    })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    #define BUG ;
    void foo() {
     #if !defined(DeleteDCEMarkerBlock0_) || !defined(DeleteDCEMarkerBlock1_)

      #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        if (
            #endif
        1
        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        )
        #else

            ;
        #endif

        #ifndef DeleteDCEMarkerBlock1_

    {
        DCEMarker1_();
        return BUG
    }
        #endif

    #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        else {
            DCEMarker0_();
        }
    #endif
    #endif
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if-else with semi return macro",
          "[loop][if][macro][return]") {

    auto Code = R"code(#define BUG ;
    void foo() {
    if (1)
        return BUG
    else
        return;
    })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    #define BUG ;
    void foo() {
     #if !defined(DeleteDCEMarkerBlock0_) || !defined(DeleteDCEMarkerBlock1_)

      #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        if (
            #endif
        1
        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        )
        #else

            ;
        #endif

        #ifndef DeleteDCEMarkerBlock1_

    {
        DCEMarker1_();
        return BUG
    }
        #endif

    #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        else 

    #endif

    #ifndef DeleteDCEMarkerBlock0_

        {
            DCEMarker0_();
            return;
        }
    #endif

    #endif
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if-else nested with while", "[if][loop][while]") {
    std::string Code = R"code(int foo(int a){
      if (a > 0))code";

    bool compoundThen = GENERATE(true, false);
    if (compoundThen)
        Code += R"code({)code";
    Code += R"code(
         while(a--))code";

    Code += GENERATE(
        R"code( {
    return 1;
    }
    )code",
        R"code(
    return 1;
    )code");

    if (compoundThen)
        Code += R"code(}
        )code";

    Code += GENERATE(R"code( else
                    a = 0;
                )code",
                     R"code( else {
                    a = 0;
                }
                )code");

    Code += R"code(return a;
                    })code";
    Code = formatCode(Code);

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    int foo(int a) {
        #if !defined(DeleteDCEMarkerBlock0_) || !defined(DeleteDCEMarkerBlock1_)

        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        if (
        #endif
            a > 0
        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        )
        #else

            ;
        #endif

        #ifndef DeleteDCEMarkerBlock1_

        {
            DCEMarker1_();
        #ifndef DeleteDCEMarkerBlock2_

            while(
        #endif
            a--
        #ifndef DeleteDCEMarkerBlock2_

            )
        #endif

        #ifndef DeleteDCEMarkerBlock2_

            {
                DCEMarker2_();
                return 1;
            }
        #endif
        } 
        #endif

        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        else

        #endif
        
        #ifndef DeleteDCEMarkerBlock0_

        {
            DCEMarker0_();
            a = 0;
        }
        #endif

        #endif

        return a;
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter while stmt", "[while][loop]") {

    auto Code = std::string{R"code(int foo(int a){
        int b = 0;
        while(true))code"};

    Code += GENERATE(
        R"code(
    return 0;
    )code",
        R"code( {
    return 0;
    }
    )code");
    Code += R"code(return b;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    int foo(int a){
        int b = 0;
        #ifndef DeleteDCEMarkerBlock0_

            while(
        #endif
            true
        #ifndef DeleteDCEMarkerBlock0_

            )
        #endif

        #ifndef DeleteDCEMarkerBlock0_

            {
                DCEMarker0_();
                return 0;
            }
        #endif

        return b;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter for stmt nested if with return",
          "[for][if][nested][return]") {

    auto Code = std::string{R"code(int foo(int a){
        int b = 0;
        for (int i = 0; i < a; ++i))code"};

    bool compoundFor = GENERATE(true, false);
    if (compoundFor)
        Code += R"code({)code";
    Code += R"code(
            if (i == 3)
            )code";
    Code += GENERATE(R"code(return b;)code", R"code({return b;})code");
    Code += R"code(
            else
            )code";
    Code += GENERATE(R"code(++b;)code", R"code({++b;})code");

    if (compoundFor)
        Code += R"code(})code";
    Code += R"code(
        return b;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    int foo(int a){
        int b = 0;
        #ifndef DeleteDCEMarkerBlock0_

        for (
        #else
        {
        #endif
        int i = 0; i < a;
        #ifndef DeleteDCEMarkerBlock0_

        ++i)
        #endif

        #ifndef DeleteDCEMarkerBlock0_

        {
            DCEMarker0_();
        #if !defined(DeleteDCEMarkerBlock1_) || !defined(DeleteDCEMarkerBlock2_)

        #if !defined(DeleteDCEMarkerBlock1_) && !defined(DeleteDCEMarkerBlock2_)

        if (
        #endif
            i == 3
        #if !defined(DeleteDCEMarkerBlock1_) && !defined(DeleteDCEMarkerBlock2_)

        )
        #else

            ;
        #endif

        #ifndef DeleteDCEMarkerBlock2_

            {
                DCEMarker2_();
                return b;
            }
            #endif

        #if !defined(DeleteDCEMarkerBlock1_) && !defined(DeleteDCEMarkerBlock2_)

             else 

        #endif

        #ifndef DeleteDCEMarkerBlock1_

             {
                DCEMarker1_();
                ++b;
            }
        #endif

        #endif

        #endif
        }
        return b;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter for stmt nested if with return and extra stmt",
          "[for][if][nested][return]") {

    auto Code = std::string{R"code(int foo(int a){
        int b = 0;
        for (int i = 0; i < a; ++i){)code"};
    Code += R"code(
            if (i == 3)
            )code";
    Code += GENERATE(R"code(return b;)code", R"code({return b;})code");
    Code += R"code(
            else
            )code";
    Code += GENERATE(R"code(++b;)code", R"code({++b;})code");

    Code += R"code(
        ++b;
        }
        return b;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    int foo(int a){
        int b = 0;
        #ifndef DeleteDCEMarkerBlock0_

        for (
        #else
        {
        #endif
        int i = 0; i < a;
        #ifndef DeleteDCEMarkerBlock0_

        ++i)
        #endif

        #ifndef DeleteDCEMarkerBlock0_

        {
            DCEMarker0_();
        #if !defined(DeleteDCEMarkerBlock1_) || !defined(DeleteDCEMarkerBlock2_)

        #if !defined(DeleteDCEMarkerBlock1_) && !defined(DeleteDCEMarkerBlock2_)

        if (
        #endif
            i == 3
        #if !defined(DeleteDCEMarkerBlock1_) && !defined(DeleteDCEMarkerBlock2_)

        )
        #else

            ;
        #endif

        #ifndef DeleteDCEMarkerBlock2_

            {
                DCEMarker2_();
                return b;
            }
            #endif

        #if !defined(DeleteDCEMarkerBlock1_) && !defined(DeleteDCEMarkerBlock2_)

             else 

        #endif

        #ifndef DeleteDCEMarkerBlock1_

             {
                DCEMarker1_();
                ++b;
            }
        #endif

        #endif

        ++b;

        #endif
        }
        return b;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter for stmt with return", "[for][return]") {

    auto Code = R"code(int foo(int a){
        int b = 0;
        for (int i = 0; i < a; ++i)
            return i;
        return b;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    int foo(int a){
        int b = 0;
        #ifndef DeleteDCEMarkerBlock0_

        for (
        #else
        {
        #endif
        int i = 0; i < a; 
        #ifndef DeleteDCEMarkerBlock0_

        ++i)
        #endif

        #ifndef DeleteDCEMarkerBlock0_

        {
            DCEMarker0_();
            return i;
        #endif
        }
        return b;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter do while stmt with return", "[do][return]") {

    auto Code = std::string{R"code(int foo(int a){
        int b = 0;
        do 
        )code"};
    Code += GENERATE(R"code(return b;)code", R"code({return b;})code");
    Code += R"code(while(b<10);
        return b;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    int foo(int a){
        int b = 0;
        #ifndef DeleteDCEMarkerBlock0_

        do {
          DCEMarker0_();
          return b;
        } while(b<10);
        #endif

        return b;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter do while and if with return",
          "[if][dowhile][return]") {
    auto Code = std::string{R"code(#define X 1
    int foo(int a) {
        do )code"};

    bool compoundDo = GENERATE(true, false);
    if (compoundDo)
        Code += R"code({)code";
    Code += R"code(
            if (a + 1 == 2)
             )code";
    Code += GENERATE(R"code(return X;)code", R"code({return X;})code");
    if (compoundDo)
        Code += R"code(
        })code";
    Code += R"code(
         while (++a);
        return 0;
    })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
        void DCEMarker1_(void);
        void DCEMarker2_(void);
        #define X 1
        int foo(int a) {
          #ifndef DeleteDCEMarkerBlock0_

          do {
            DCEMarker0_();
            #if !defined(DeleteDCEMarkerBlock1_) || !defined(DeleteDCEMarkerBlock2_)

            #if !defined(DeleteDCEMarkerBlock1_) && !defined(DeleteDCEMarkerBlock2_)

            if (
                #endif
            a + 1 == 2
            #if !defined(DeleteDCEMarkerBlock1_) && !defined(DeleteDCEMarkerBlock2_)

            )
            #else

            ;
            #endif

            #ifndef DeleteDCEMarkerBlock2_

            {
            DCEMarker2_();
            return X;
            }
            #endif

            #if !defined(DeleteDCEMarkerBlock1_) && !defined(DeleteDCEMarkerBlock2_)

            else {
                DCEMarker1_();
            }
            #endif
            #endif

          } while (++a);
          #endif

          return 0;
    })code";
    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter do while and if else with return",
          "[if][dowhile][return]") {
    auto Code = std::string{R"code(int foo(int a) {
                if (a))code"};

    bool compoundDo = GENERATE(true, false);
    if (compoundDo)
        Code += R"code({)code";

    Code += R"code(
                do )code";
    Code += GENERATE(R"code(--a;)code", R"code({--a;})code");
    Code += R"code(
        while(a);
        )code";

    if (compoundDo)
        Code += R"code(})code";

    Code += R"code(else
        )code";

    Code += GENERATE(R"code(return 1;)code", R"code({return 1;})code");
    Code += R"code(       
    return 0;
    })code";

    Code = formatCode(Code);

    auto ExpectedCode = R"code(void DCEMarker0_(void);
        void DCEMarker1_(void);
        void DCEMarker2_(void);
        int foo(int a) {
            #if !defined(DeleteDCEMarkerBlock0_) || !defined(DeleteDCEMarkerBlock1_)

            #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

            if (
                #endif
            a
            #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

            )
            #else

            ;
            #endif

            #ifndef DeleteDCEMarkerBlock1_

            {
                DCEMarker1_();
              #ifndef DeleteDCEMarkerBlock2_

              do {
                DCEMarker2_();
                --a;
              } while (a);
              #endif
            }
            #endif

            #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

            else 

            #endif

            #ifndef DeleteDCEMarkerBlock0_

            {
                DCEMarker0_();
                return 1;
            }
            #endif

            #endif

          return 0;
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if dowhile with nested macro",
          "[if][do][macro][return]") {

    auto Code = std::string{R"code(#define M
    #define bar    \
    do {           \
    } while (0) M

    void foo() {
       if (1)
       )code"};
    Code += GENERATE(R"code(bar;)code", R"code({bar;})code");
    Code += R"code(   })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
        void DCEMarker1_(void);
        #define M
        #define bar    \
        do  {          \
        } while (0) M

        void foo() {
            #if !defined(DeleteDCEMarkerBlock0_) || !defined(DeleteDCEMarkerBlock1_)

            #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

            if (
                #endif
            1
            #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

            )
            #else

            ;
            #endif

            #ifndef DeleteDCEMarkerBlock1_

            {
                DCEMarker1_();
              bar; 
            }
            #endif

            #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

            else
            {
                DCEMarker0_();
            }
            #endif
            #endif
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if while do and braces without whitespace",
          "[if][do][macro][return]") {

    auto Code = R"code(
    void foo() {
        while (1) {}
        if (1) {}
        do {} while(1);
        if (1);
    })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    void DCEMarker3_(void);
    void DCEMarker4_(void);
    void DCEMarker5_(void);

    void foo() {
      #ifndef DeleteDCEMarkerBlock0_

        while (
      #endif
        1
      #ifndef DeleteDCEMarkerBlock0_

        ) 
      #endif

      #ifndef DeleteDCEMarkerBlock0_

        {
        DCEMarker0_();
        }
      #endif

#if !defined(DeleteDCEMarkerBlock1_) || !defined(DeleteDCEMarkerBlock2_)

#if !defined(DeleteDCEMarkerBlock1_) && !defined(DeleteDCEMarkerBlock2_)

  if (
  #endif
        1
  #if !defined(DeleteDCEMarkerBlock1_) && !defined(DeleteDCEMarkerBlock2_)

  )
        #else

            ;
  #endif

#ifndef DeleteDCEMarkerBlock2_

  {
      DCEMarker2_();
    }
  #endif

#if !defined(DeleteDCEMarkerBlock1_) && !defined(DeleteDCEMarkerBlock2_)

  else {
      DCEMarker1_();
    }
  #endif
  #endif

  #ifndef DeleteDCEMarkerBlock3_

        do {
        DCEMarker3_();
        } while(1);
      #endif

#if !defined(DeleteDCEMarkerBlock4_) || !defined(DeleteDCEMarkerBlock5_)

#if !defined(DeleteDCEMarkerBlock4_) && !defined(DeleteDCEMarkerBlock5_)

  if (
  #endif
        1
  #if !defined(DeleteDCEMarkerBlock4_) && !defined(DeleteDCEMarkerBlock5_)

  )
        #else

            ;
  #endif
  #ifndef DeleteDCEMarkerBlock5_

  {
      DCEMarker5_();
      ;
    }
  #endif

#if !defined(DeleteDCEMarkerBlock4_) && !defined(DeleteDCEMarkerBlock5_)

  else {
      DCEMarker4_();
    }
  #endif
  #endif
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter switch", "[switch][return]") {
    auto Code = R"code(int foo(int a){
        switch(a){
        case 1:
            a = 2;
            break;
        case 2:
        case 3:
            break;
        case 4:
            return 3;
        case 5:{
            a = 5;
        }
        default:
            a = 42;
        }
        return a;
    }
    )code";
    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    void DCEMarker3_(void);
    void DCEMarker4_(void);
    void DCEMarker5_(void);
    int foo(int a){
        switch(a){
#ifndef DeleteDCEMarkerBlock0_

        case 1: 
            DCEMarker0_();
            a = 2;
            break;

#endif
#ifndef DeleteDCEMarkerBlock1_

        case 2:
          DCEMarker1_();

#endif
#ifndef DeleteDCEMarkerBlock5_

        case 3:
           DCEMarker5_();
           break;

#endif
#ifndef DeleteDCEMarkerBlock2_

        case 4:
          DCEMarker2_();
          return 3;

#endif
#ifndef DeleteDCEMarkerBlock3_

        case 5:
          DCEMarker3_();
          {a = 5;}

#endif
#ifndef DeleteDCEMarkerBlock4_

        default:
          DCEMarker4_();
          a = 42;

#endif
        }
        return a;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter switch if and macro", "[if][switch][macro]") {
    auto Code = R"code(#define TEST bar

                        int bar();

                        void baz(int a) {
                            switch (a) {
                            case 1:
                                TEST();
                            }
                        }

                        void foo(int a) {
                            if (a)
                                a = 1;
                        })code";
    auto ExpectedCode = R"code(void DCEMarker0_(void);
                        void DCEMarker1_(void);
                        void DCEMarker2_(void);
                        #define TEST bar

                        int bar();

                        void baz(int a) {
                            switch (a) {
                            #ifndef DeleteDCEMarkerBlock0_

                            case 1:
                                DCEMarker0_();
                                TEST();

                            #endif
                            }
                        }

                        void foo(int a) {
                            #if !defined(DeleteDCEMarkerBlock1_) || !defined(DeleteDCEMarkerBlock2_)

                            #if !defined(DeleteDCEMarkerBlock1_) && !defined(DeleteDCEMarkerBlock2_)

                              if (
                              #endif
                                    a
                              #if !defined(DeleteDCEMarkerBlock1_) && !defined(DeleteDCEMarkerBlock2_)

                              )
                                    #else

                                        ;
                              #endif

                            #ifndef DeleteDCEMarkerBlock2_

                            {
                                DCEMarker2_();
                                a = 1;
                                }
                            #endif

                              #if !defined(DeleteDCEMarkerBlock1_) && !defined(DeleteDCEMarkerBlock2_)

                              else {
                                  DCEMarker1_();
                              }
                            #endif
                            #endif
                        })code";
    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter switch if with return and macro",
          "[if][return][switch][macro]") {

    auto Code = R"code(#define FFFF 1
    int foo() {
        if (1)
          switch (1) {
            default:
              return FFFF;
            }
        else if (1)
            return FFFF;
    })code";
    auto ExpectedCode = R"code(void DCEMarker0_(void);
                        void DCEMarker1_(void);
                        void DCEMarker2_(void);
                        void DCEMarker3_(void);
                        void DCEMarker4_(void);
#define FFFF 1
    int foo() {
            #if !defined(DeleteDCEMarkerBlock0_) || !defined(DeleteDCEMarkerBlock1_)
            
            #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)
            
              if (
              #endif
                    1
              #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)
            
              )
              #else
            
                  ;
              #endif

        #ifndef DeleteDCEMarkerBlock1_

        {
          DCEMarker1_();
          switch (1) {
        #ifndef DeleteDCEMarkerBlock4_
        
            default:
              DCEMarker4_();
              return FFFF;

              #endif
            }
        } 
        #endif

        #if !defined(DeleteDCEMarkerBlock0_) && !defined(DeleteDCEMarkerBlock1_)

        else 

        #endif
        
        #ifndef DeleteDCEMarkerBlock0_

        {
          DCEMarker0_();
          #if !defined(DeleteDCEMarkerBlock2_) || !defined(DeleteDCEMarkerBlock3_)

              #if !defined(DeleteDCEMarkerBlock2_) && !defined(DeleteDCEMarkerBlock3_)

            if (
              #endif
                      1
              #if !defined(DeleteDCEMarkerBlock2_) && !defined(DeleteDCEMarkerBlock3_)

                )
              #else

                    ;
              #endif


        #ifndef DeleteDCEMarkerBlock3_

            {
            DCEMarker3_();
            return FFFF;
            }
        #endif

        #if !defined(DeleteDCEMarkerBlock2_) && !defined(DeleteDCEMarkerBlock3_)

            else {
                DCEMarker2_();
              }
          #endif
        #endif
        }
        #endif
        
        #endif
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

