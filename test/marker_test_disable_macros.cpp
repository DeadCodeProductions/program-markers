#include "test_tool.hpp"
#include <catch2/catch.hpp>

TEST_CASE("BranchInstrumenter if without else (with disable macros)", "[if]") {
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

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    int foo(int a){

        #if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        if (

        #endif

        a > 0

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

        else {
            DCEMarker0_();
        }
        #endif

        #endif

        return 0;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter if-else (with disable macros)", "[if]") {
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

        #if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        if (

        #endif

            a > 0

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        )

        #else

            ;
        #endif

        #ifndef DeleteBlockDCEMarker1_

        {

            DCEMarker1_();

            a = 1;

        } 

        #endif

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        else

        #endif

        #ifndef DeleteBlockDCEMarker0_

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
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter if with return macro (with disable macros)",
          "[if][macro]") {
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

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    #define R return

    int foo(int a){

        #if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        if (

        #endif

        a > 0

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

            )

        #else

            ;
        #endif

        #ifndef DeleteBlockDCEMarker1_

        {

            DCEMarker1_();

            R 0;

        }

        #endif

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        else {
            DCEMarker0_();
        }
        #endif

        #endif

        return a;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter if with return macro 2 (with disable macros)",
          "[if][macro]") {
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

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    #define R return 0

    int foo(int a){

        #if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        if (

        #endif

        a > 0

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

            )

        #else

            ;
        #endif

        #ifndef DeleteBlockDCEMarker1_

        {

            DCEMarker1_();

            R;

        }

        #endif

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        else {
            DCEMarker0_();
        }
        #endif

        #endif

        return a;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter if with return macro 3 (with disable macros)",
          "[if][macro]") {
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

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    #define R return 0;

    int foo(int a){

        #if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        if (

        #endif

        a > 0

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

            )

        #else

            ;
        #endif

        #ifndef DeleteBlockDCEMarker1_

        {

            DCEMarker1_();

            R

        }

        #endif

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        else {
            DCEMarker0_();
        }
        #endif

        #endif

        return a;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter nested if with macro (with disable macros)",
          "[if][nested][macro]") {
    auto Code = std::string{R"code(#define A a = 1;
    int foo(int a){
        if (a > 0)
        )code"};

    bool compoundThen = GENERATE(true, false);
    if (compoundThen)
        Code += R"code({ )code";

    Code += R"code(if (a==1) )code";

    Code += GENERATE(R"code(A)code", R"code({A

    })code");
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

        #if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        if (

            #endif

        a > 0

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        )

        #else

            ;
        #endif

    #ifndef DeleteBlockDCEMarker1_

        {

            DCEMarker1_();

            #if !defined(DeleteBlockDCEMarker2_) || !defined(DeleteBlockDCEMarker3_)

            #if !defined(DeleteBlockDCEMarker2_) && !defined(DeleteBlockDCEMarker3_)

                if (

            #endif

                    a==1

            #if !defined(DeleteBlockDCEMarker2_) && !defined(DeleteBlockDCEMarker3_)

                ) 

        #else

            ;
            #endif

            #ifndef DeleteBlockDCEMarker3_

                      {

                          DCEMarker3_();

                          A

                      }

            #endif
            
            #if !defined(DeleteBlockDCEMarker2_) && !defined(DeleteBlockDCEMarker3_)

                else 

            #endif

            #ifndef DeleteBlockDCEMarker2_

                {

                    DCEMarker2_();

                    a = 2;
                }

            #endif

        #endif

        }

    #endif

    #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        else {
            DCEMarker0_();
        }
        #endif

        #endif

        return 0;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(formatCode(ExpectedCode)),
                 formatCode(runBranchInstrumenterOnCode(Code, true)));
}

