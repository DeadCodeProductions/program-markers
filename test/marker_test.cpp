#include "test_tool.hpp"
#include <catch2/catch.hpp>

TEST_CASE("BranchInstrumenter if without else", "[if]") {
    auto Code = std::string{R"code(int foo(int a){
        if (a > 0))code"};
    Code += GENERATE(
        R"code(
                return 1;)code",
        R"code(

        {

        return 1; 

        }

        )code");
    Code += R"code(       
     return 0;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    int foo(int a){
        if ( a > 0)

        {

            DCEMarker1_();

            return 1;

        }

        else {
            DCEMarker0_();
        }

        return 0;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
}

TEST_CASE("BranchInstrumenter if-else", "[if]") {
    auto Code = std::string{R"code(int foo(int a){
        if (a > 0))code"};

    Code += GENERATE(
        R"code(

        {
        a = 1;

        }

        )code",
        R"code(
        a = 1;
        )code");

    Code += GENERATE(
        R"code(

        else

        {
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
        if ( a > 0)

        {

            DCEMarker1_();

            a = 1;

        } 

        else 

        {

            DCEMarker0_();

            a = 0;
        }

        return a;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
}

TEST_CASE("BranchInstrumenter if with return macro", "[if][macro]") {
    auto Code = std::string{R"code(#define R return

    int foo(int a){
        if (a > 0))code"};

    Code += GENERATE(R"code(
      R 0;
    )code",
                     R"code(

                     {
      R 0;

    }
    )code");
    Code +=
        R"code(       return a;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    #define R return

    int foo(int a){
        if ( a > 0)

        {

            DCEMarker1_();

            R 0;

        }

        else {
            DCEMarker0_();
        }

        return a;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
}

TEST_CASE("BranchInstrumenter if with return macro 2", "[if][macro]") {
    auto Code = std::string{R"code(#define R return 0

    int foo(int a){
        if (a > 0))code"};

    Code += GENERATE(R"code(
      R;
    )code",
                     R"code(

                     {
      R;

    }

    )code");
    Code +=
        R"code(return a;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    #define R return 0

    int foo(int a){
        if ( a > 0)

        {

            DCEMarker1_();

            R;

        }


        else {
            DCEMarker0_();
        }

        return a;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
}

TEST_CASE("BranchInstrumenter if with return macro 3", "[if][macro]") {
    auto Code = std::string{R"code(#define R return 0;

    int foo(int a){
        if (a > 0))code"};

    Code += GENERATE(R"code(
      R
    )code",
                     R"code(

                     {
      R

    }
    )code");
    Code +=
        R"code(
        return a;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    #define R return 0;

    int foo(int a){
        if ( a > 0)

        {

            DCEMarker1_();

            R

        }


        else {
            DCEMarker0_();
        }

        return a;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
}

TEST_CASE("BranchInstrumenter nested if with macro", "[if][nested][macro]") {
    auto Code = std::string{R"code(#define A a = 1;
    int foo(int a){
        if (a > 0)
        )code"};

    bool compoundThen = GENERATE(true, false);
    if (compoundThen)
        Code += R"code(

        { )code";

    Code += R"code(if (a==1) )code";

    Code += GENERATE(R"code(A)code", R"code(

    {A

    })code");
    Code += R"code(

        else

        )code";
    Code += GENERATE(R"code(a = 2;)code", R"code({a = 2;}


    )code");
    if (compoundThen)
        Code += R"code(


        } )code";
    Code += R"code(   
        return 0;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    void DCEMarker3_(void);
    #define A a = 1;
    int foo(int a){
        if ( a > 0)

        {

            DCEMarker1_();

                if ( a==1) 

                      {

                          DCEMarker3_();

                          A

                      }

                else 

                {

                    DCEMarker2_();

                    a = 2;
                }

        }

        else {
            DCEMarker0_();
        }

        return 0;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 formatCode(runBranchInstrumenterOnCode(Code, false)));
}

TEST_CASE("BranchInstrumenter nested if with return", "[if][return][nested]") {
    auto Code = std::string{R"code(int foo(int a){
        if (a >= 0)

        )code"};

    bool compoundThen = GENERATE(true, false);
    if (compoundThen)
        Code += R"code({ )code";

    Code += R"code(if (a>=1) 

    )code";

    Code += GENERATE(R"code(return 1;)code", R"code({return 1;

    })code");
    if (compoundThen)
        Code += R"code(
        } )code";
    Code += R"code(   
        return 0;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    void DCEMarker3_(void);
    int foo(int a){
        if ( a >= 0)

        {

            DCEMarker1_();

              if ( a >= 1)

              {

                DCEMarker3_();

                return 1;
                
              }

                else {
                    DCEMarker2_();
                }

                }


        else {
            DCEMarker0_();
        }

        return 0;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
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
        if (1)

    {

        DCEMarker1_();

        return X /* comment */;

    }

        else {
            DCEMarker0_();
        }

    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
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
        if ( 1)

    {

        DCEMarker1_();

        return BUG;

    }

        else {
            DCEMarker0_();
        }
    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
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
        if ( 1)

    {

        DCEMarker1_();

        return BUG

    }

        else {
            DCEMarker0_();
        }
    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
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
        if ( 1)

    {

        DCEMarker1_();

        return BUG

    }

        else 

        {

            DCEMarker0_();

            return;
        }

    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
}

