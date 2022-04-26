#include "test_tool.hpp"
#include <catch2/catch.hpp>

TEST_CASE("BranchInstrumenter if-else compound", "[if]") {
    auto Code = R"code(int foo(int a){
        if (a > 0){
        a = 1;
        } else{
        a = 0;
        }
        return a;
    }
    )code";
    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    int foo(int a){
        DCEMarker0_();
        if (a > 0){
        DCEMarker1_();
        a = 1;
        } else{
        DCEMarker2_();
        a = 0;
        }
        return a;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if-else nested with return", "[if]") {
    auto Code = R"code(int foo(int a){
        if (a > 0)
        while(a--)
            return 1;
         else
        a = 0;
        
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
        DCEMarker0_();
        if (a > 0){
            DCEMarker2_();
            while(a--){
                DCEMarker5_();
                return 1;
            }
            DCEMarker4_();
        } else{
            DCEMarker3_();
            a = 0;
        }
        DCEMarker1_();

        return a;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if-else nested with return in compound", "[if]") {
    auto Code = R"code(int foo(int a){
        if (a > 0)
        while(a--){
            return 1;
            }
         else
        a = 0;
        
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
        DCEMarker0_();
        if (a > 0){
            DCEMarker2_();
            while(a--){
                DCEMarker5_();
                return 1;
            }
            DCEMarker4_();
        } else{
            DCEMarker3_();
            a = 0;
        }
        DCEMarker1_();

        return a;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if with return macro", "[if]") {
    auto Code = R"code(#define R return

    int foo(int a){
        if (a > 0)
            R 0;
        return a;
    }
    )code";
    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    #define R return

    int foo(int a){
        DCEMarker0_();
        if (a > 0){
            DCEMarker2_();
            R 0;
        }
        DCEMarker1_();
        return a;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if-compound with return macro", "[if]") {
    auto Code = R"code(#define R return 0;

    int foo(int a){
        if (a > 0){
            R
        }
        return a;
    }
    )code";
    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    #define R return 0;

    int foo(int a){
        DCEMarker0_();
        if (a > 0){
        DCEMarker2_();
        R
        } 
        DCEMarker1_();
        return a;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if-else non-compound", "[if]") {
    auto Code = R"code(int foo(int a){
        if (a > 0)
            a=1;
        else
            a=0;
        return a;
    }
    )code";
    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    int foo(int a){
        DCEMarker0_();
        if (a > 0){
        DCEMarker1_();
            a=1;
        } else{
        DCEMarker2_();
            a=0;
        }
        return a;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter nested if", "[if][nested]") {
    auto Code = R"code(#define A a = 1;
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
    )code";
    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    void DCEMarker3_(void);
    #define A a = 1;
    int foo(int a){
        DCEMarker0_();
        if (a > 0){
            DCEMarker1_();
            if (a==1) {
                DCEMarker2_();
                A
            }
            else {
                DCEMarker3_();
                a = 2;
            }
        }
        return 0;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if with return", "[if][return]") {
    auto Code = R"code(int foo(int a){
        if (a > 0)
            return 1;
        return 0;
    }
    )code";
    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    int foo(int a){
        DCEMarker0_();
        if (a > 0){
            DCEMarker2_();
            return 1;
        }
        DCEMarker1_();
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
            DCEMarker2_();
            if (a >= 0) {
                DCEMarker4_();
                return 1;
            }
            DCEMarker3_();
        }
        DCEMarker1_();
        return 0;
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
            DCEMarker2_();
            if (i == 3){
                DCEMarker4_();
                return b;
            } else {
                DCEMarker5_();
                ++b;
            }
            DCEMarker3_();
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
            DCEMarker2_();
            if (i == 3){
                DCEMarker4_();
                return b;
            } else {
                DCEMarker5_();
                ++b;
            }
            DCEMarker3_();
            ++b;
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
            DCEMarker2_();
            return i;
        }
        DCEMarker1_();
        return b;
    }
    )code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter while stmt with return", "[while][return]") {

    auto Code = R"code(int foo(int a){
        int b = 0;
        while(true)
            return 0;
        return b;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    int foo(int a){
        DCEMarker0_();
        int b = 0;
        while(true) {
            DCEMarker2_();
            return 0;
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
          DCEMarker2_();
          return b;

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
            DCEMarker2_();
            if (a + 1 == 2) {
              DCEMarker4_();
              return 1;
            }
            DCEMarker3_();

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
                    DCEMarker2_();
                    do {
                      DCEMarker4_();
                      --a;

                    } while (a);
                  } else {
                    DCEMarker3_();
                    return 1;
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
                            DCEMarker3_();
                                a = 1;
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
          DCEMarker2_();
          switch (1) {
            default:
              DCEMarker7_();
              return FFFF;
            }
          DCEMarker4_();
        } else {
          DCEMarker3_();
          if (1) {
            DCEMarker6_();
            return FFFF;
          }
          DCEMarker5_();
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
                        DCEMarker2_();
                        if (a+1){
                            DCEMarker4_();
                            R1
                        } else {
                        DCEMarker5_();
                        a = 2;
                        }
                        DCEMarker3_();
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
            DCEMarker2_();
            if (1) {
                DCEMarker4_();
                return X;
            }
            DCEMarker3_();

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
        DCEMarker2_();
        return X /* comment */;
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
            DCEMarker2_();
            return BUG;
        }
        DCEMarker1_();
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if return with semi in macro", "[if][macro][return]") {

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
            DCEMarker2_();
            return BUG
        }
        DCEMarker1_();
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}

TEST_CASE("BranchInstrumenter if-else return with semi in macro", "[if][macro][return]") {

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
            DCEMarker2_();
            return BUG
        } else {
            DCEMarker3_();
            return;
        }
        DCEMarker1_();
    })code";

    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));
}


TEST_CASE("BranchInstrumenter if dowhile with nested macro", "[if][do][macro][return]") {


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
        DCEMarker1_();
        bar;
       }
    })code";


    CAPTURE(Code);
    REQUIRE(formatCode(ExpectedCode) == runBranchInstrumenterOnCode(Code));

}