TEST_CASE("BranchInstrumenter nested if with return (with disable macros)",
          "[if][return][nested]") {
    auto Code = std::string{R"code(int foo(int a){
        if (a >= 0)
        )code"};

    bool compoundThen = GENERATE(true, false);
    if (compoundThen)
        Code += R"code({ )code";

    Code += R"code(if (a>=0) 
    )code";

    Code += GENERATE(R"code(return 1;)code", R"code({return 1;

    })code");
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

      #if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)

      #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        if (

            #endif

        a >= 0

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        )

        #else

            ;
        #endif

        #ifndef DeleteBlockDCEMarker1_

        {

            DCEMarker1_();

            #if !defined(DeleteBlockDCEMarker2_) || !defined(DeleteBlockDCEMarker3_)

            #if !defined(DeleteBlockDCEMarker2_) && !defined(DeleteBlockDCEMarker3_)

              if (

                  #endif

              a >= 0

              #if !defined(DeleteBlockDCEMarker2_) && !defined(DeleteBlockDCEMarker3_)

              )

            #else

            ;
              #endif

              #ifndef DeleteBlockDCEMarker3_

              {

                DCEMarker3_();

                return 1;
                
              }

              #endif

            #if !defined(DeleteBlockDCEMarker2_) && !defined(DeleteBlockDCEMarker3_)

                else {
                    DCEMarker2_();
                }
            #endif

            #endif

                }

            #endif

    #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        else {
            DCEMarker0_();
        }
    #endif

    #endif

        return 0;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE(
    "BranchInstrumenter if return macro and comment (with disable macros)",
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

     #if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)

      #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        if (

            #endif

        1

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        )

        #else

            ;
        #endif

        #ifndef DeleteBlockDCEMarker1_

    {

        DCEMarker1_();

        return X /* comment */;

    }

        #endif

    #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        else {
            DCEMarker0_();
        }
    #endif

    #endif
    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter if return macro (with disable macros)",
          "[loop][if][macro][return]") {

    auto Code = R"code(#define BUG
    void foo() {
    if (1)
        return BUG;
    })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
    void DCEMarker1_(void);
    #define BUG
    void foo() {

     #if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)

      #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        if (

            #endif

        1

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        )

        #else

            ;
        #endif

        #ifndef DeleteBlockDCEMarker1_

    {

        DCEMarker1_();

        return BUG;

    }

        #endif

    #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        else {
            DCEMarker0_();
        }
    #endif

    #endif
    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter if with semi return macro (with disable macros)",
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

     #if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)

      #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        if (

            #endif

        1

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        )

        #else

            ;
        #endif

        #ifndef DeleteBlockDCEMarker1_

    {

        DCEMarker1_();

        return BUG

    }

        #endif

    #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        else {
            DCEMarker0_();
        }
    #endif

    #endif
    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE(
    "BranchInstrumenter if-else with semi return macro (with disable macros)",
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

     #if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)

      #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        if (

            #endif

        1

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        )

        #else

            ;
        #endif

        #ifndef DeleteBlockDCEMarker1_

    {

        DCEMarker1_();

        return BUG

    }

        #endif

    #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        else 

    #endif

    #ifndef DeleteBlockDCEMarker0_

        {

            DCEMarker0_();

            return;
        }

    #endif

    #endif
    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter if-else nested with while (with disable macros)",
          "[if][loop][while]") {
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

        #if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        if (

        #endif

            a > 0

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        )

        #else

            ;
        #endif

        #ifndef DeleteBlockDCEMarker1_

        {

            DCEMarker1_();

        #ifndef DeleteBlockDCEMarker2_

            while(

        #endif

            a--

        #ifndef DeleteBlockDCEMarker2_

            )

        #else
        ;
        #endif

        #ifndef DeleteBlockDCEMarker2_

            {

                DCEMarker2_();

                return 1;
            }

        #endif

        } 

        #endif

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        else

        #endif
        
        #ifndef DeleteBlockDCEMarker0_

        {
        
            DCEMarker0_();

            a = 0;
        }

        #endif

        #endif

        return a;
    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter while stmt (with disable macros)",
          "[while][loop]") {

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

        #ifndef DeleteBlockDCEMarker0_

            while(

        #endif

            true

        #ifndef DeleteBlockDCEMarker0_

            )

        #else
        ;
        #endif

        #ifndef DeleteBlockDCEMarker0_

            {

                DCEMarker0_();

                return 0;

            }

        #endif

        return b;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter nested for stmt (with disable macros)",
          "[for][if][nested][return]") {

    auto Code = std::string{R"code(int foo(int a){
        for (;;))code"};

    bool compoundFor = GENERATE(true, false);
    if (compoundFor)
        Code += R"code({)code";

    Code += R"code(
    for(;;)
    )code";
    Code += GENERATE(R"code(++a;)code", R"code({++a;})code");
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

        #ifndef DeleteBlockDCEMarker0_

        for (

        #else
        {
        #endif

        ;;

        #ifndef DeleteBlockDCEMarker0_

        )
        
        #endif

        #ifndef DeleteBlockDCEMarker0_

        {

            DCEMarker0_();

        #ifndef DeleteBlockDCEMarker1_

        for (

        #else
        {
        #endif

        ;;

        #ifndef DeleteBlockDCEMarker1_

        )

        #endif

        #ifndef DeleteBlockDCEMarker1_

            {

                DCEMarker1_();

                ++a;

            #endif
            }

        #endif
        }
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 formatCode(runBranchInstrumenterOnCode(Code, true)));
}