TEST_CASE("BranchInstrumenter if-else nested with while", "[if][loop][while]") {
    std::string Code = R"code(int foo(int a){
      if (a > 0))code";

    bool compoundThen = GENERATE(true, false);
    if (compoundThen)
        Code += R"code(

        {)code";
    Code += R"code(
         while(a--))code";

    Code += GENERATE(
        R"code( 

        {
    return 1;
    }
    )code",
        R"code(
    return 1;
    )code");

    if (compoundThen)
        Code += R"code(

        }

        )code";

    Code += GENERATE(R"code( else
                    a = 0;
                )code",
                     R"code( else 

                     {
                    a = 0;
                }
                )code");

    Code += R"code(
    return a;
                    })code";
    //Code = formatCode(Code);

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    int foo(int a) {
        if ( a > 0)

        {

            DCEMarker1_();

            while( a--)

            {

                DCEMarker2_();

                return 1;
            }

        } 

        else

        {
        
            DCEMarker0_();

            a = 0;
        }

        return a;
    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
}

TEST_CASE("BranchInstrumenter while stmt", "[while][loop]") {

    auto Code = std::string{R"code(int foo(int a){
        int b = 0;
        while(true))code"};

    Code += GENERATE(
        R"code(
    return 0;
    )code",
        R"code( 

        {
    return 0;
    }


    )code");
    Code += R"code(return b;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    int foo(int a){
        int b = 0;
            while(true)

            {

                DCEMarker0_();

                return 0;

            }

        return b;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
}
TEST_CASE("BranchInstrumenter nested for stmt", "[for][if][nested][return]") {

    auto Code = std::string{R"code(int foo(int a){
        for (;;))code"};

    bool compoundFor = GENERATE(true, false);
    if (compoundFor)
        Code += R"code(

        {)code";

    Code += R"code(
    for(;;)
    )code";
    Code += GENERATE(R"code(++a;)code", R"code(

    {++a;})code");
    if (compoundFor)
        Code += R"code(}

        )code";
    Code += R"code(
    }

    )code";
    Code = formatCode(Code);

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    int foo(int a){
        for ( ;;)

        {

            DCEMarker0_();

        for ( ;;)

            {

                DCEMarker1_();

                ++a;

            }

        }
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 formatCode(runBranchInstrumenterOnCode(Code, false)));
}

TEST_CASE("BranchInstrumenter for stmt nested if with return",
          "[for][if][nested][return]") {

    auto Code = std::string{R"code(int foo(int a){
        int b = 0;
        for (int i = 0; i < a; ++i))code"};

    bool compoundFor = GENERATE(true, false);
    if (compoundFor)
        Code += R"code(

        {)code";
    Code += R"code(
            if (i == 3)
            )code";
    Code += GENERATE(R"code(return b;)code", R"code(

    {return b;

    })code");
    Code += R"code(

            else
            )code";
    Code += GENERATE(R"code(++b;)code", R"code(

    {++b;

    })code");

    if (compoundFor)
        Code += R"code(

        }

        )code";
    Code += R"code(
        return b;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    void DCEMarker2_(void);
    int foo(int a){
        int b = 0;
        for ( int i = 0; i < a; ++i)

        {

            DCEMarker0_();


        if ( i == 3)

            {

                DCEMarker2_();

                return b;

            }


             else 

             {

                DCEMarker1_();

                ++b;
            }

        }

        return b;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
}

TEST_CASE("BranchInstrumenter for stmt nested if with return and extra stmt",
          "[for][if][nested][return]") {

    auto Code = std::string{R"code(int foo(int a){
        int b = 0;
        for (int i = 0; i < a; ++i){)code"};
    Code += R"code(
            if (i == 3)
            )code";
    Code += GENERATE(R"code(return b;)code", R"code(

    {

    return b;

    })code");
    Code += R"code(

            else
            )code";
    Code += GENERATE(R"code(++b;)code", R"code(

    {++b;

    }

    )code");

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
        for ( int i = 0; i < a; ++i) {

            DCEMarker0_();

        if ( i == 3)

            {

                DCEMarker2_();

                return b;

            }

             else 

             {

                DCEMarker1_();

                ++b;
            }

        ++b;
        }
        return b;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
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
        for ( int i = 0; i < a; ++i)

        {

            DCEMarker0_();

            return i;

        }

        return b;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
}

TEST_CASE("BranchInstrumenter do while stmt with return", "[do][return]") {

    auto Code = std::string{R"code(int foo(int a){
        int b = 0;
        do 
        )code"};
    Code += GENERATE(R"code(return b;)code", R"code(
    {

    return b;

    }

    )code");
    Code += R"code(while(b<10);
        return b;
    }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    int foo(int a){
        int b = 0;
        do 

        {
        
          DCEMarker0_();

          return b;

        } 

        while(b<10);
        return b;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
}

