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
        #endif

        #ifndef DeleteDCEMarkerBlock1_

        {
            DCEMarker1_();
            a = 1;
        } 
        #endif

        #ifndef DeleteDCEMarkerBlock1_

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

TEST_CASE("BranchInstrumenter nested if", "[if][nested]") {
    auto Code = std::string{R"code(#define A a = 1;
    int foo(int a){
        if (a > 0){
            if (a==1) {
                A
            }
            else 
                a = 2;
            
        }
        return 0;
    }
    )code"};
    Code = formatCode(Code);

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    void DCEMarker3_(void);
    #define A a = 1;
    int foo(int a){
        DCEMarker0_();
        if (a > 0){
        #ifndef DeleteDCEMarkerBlock1_

            DCEMarker1_();
            if (a==1) {
            #ifndef DeleteDCEMarkerBlock2_

                DCEMarker2_();
                A
            #endif
            }
            else {
            #ifndef DeleteDCEMarkerBlock3_

                DCEMarker3_();
                a = 2;
            #endif
            }

        #endif
        }
        return 0;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter nested if with return", "[if][return][nested]") {
    auto Code = R"code(int foo(int a){
        if (a >= 0) {
            if (a >= 0) {
                return 1;
            }
        }
        return 0;
    }
    )code";
    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    void DCEMarker3_(void);
    void DCEMarker4_(void);
    int foo(int a){
        DCEMarker0_();
        if (a >= 0) {
        #ifndef DeleteDCEMarkerBlock2_

            DCEMarker2_();
            if (a >= 0) {
            #ifndef DeleteDCEMarkerBlock4_

                DCEMarker4_();
                return 1;
            #endif
            }
            DCEMarker3_();
        #endif
        }
        DCEMarker1_();
        return 0;
    }
    )code";

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

        #ifndef DeleteDCEMarkerBlock1_

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

    auto Code = R"code(int foo(int a){
        int b = 0;
        for (int i = 0; i < a; ++i)
            if (i == 3)
                return b;
            else 
                ++b;
        return b;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    void DCEMarker3_(void);
    void DCEMarker4_(void);
    void DCEMarker5_(void);
    int foo(int a){
        DCEMarker0_();
        int b = 0;
        for (int i = 0; i < a; ++i){
        #ifndef DeleteDCEMarkerBlock2_

            DCEMarker2_();
            if (i == 3){
            #ifndef DeleteDCEMarkerBlock4_

                DCEMarker4_();
                return b;
            #endif
            } else {
            #ifndef DeleteDCEMarkerBlock5_

                DCEMarker5_();
                ++b;
            #endif
            }
            DCEMarker3_();
        #endif
        }
        DCEMarker1_();
        return b;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter for stmt nested if with return and extra stmt",
          "[for][if][nested][return]") {

    auto Code = R"code(int foo(int a){
        int b = 0;
        for (int i = 0; i < a; ++i){
            if (i == 3)
                return b;
            else 
                ++b;
            ++b;
        }
        return b;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    void DCEMarker3_(void);
    void DCEMarker4_(void);
    void DCEMarker5_(void);
    int foo(int a){
        DCEMarker0_();
        int b = 0;
        for (int i = 0; i < a; ++i){
        #ifndef DeleteDCEMarkerBlock2_
       
            DCEMarker2_();
            if (i == 3){
            #ifndef DeleteDCEMarkerBlock4_

                DCEMarker4_();
                return b;
            #endif
            } else {
            #ifndef DeleteDCEMarkerBlock5_

                DCEMarker5_();
                ++b;
            #endif
            }
            DCEMarker3_();
            ++b;
        #endif
        }
        DCEMarker1_();
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
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    int foo(int a){
        DCEMarker0_();
        int b = 0;
        for (int i = 0; i < a; ++i){
       #ifndef DeleteDCEMarkerBlock2_

            DCEMarker2_();
            return i;
        #endif
        }
        DCEMarker1_();
        return b;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter do while stmt with return", "[do][return]") {

    auto Code = R"code(int foo(int a){
        int b = 0;
        do 
          return b;
        while(b<10);
        return b;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    int foo(int a){
        DCEMarker0_();
        int b = 0;
        do {
        #ifndef DeleteDCEMarkerBlock2_

          DCEMarker2_();
          return b;

        #endif
        } while(b<10);
        DCEMarker1_();
        return b;
    }
    )code";

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
    void DCEMarker6_(void);
    void DCEMarker7_(void);
    int foo(int a){
        DCEMarker0_();
        switch(a){
        case 1: 
            DCEMarker2_();
            a = 2;
            break;
        case 2:
          DCEMarker3_();
        case 3:
           DCEMarker7_();
           break;
        case 4:
          DCEMarker4_();
          return 3;
        case 5:
          DCEMarker5_();
          {a = 5;}
        default:
          DCEMarker6_();
          a = 42;
        }
        DCEMarker1_();
        return a;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter do while and if with return",
          "[if][dowhile][return]") {
    auto Code = R"code(int foo(int a) {
        do
            if (a + 1 == 2)
                return 1;
        while (++a);
        return 0;
    })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
        void DCEMarker1_(void);
        void DCEMarker2_(void);
        void DCEMarker3_(void);
        void DCEMarker4_(void);
        int foo(int a) {
          DCEMarker0_();
          do {
          #ifndef DeleteDCEMarkerBlock2_

            DCEMarker2_();
            if (a + 1 == 2) {
            #ifndef DeleteDCEMarkerBlock4_

              DCEMarker4_();
              return 1;
            #endif
            }
            DCEMarker3_();

          #endif
          } while (++a);
          DCEMarker1_();
          return 0;
    })code";
    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter do while and if else with return",
          "[if][dowhile][return]") {
    auto Code = R"code(int foo(int a) {
    if (a)
    do 
    --a;
    while(a);
    else
    return 1;
    return 0;
    })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
                void DCEMarker1_(void);
                void DCEMarker2_(void);
                void DCEMarker3_(void);
                void DCEMarker4_(void);
                int foo(int a) {
                  DCEMarker0_();
                  if (a) {
                  #ifndef DeleteDCEMarkerBlock2_
                 
                    DCEMarker2_();
                    do {
                    #ifndef DeleteDCEMarkerBlock4_

                      DCEMarker4_();
                      --a;

                    #endif
                    } while (a);
                  #endif
                  } else {
                  #ifndef DeleteDCEMarkerBlock3_

                    DCEMarker3_();
                    return 1;
                  #endif
                  }
                  DCEMarker1_();
                  return 0;
                })code";
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
                        void DCEMarker3_(void);
                        #define TEST bar

                        int bar();

                        void baz(int a) {
                            DCEMarker0_();
                            switch (a) {
                            case 1:
                                DCEMarker1_();
                                TEST();
                            }
                        }

                        void foo(int a) {
                            DCEMarker2_();
                            if (a){
                            #ifndef DeleteDCEMarkerBlock3_

                            DCEMarker3_();
                                a = 1;
                            #endif
                            }
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
                        void DCEMarker5_(void);
                        void DCEMarker6_(void);
                        void DCEMarker7_(void);
#define FFFF 1
    int foo() {
        DCEMarker0_();
        if (1) {
        #ifndef DeleteDCEMarkerBlock2_

          DCEMarker2_();
          switch (1) {
            default:
              DCEMarker7_();
              return FFFF;
            }
          DCEMarker4_();
        #endif
        } else {
        #ifndef DeleteDCEMarkerBlock3_

          DCEMarker3_();
          if (1) {
        #ifndef DeleteDCEMarkerBlock6_

            DCEMarker6_();
            return FFFF;
        #endif
          }
          DCEMarker5_();
        #endif
        }
        DCEMarker1_();
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if else with return and macro body",
          "[if][return][macro]") {

    auto Code = R"code(#define R1 return 1;
    int foo(int a) {
        if (a)
          if(a+1)
            R1
        else
            a = 2;
        return a;
    })code";
    auto ExpectedCode = R"code(void DCEMarker0_(void);
                        void DCEMarker1_(void);
                        void DCEMarker2_(void);
                        void DCEMarker3_(void);
                        void DCEMarker4_(void);
                        void DCEMarker5_(void);
                #define R1 return 1;
                int foo(int a) {
                    DCEMarker0_();
                    if (a){
        #ifndef DeleteDCEMarkerBlock2_

                        DCEMarker2_();
                        if (a+1){
        #ifndef DeleteDCEMarkerBlock4_

                            DCEMarker4_();
                            R1
        #endif
                        } else {
        #ifndef DeleteDCEMarkerBlock5_

                        DCEMarker5_();
                        a = 2;
        #endif
                        }
                        DCEMarker3_();
        #endif
                    }
                    DCEMarker1_();
                    return a;
                 })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter do while if and macro return",
          "[loop][if][return][macro]") {

    auto Code = R"code(#define X 1

    int foo() {
        do
            if (1)
                return X;
        while (1);
        return X;
    })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    void DCEMarker3_(void);
    void DCEMarker4_(void);
    #define X 1

    int foo() {
        DCEMarker0_();
        do{
        #ifndef DeleteDCEMarkerBlock2_

            DCEMarker2_();
            if (1) {
        #ifndef DeleteDCEMarkerBlock4_

                DCEMarker4_();
                return X;
        #endif
            }
            DCEMarker3_();

        #endif
        } while (1);
        DCEMarker1_();
        return X;
    })code";

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
    void DCEMarker2_(void);
    #define X 0
    int foo() {
    DCEMarker0_();
    if (1) {
        #ifndef DeleteDCEMarkerBlock2_

        DCEMarker2_();
        return X /* comment */;
        #endif
    }
    DCEMarker1_();
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if return macro", "[if][macro][return]") {

    auto Code = R"code(#define BUG

void foo() {
    if (1)
        return BUG;
})code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    #define BUG

    void foo() {
        DCEMarker0_();
        if (1) {
        #ifndef DeleteDCEMarkerBlock2_

            DCEMarker2_();
            return BUG;
        #endif
        }
        DCEMarker1_();
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if return with semi in macro",
          "[if][macro][return]") {

    auto Code = R"code(#define BUG ;

void foo() {
    if (1)
        return BUG
})code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    #define BUG ;

    void foo() {
        DCEMarker0_();
        if (1) {
        #ifndef DeleteDCEMarkerBlock2_

            DCEMarker2_();
            return BUG
        #endif
        }
        DCEMarker1_();
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if-else return with semi in macro",
          "[if][macro][return]") {

    auto Code = R"code(#define BUG ;

void foo() {
    if (1)
        return BUG
    else
        return;
        
})code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    void DCEMarker3_(void);
    #define BUG ;

    void foo() {
        DCEMarker0_();
        if (1) {
       #ifndef DeleteDCEMarkerBlock2_

            DCEMarker2_();
            return BUG
        #endif
        } else {
       #ifndef DeleteDCEMarkerBlock3_

            DCEMarker3_();
            return;
        #endif
        }
        DCEMarker1_();
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if dowhile with nested macro",
          "[if][do][macro][return]") {

    auto Code = R"code(#define M
    #define bar    \
    do {           \
    } while (0) M

    void foo() {
       if (1)
        bar;
    })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    #define M
    #define bar    \
    do  {          \
    } while (0) M

    void foo() {
       DCEMarker0_();
       if (1){
       #ifndef DeleteDCEMarkerBlock1_

        DCEMarker1_();
        bar;
       #endif
       }
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

    void foo() {
        DCEMarker0_();
        while (1) {
      #ifndef DeleteDCEMarkerBlock1_

        DCEMarker1_();
      #endif
        }
        if (1) {
      #ifndef DeleteDCEMarkerBlock2_

        DCEMarker2_();
      #endif
        }
        do {
      #ifndef DeleteDCEMarkerBlock3_

        DCEMarker3_();
      #endif
        } while(1);
        if (1){
      #ifndef DeleteDCEMarkerBlock4_

        DCEMarker4_();
        ;
      #endif
        }
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}