TEST_CASE(
    "BranchInstrumenter for stmt nested if with return (with disable macros)",
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
    Code += GENERATE(R"code(return b;)code", R"code({return b;

    })code");
    Code += R"code(
            else
            )code";
    Code += GENERATE(R"code(++b;)code", R"code({++b;})code");

    if (compoundFor)
        Code += R"code(}

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

        #ifndef DeleteBlockDCEMarker0_

        for (

        #else
        {
        #endif

        int i = 0; i < a;

        #ifndef DeleteBlockDCEMarker0_

        ++i)

        #endif

        #ifndef DeleteBlockDCEMarker0_

        {

            DCEMarker0_();

        #if !defined(DeleteBlockDCEMarker1_) || !defined(DeleteBlockDCEMarker2_)

        #if !defined(DeleteBlockDCEMarker1_) && !defined(DeleteBlockDCEMarker2_)

        if (

        #endif

            i == 3

        #if !defined(DeleteBlockDCEMarker1_) && !defined(DeleteBlockDCEMarker2_)

        )

        #else

            ;
        #endif

        #ifndef DeleteBlockDCEMarker2_

            {

                DCEMarker2_();

                return b;

            }

            #endif

        #if !defined(DeleteBlockDCEMarker1_) && !defined(DeleteBlockDCEMarker2_)

             else 

        #endif

        #ifndef DeleteBlockDCEMarker1_

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
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter for stmt nested if with return and extra stmt "
          "(with disable macros)",
          "[for][if][nested][return]") {

    auto Code = std::string{R"code(int foo(int a){
        int b = 0;
        for (int i = 0; i < a; ++i){)code"};
    Code += R"code(
            if (i == 3)
            )code";
    Code += GENERATE(R"code(return b;)code", R"code({return b;

    })code");
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

        #ifndef DeleteBlockDCEMarker0_

        for (

        #else
        {
        #endif

        int i = 0; i < a;

        #ifndef DeleteBlockDCEMarker0_

        ++i)

        #endif

        #ifndef DeleteBlockDCEMarker0_

        {

            DCEMarker0_();

        #if !defined(DeleteBlockDCEMarker1_) || !defined(DeleteBlockDCEMarker2_)

        #if !defined(DeleteBlockDCEMarker1_) && !defined(DeleteBlockDCEMarker2_)

        if (

        #endif

            i == 3

        #if !defined(DeleteBlockDCEMarker1_) && !defined(DeleteBlockDCEMarker2_)

        )

        #else

            ;
        #endif

        #ifndef DeleteBlockDCEMarker2_

            {

                DCEMarker2_();

                return b;

            }

            #endif

        #if !defined(DeleteBlockDCEMarker1_) && !defined(DeleteBlockDCEMarker2_)

             else 

        #endif

        #ifndef DeleteBlockDCEMarker1_

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
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter for stmt with return (with disable macros)",
          "[for][return]") {

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

        #ifndef DeleteBlockDCEMarker0_

        for (

        #else
        {
        #endif

        int i = 0; i < a; 

        #ifndef DeleteBlockDCEMarker0_

        ++i)

        #endif

        #ifndef DeleteBlockDCEMarker0_

        {

            DCEMarker0_();

            return i;

        #endif
        }

        return b;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter do while stmt with return (with disable macros)",
          "[do][return]") {

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

        #ifndef DeleteBlockDCEMarker0_

        do 

        {
        
          DCEMarker0_();

          return b;

        } 

        while(b<10);

        #endif

        return b;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE(
    "BranchInstrumenter do while and if with return (with disable macros)",
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
    Code += GENERATE(R"code(return X;)code", R"code({return X;

    })code");
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

          #ifndef DeleteBlockDCEMarker0_

          do 

          {

            DCEMarker0_();

            #if !defined(DeleteBlockDCEMarker1_) || !defined(DeleteBlockDCEMarker2_)

            #if !defined(DeleteBlockDCEMarker1_) && !defined(DeleteBlockDCEMarker2_)

            if (

                #endif

            a + 1 == 2

            #if !defined(DeleteBlockDCEMarker1_) && !defined(DeleteBlockDCEMarker2_)

            )

            #else

            ;
            #endif

            #ifndef DeleteBlockDCEMarker2_

            {

            DCEMarker2_();

            return X;

            }

            #endif

            #if !defined(DeleteBlockDCEMarker1_) && !defined(DeleteBlockDCEMarker2_)

            else {
                DCEMarker1_();
            }
            #endif

            #endif

          } 

          while (++a);

          #endif

          return 0;
    })code";
    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE(
    "BranchInstrumenter do while and if else with return (with disable macros)",
    "[if][dowhile][return]") {
    auto Code = std::string{R"code(int foo(int a) {
                if (a))code"};

    bool compoundDo = GENERATE(true, false);
    if (compoundDo)
        Code += R"code({)code";

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

            #if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)

            #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

            if (

                #endif

            a

            #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

            )

            #else

            ;
            #endif

            #ifndef DeleteBlockDCEMarker1_

            {

                DCEMarker1_();

              #ifndef DeleteBlockDCEMarker2_

              do 

              {

                DCEMarker2_();

                --a;

              } 

              while (a);

              #endif

            }

            #endif

            #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

            else 

            #endif

            #ifndef DeleteBlockDCEMarker0_

            {

                DCEMarker0_();

                return 1;
            }

            #endif

            #endif

          return 0;
    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE(
    "BranchInstrumenter if dowhile with nested macro (with disable macros)",
    "[if][do][macro][return]") {

    auto Code = std::string{R"code(#define M
    #define bar    \
    do {           \
    } while (0) M

    void foo() {
       if (1)
       )code"};
    Code += GENERATE(R"code(bar;)code", R"code({bar;

    })code");
    Code += R"code(   })code";

    auto ExpectedCode = R"code(void DCEMarker0_(void);
        void DCEMarker1_(void);
        #define M
        #define bar    \
        do  {          \
        } while (0) M

        void foo() {

            #if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)

            #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

            if (

                #endif

            1

            #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

            )

            #else

            ;
            #endif

            #ifndef DeleteBlockDCEMarker1_

            {

                DCEMarker1_();

              bar; 

            }

            #endif

            #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

            else
            {
                DCEMarker0_();
            }
            #endif

            #endif
    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter if while do and braces without whitespace (with "
          "disable macros)",
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

      #ifndef DeleteBlockDCEMarker0_

        while (

      #endif

        1

      #ifndef DeleteBlockDCEMarker0_

        ) 

      #else
      ;
      #endif

      #ifndef DeleteBlockDCEMarker0_

        {

        DCEMarker0_();
        }

      #endif

#if !defined(DeleteBlockDCEMarker1_) || !defined(DeleteBlockDCEMarker2_)

#if !defined(DeleteBlockDCEMarker1_) && !defined(DeleteBlockDCEMarker2_)

  if (

  #endif

        1

  #if !defined(DeleteBlockDCEMarker1_) && !defined(DeleteBlockDCEMarker2_)

  )

        #else

            ;
  #endif

#ifndef DeleteBlockDCEMarker2_

  {

      DCEMarker2_();

    }

  #endif

#if !defined(DeleteBlockDCEMarker1_) && !defined(DeleteBlockDCEMarker2_)

  else {
      DCEMarker1_();
    }
  #endif

  #endif

  #ifndef DeleteBlockDCEMarker3_

        do {

        DCEMarker3_();

        } while(1);

      #endif

#if !defined(DeleteBlockDCEMarker4_) || !defined(DeleteBlockDCEMarker5_)

#if !defined(DeleteBlockDCEMarker4_) && !defined(DeleteBlockDCEMarker5_)

  if (

  #endif

        1

  #if !defined(DeleteBlockDCEMarker4_) && !defined(DeleteBlockDCEMarker5_)

  )

        #else

            ;
  #endif

  #ifndef DeleteBlockDCEMarker5_

  {

      DCEMarker5_();

      ;

    }

  #endif

#if !defined(DeleteBlockDCEMarker4_) && !defined(DeleteBlockDCEMarker5_)

  else {
      DCEMarker4_();
    }
  #endif

  #endif
    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter switch (with disable macros)",
          "[switch][return]") {
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

#ifndef DeleteBlockDCEMarker0_

        case 1: 

            DCEMarker0_();

            a = 2;
            break;

#endif

#ifndef DeleteBlockDCEMarker5_

        case 2:

          DCEMarker5_();

#endif

#ifndef DeleteBlockDCEMarker4_

        case 3:

           DCEMarker4_();

           break;

#endif

#ifndef DeleteBlockDCEMarker3_

        case 4:

          DCEMarker3_();

          return 3;

#endif

#ifndef DeleteBlockDCEMarker2_

        case 5:

          DCEMarker2_();

          {a = 5;}

#endif

#ifndef DeleteBlockDCEMarker1_

        default:

          DCEMarker1_();

          a = 42;

#endif
        }
        return a;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter cascaded switch (with disable macros)",
          "[switch]") {

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

#ifndef DeleteBlockDCEMarker0_

                  case 0:

                    DCEMarker0_();

                    a = 1;
                      break;

#endif

#ifndef DeleteBlockDCEMarker2_

                  default:

                    DCEMarker2_();

#endif

#ifndef DeleteBlockDCEMarker4_

                  case 1:

                    DCEMarker4_();

#endif

#ifndef DeleteBlockDCEMarker3_

                  case 2:

                    DCEMarker3_();

                    a = 2;
                      break;

#endif

#ifndef DeleteBlockDCEMarker1_

                  case 3:

                    DCEMarker1_();

                    break;

#endif
                    }
                  }
)code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter empty switch (with disable macros)", "[switch]") {
    auto Code = R"code(int foo(int a){
        switch(a){
        }
        return a;
    }
    )code";

    CAPTURE(Code);
    compare_code(formatCode(Code), runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE("BranchInstrumenter switch if and macro (with disable macros)",
          "[if][switch][macro]") {
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

                            #ifndef DeleteBlockDCEMarker0_

                            case 1:

                                DCEMarker0_();

                                TEST();

                            #endif
                            }
                        }

                        void foo(int a) {

                            #if !defined(DeleteBlockDCEMarker1_) || !defined(DeleteBlockDCEMarker2_)

                            #if !defined(DeleteBlockDCEMarker1_) && !defined(DeleteBlockDCEMarker2_)

                              if (

                              #endif

                                    a

                              #if !defined(DeleteBlockDCEMarker1_) && !defined(DeleteBlockDCEMarker2_)

                              )

                                    #else

                                        ;
                              #endif

                            #ifndef DeleteBlockDCEMarker2_

                            {

                                DCEMarker2_();

                                a = 1;

                                }

                            #endif

                              #if !defined(DeleteBlockDCEMarker1_) && !defined(DeleteBlockDCEMarker2_)

                              else {
                                  DCEMarker1_();
                              }
                            #endif

                            #endif
                        })code";
    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