TEST_CASE("BranchInstrumenter do while and if with return",
          "[if][dowhile][return]") {
    auto Code = std::string{R"code(#define X 1
    int foo(int a) {
        do )code"};

    bool compoundDo = GENERATE(true, false);
    if (compoundDo)
        Code += R"code(

        {)code";
    Code += R"code(
            if (a + 1 == 2)
             )code";
    Code += GENERATE(R"code(return X;)code", R"code(

    {

    return X;

    }

    )code");
    if (compoundDo)
        Code += R"code(
        }

        )code";
    Code += R"code(
         while (++a);
        return 0;
    })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
        void DCEMarker1_(void);
        void DCEMarker2_(void);
        #define X 1
        int foo(int a) {
          do 

          {

            DCEMarker0_();

            if ( a + 1 == 2)

            {

            DCEMarker2_();

            return X;

            }

            else {
                DCEMarker1_();
            }

          } 

          while (++a);
          return 0;
    })code";
    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
}

TEST_CASE("BranchInstrumenter do while and if else with return",
          "[if][dowhile][return]") {
    auto Code = std::string{R"code(int foo(int a) {
                if (a))code"};

    bool compoundDo = GENERATE(true, false);
    if (compoundDo)
        Code += R"code(

        {

        )code";

    Code += R"code(
                do )code";
    Code += GENERATE(R"code(--a;)code", R"code(

    {

    --a;

    })code");
    Code += R"code(

        while(a);
        )code";

    if (compoundDo)
        Code += R"code(

        }

        )code";

    Code += R"code(else
        )code";

    Code += GENERATE(R"code(return 1;)code", R"code(

    {

    return 1;

    }

    )code");
    Code += R"code(       
    return 0;
    })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
        void DCEMarker1_(void);
        void DCEMarker2_(void);
        int foo(int a) {
            if ( a)

            {

                DCEMarker1_();

              do 

              {

                DCEMarker2_();

                --a;

              } 

              while (a);

            }

            else 

            {

                DCEMarker0_();

                return 1;
            }

          return 0;
    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
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
    Code += GENERATE(R"code(bar;)code", R"code(
    {

    bar;

    })code");
    Code += R"code(   })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
        void DCEMarker1_(void);
        #define M
        #define bar    \
        do  {          \
        } while (0) M

        void foo() {
            if ( 1)

            {

                DCEMarker1_();

              bar; 

            }

            else
            {
                DCEMarker0_();
            }
            
    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
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
        while ( 1) {

        DCEMarker0_();
        }
  if ( 1) {

      DCEMarker2_();

    }

  else {
      DCEMarker1_();
    }

        do {

        DCEMarker3_();

        } while(1);
  if ( 1)

  {

      DCEMarker5_();

      ;

    }

  else {
      DCEMarker4_();
    }

    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
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
        case 1: 

            DCEMarker0_();

            a = 2;
            break;
        case 2:

          DCEMarker5_();

        case 3:

           DCEMarker4_();

           break;
        case 4:

          DCEMarker3_();

          return 3;
        case 5:

          DCEMarker2_();

          {a = 5;}
        default:

          DCEMarker1_();

          a = 42;

        }
        return a;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
}

TEST_CASE("BranchInstrumenter cascaded switch", "[switch]") {

    auto Code = R"code(int foo(int a){
            switch (a) {
            case 0:
                a=1;
                break;
            default:
            case 1:
            case 2:
                a=2;
                break;
            case 3:
                break;
            }
        }
    )code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
                  void DCEMarker1_(void);
                  void DCEMarker2_(void);
                  void DCEMarker3_(void);
                  void DCEMarker4_(void);
                  int foo(int a) {
                    switch (a) {
                  case 0:

                    DCEMarker0_();

                    a = 1;
                      break;
                  default:

                    DCEMarker2_();

                  case 1:

                    DCEMarker4_();

                  case 2:

                    DCEMarker3_();

                    a = 2;
                      break;
                  case 3:

                    DCEMarker1_();

                    break;
                    }
                  }
)code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
}

TEST_CASE("BranchInstrumenter empty switch", "[switch]") {
    auto Code = R"code(int foo(int a){
        switch(a){
        }
        return a;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(Code), runBranchInstrumenterOnCode(Code, false));
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
                            case 1:

                                DCEMarker0_();

                                TEST();
                            }
                        }

                        void foo(int a) {
                              if ( a)

                            {

                                DCEMarker2_();

                                a = 1;

                                }

                              else {
                                  DCEMarker1_();
                              }

                        })code";
    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
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
              if ( 1)

        {

          DCEMarker1_();

          switch (1) {
            default:

              DCEMarker2_();

              return FFFF;
            }

        } 

        else 

        {

          DCEMarker0_();

            if ( 1)

            {

            DCEMarker4_();

            return FFFF;

            }

            else {
                DCEMarker3_();
              }

        }

    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, false));
}