TEST_CASE(
    "BranchInstrumenter switch if with return and macro (with disable macros)",
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

            #if !defined(DeleteBlockDCEMarker0_) || !defined(DeleteBlockDCEMarker1_)
            
            #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)
            
              if (

              #endif

                    1

              #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)
            
              )

              #else
            
                  ;
              #endif

        #ifndef DeleteBlockDCEMarker1_

        {

          DCEMarker1_();

          switch (1) {

        #ifndef DeleteBlockDCEMarker2_
        
            default:

              DCEMarker2_();

              return FFFF;

              #endif
            }

        } 

        #endif

        #if !defined(DeleteBlockDCEMarker0_) && !defined(DeleteBlockDCEMarker1_)

        else 

        #endif
        
        #ifndef DeleteBlockDCEMarker0_

        {

          DCEMarker0_();

          #if !defined(DeleteBlockDCEMarker3_) || !defined(DeleteBlockDCEMarker4_)

              #if !defined(DeleteBlockDCEMarker3_) && !defined(DeleteBlockDCEMarker4_)

            if (

              #endif

                      1

              #if !defined(DeleteBlockDCEMarker3_) && !defined(DeleteBlockDCEMarker4_)

                )

              #else

                    ;
              #endif

        #ifndef DeleteBlockDCEMarker4_

            {

            DCEMarker4_();

            return FFFF;

            }

        #endif

        #if !defined(DeleteBlockDCEMarker3_) && !defined(DeleteBlockDCEMarker4_)

            else {
                DCEMarker3_();
              }
          #endif

        #endif
        }

        #endif
        
        #endif
    })code";

    CAPTURE(Code);
    compare_code(formatCode(ExpectedCode),
                 runBranchInstrumenterOnCode(Code, true));
}

