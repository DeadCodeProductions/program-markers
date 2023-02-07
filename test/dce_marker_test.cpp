#include <DCEInstrumenter.h>

#include "test_tool.h"
#include <catch2/catch.hpp>

TEST_CASE("DCEInstrumenter if without else", "[if]") {
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

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) + "// MARKERS END\n" +
      R"code(int foo(int a){
        if ( a > 0)

        {

            DCEMARKERMACRO1_

            return 1;

        }

        else {
            DCEMARKERMACRO0_
        }

        return 0;
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter if without else and semicolon after curly brace",
          "[if]") {
  auto Code = std::string{R"code(int foo(int a){
        if (a > 0) {
            return 1; 
        };
     return 0;
    }
    )code"};

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) + "// MARKERS END\n" +
      R"code(int foo(int a){
        if ( a > 0) {

            DCEMARKERMACRO1_

            return 1;
        }

            else {
            DCEMARKERMACRO0_
        }

        ;
        return 0;
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter if-else", "[if]") {
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

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) + "// MARKERS END\n" +
      R"code(int foo(int a){
        if ( a > 0)

        {

            DCEMARKERMACRO1_

            a = 1;

        } 

        else 

        {

            DCEMARKERMACRO0_

            a = 0;
        }

        return a;
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter if with return macro", "[if][macro]") {
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

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) + "// MARKERS END\n" +
      R"code(#define R return

    int foo(int a){
        if ( a > 0)

        {
        
            DCEMARKERMACRO1_ 

            R 0;

        }

        else {
            DCEMARKERMACRO0_ 
        }

        return a;
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, false));
  compare_code(formatCode(Code), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter if with return macro 2", "[if][macro]") {
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

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) + "// MARKERS END\n" +
      R"code(#define R return 0

    int foo(int a){
        if ( a > 0)

        {

            DCEMARKERMACRO1_

            R;

        }


        else {
            DCEMARKERMACRO0_
        }

        return a;
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, false));
  compare_code(formatCode(Code), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter if with return macro 3", "[if][macro]") {
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

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) + "// MARKERS END\n" +
      R"code(#define R return 0;

    int foo(int a){
        if ( a > 0)

        {

            DCEMARKERMACRO1_

            R

        }


        else {
            DCEMARKERMACRO0_
        }

        return a;
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, false));
  compare_code(formatCode(Code), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter nested if with macro", "[if][nested][macro]") {
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

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::DCEInstrumenter::makeMarkerMacros(0) +
                      markers::DCEInstrumenter::makeMarkerMacros(1) +
                      markers::DCEInstrumenter::makeMarkerMacros(2) +
                      markers::DCEInstrumenter::makeMarkerMacros(3) +
                      "// MARKERS END\n" + +R"code(#define A a = 1;
    int foo(int a){
        if ( a > 0)

        {

            DCEMARKERMACRO1_

                if ( a==1) 

                      {

                          DCEMARKERMACRO3_

                          A

                      }

                else 

                {

                  DCEMARKERMACRO2_

                    a = 2;
                }

        }

        else {
          DCEMARKERMACRO0_
        }

        return 0;
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode),
               formatCode(runDCEInstrumenterOnCode(Code, false)));
  compare_code(formatCode(Code), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter nested if with return", "[if][return][nested]") {
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

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) +
      markers::DCEInstrumenter::makeMarkerMacros(2) +
      markers::DCEInstrumenter::makeMarkerMacros(3) + "// MARKERS END\n" +
      R"code(int foo(int a){
        if ( a >= 0)

        {

           DCEMARKERMACRO1_

              if ( a >= 1)

              {

                DCEMARKERMACRO3_

                return 1;
                
              }

                else {
                    DCEMARKERMACRO2_
                }

                }


        else {
            DCEMARKERMACRO0_
        }

        return 0;
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter if return macro and comment",
          "[loop][if][macro][return]") {

  auto Code = R"code(#define X 0
    int foo() {
    if (1)
        return X /* comment */;
    })code";

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) + "// MARKERS END\n" +
      R"code(#define X 0
    int foo() {
        if (1)

    {

       DCEMARKERMACRO1_

        return X /* comment */;

    }

        else {
           DCEMARKERMACRO0_
        }

    })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, false));
  compare_code(formatCode(Code), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter if return macro", "[loop][if][macro][return]") {

  auto Code = R"code(#define BUG
    void foo() {
    if (1)
        return BUG;
    })code";

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) + "// MARKERS END\n" +
      R"code(#define BUG
    void foo() {
        if ( 1)

    {

       DCEMARKERMACRO1_

        return BUG;

    }

        else {
           DCEMARKERMACRO0_
        }
    })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, false));
  // XXX: this doesn't work, containsMacroExpansions
  // can't match the BUG macro
  // compare_code(formatCode(Code),
  // runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter if with semi return macro",
          "[loop][if][macro][return]") {

  auto Code = R"code(#define BUG ;
    void foo() {
    if (1)
        return BUG
    })code";

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) + "// MARKERS END\n" +
      R"code(#define BUG ;
    void foo() {
        if ( 1)

    {

       DCEMARKERMACRO1_

        return BUG

    }

        else {
           DCEMARKERMACRO0_
        }
    })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, false));
  compare_code(formatCode(Code), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter if-else with semi return macro",
          "[loop][if][macro][return]") {

  auto Code = R"code(#define BUG ;
    void foo() {
    if (1)
        return BUG
    else
        return;
    })code";

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) + "// MARKERS END\n" +
      R"code(#define BUG ;
    void foo() {
        if ( 1)

    {

       DCEMARKERMACRO1_

        return BUG

    }

        else 

        {

           DCEMARKERMACRO0_

            return;
        }

    })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, false));
  compare_code(formatCode(Code), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter if-else nested with while", "[if][loop][while]") {
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
  // Code = formatCode(Code);

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) +
      markers::DCEInstrumenter::makeMarkerMacros(2) + "// MARKERS END\n" +
      R"code(int foo(int a) {
        if ( a > 0)

        {

           DCEMARKERMACRO1_

            while( a--)

            {

               DCEMARKERMACRO2_

                return 1;
            }

        } 

        else

        {
        
           DCEMARKERMACRO0_

            a = 0;
        }

        return a;
    })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter while stmt", "[while][loop]") {

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

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::DCEInstrumenter::makeMarkerMacros(0) +
                      "// MARKERS END\n" +
                      R"code(int foo(int a){
        int b = 0;
            while(true)

            {

               DCEMARKERMACRO0_

                return 0;

            }

        return b;
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter nested for stmt", "[for][if][nested][return]") {

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

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) + "// MARKERS END\n" +
      R"code(int foo(int a){
        for ( ;;)

        {

           DCEMARKERMACRO0_

        for ( ;;)

            {

               DCEMARKERMACRO1_

                ++a;

            }

        }
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode),
               formatCode(runDCEInstrumenterOnCode(Code, true)));
}

TEST_CASE("DCEInstrumenter for stmt nested if with return",
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

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) +
      markers::DCEInstrumenter::makeMarkerMacros(2) + "// MARKERS END\n" +
      R"code(int foo(int a){
        int b = 0;
        for ( int i = 0; i < a; ++i)

        {

           DCEMARKERMACRO0_


        if ( i == 3)

            {

               DCEMARKERMACRO2_

                return b;

            }


             else 

             {

               DCEMARKERMACRO1_

                ++b;
            }

        }

        return b;
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter for stmt nested if with return and extra stmt",
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

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) +
      markers::DCEInstrumenter::makeMarkerMacros(2) + "// MARKERS END\n" +
      R"code(int foo(int a){
        int b = 0;
        for ( int i = 0; i < a; ++i) {

           DCEMARKERMACRO0_

        if ( i == 3)

            {

               DCEMARKERMACRO2_

                return b;

            }

             else 

             {

               DCEMARKERMACRO1_

                ++b;
            }

        ++b;
        }
        return b;
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter for stmt with return", "[for][return]") {

  auto Code = R"code(int foo(int a){
        int b = 0;
        for (int i = 0; i < a; ++i)
            return i;
        return b;
    }
    )code";

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::DCEInstrumenter::makeMarkerMacros(0) +
                      "// MARKERS END\n" +
                      R"code(int foo(int a){
        int b = 0;
        for ( int i = 0; i < a; ++i)

        {

           DCEMARKERMACRO0_

            return i;

        }

        return b;
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter do while stmt with return", "[do][return]") {

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

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::DCEInstrumenter::makeMarkerMacros(0) +
                      "// MARKERS END\n" +
                      R"code(int foo(int a){
        int b = 0;
        do 

        {
        
         DCEMARKERMACRO0_

          return b;

        } 

        while(b<10);
        return b;
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter do while and if with return",
          "[if][dowhile][return][macro]") {
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

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) +
      markers::DCEInstrumenter::makeMarkerMacros(2) + "// MARKERS END\n" +
      R"code(#define X 1
        int foo(int a) {
          do 

          {

           DCEMARKERMACRO0_

            if ( a + 1 == 2)

            {

           DCEMARKERMACRO2_

            return X;

            }

            else {
               DCEMARKERMACRO1_
            }

          } 

          while (++a);
          return 0;
    })code";
  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, false));
  compare_code(formatCode(Code), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter do while and if else with return",
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

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) +
      markers::DCEInstrumenter::makeMarkerMacros(2) + "// MARKERS END\n" +
      R"code(int foo(int a) {
            if ( a)

            {

               DCEMARKERMACRO1_

              do 

              {

               DCEMARKERMACRO2_

                --a;

              } 

              while (a);

            }

            else 

            {

               DCEMARKERMACRO0_

                return 1;
            }

          return 0;
    })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter if dowhile with nested macro",
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

  auto ExpectedCode = "// MARKERS START\n" +
                      markers::DCEInstrumenter::makeMarkerMacros(0) +
                      markers::DCEInstrumenter::makeMarkerMacros(1) +
                      "// MARKERS END\n" + +R"code(#define M
        #define bar    \
        do  {          \
        } while (0) M

        void foo() {
            if ( 1)

            {

               DCEMARKERMACRO1_

              bar; 

            }

            else
            {
               DCEMARKERMACRO0_
            }
            
    })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, false));
  compare_code(formatCode(Code), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter if while do and braces without whitespace",
          "[if][do][return]") {

  auto Code = R"code(void foo() {
        while (1) {}
        if (1) {}
        do {} while(1);
        if (1);
    })code";

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) +
      markers::DCEInstrumenter::makeMarkerMacros(2) +
      markers::DCEInstrumenter::makeMarkerMacros(3) +
      markers::DCEInstrumenter::makeMarkerMacros(4) +
      markers::DCEInstrumenter::makeMarkerMacros(5) + "// MARKERS END\n" +
      R"code(void foo() {
        while ( 1) {

       DCEMARKERMACRO0_
        }
  if ( 1) {

     DCEMARKERMACRO2_

    }

  else {
     DCEMARKERMACRO1_
    }

        do {

       DCEMARKERMACRO3_

        } while(1);
  if ( 1)

  {

     DCEMARKERMACRO5_

      ;

    }

  else {
     DCEMARKERMACRO4_
    }

    })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, false));
}

TEST_CASE("DCEInstrumenter switch", "[switch][return]") {
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
  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) +
      markers::DCEInstrumenter::makeMarkerMacros(2) +
      markers::DCEInstrumenter::makeMarkerMacros(3) +
      markers::DCEInstrumenter::makeMarkerMacros(4) +
      markers::DCEInstrumenter::makeMarkerMacros(5) + "// MARKERS END\n" +
      R"code(int foo(int a){
        switch(a){
        case 1: 

           DCEMARKERMACRO0_

            a = 2;
            break;
        case 2:

         DCEMARKERMACRO5_

        case 3:

          DCEMARKERMACRO4_

           break;
        case 4:

         DCEMARKERMACRO3_

          return 3;
        case 5:

         DCEMARKERMACRO2_

          {a = 5;}
        default:

         DCEMARKERMACRO1_

          a = 42;

        }
        return a;
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, false));
}

TEST_CASE("DCEInstrumenter cascaded switch", "[switch]") {

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

  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) +
      markers::DCEInstrumenter::makeMarkerMacros(2) +
      markers::DCEInstrumenter::makeMarkerMacros(3) +
      markers::DCEInstrumenter::makeMarkerMacros(4) + "// MARKERS END\n" +
      R"code(int foo(int a) {
                    switch (a) {
                  case 0:

                   DCEMARKERMACRO0_

                    a = 1;
                      break;
                  default:

                   DCEMARKERMACRO2_

                  case 1:

                   DCEMARKERMACRO4_

                  case 2:

                   DCEMARKERMACRO3_

                    a = 2;
                      break;
                  case 3:

                   DCEMARKERMACRO1_

                    break;
                    }
                  }
)code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter empty switch", "[switch]") {
  auto Code = R"code(int foo(int a){
        switch(a){
        }
        return a;
    }
    )code";

  CAPTURE(Code);
  compare_code(formatCode(Code), runDCEInstrumenterOnCode(Code, false));
}

TEST_CASE("DCEInstrumenter switch if and macro", "[if][switch][macro]") {
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
  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) +
      markers::DCEInstrumenter::makeMarkerMacros(2) + "// MARKERS END\n" +
      R"code(#define TEST bar

                        int bar();

                        void baz(int a) {
                            switch (a) {
                            case 1:

                               DCEMARKERMACRO0_

                                TEST();
                            }
                        }

                        void foo(int a) {
                              if ( a)

                            {

                               DCEMARKERMACRO2_

                                a = 1;

                                }

                              else {
                                 DCEMARKERMACRO1_
                              }

                        })code";
  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, false));
  auto ExpectedCodeMacroIgnore =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) + "// MARKERS END\n" +
      R"code(#define TEST bar

                        int bar();

                        void baz(int a) {
                            switch (a) {
                            case 1:
                                TEST();
                            }
                        }

                        void foo(int a) {
                              if ( a)

                            {

                               DCEMARKERMACRO1_

                                a = 1;

                                }

                              else {
                                 DCEMARKERMACRO0_
                              }

                        })code";
  compare_code(formatCode(ExpectedCodeMacroIgnore),
               runDCEInstrumenterOnCode(Code, true));
}

TEST_CASE("DCEInstrumenter switch if with return and macro",
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
  auto ExpectedCode =
      "// MARKERS START\n" + markers::DCEInstrumenter::makeMarkerMacros(0) +
      markers::DCEInstrumenter::makeMarkerMacros(1) +
      markers::DCEInstrumenter::makeMarkerMacros(2) +
      markers::DCEInstrumenter::makeMarkerMacros(3) +
      markers::DCEInstrumenter::makeMarkerMacros(4) + "// MARKERS END\n" +
      R"code(#define FFFF 1
      int foo() {
              if ( 1)

        {

         DCEMARKERMACRO1_

          switch (1) {
            default:

             DCEMARKERMACRO2_

              return FFFF;
            }

        } 

        else 

        {

         DCEMARKERMACRO0_

            if ( 1)

            {

           DCEMARKERMACRO4_

            return FFFF;

            }

            else {
               DCEMARKERMACRO3_
              }

        }

    })code";

  CAPTURE(Code);
  compare_code(formatCode(ExpectedCode), runDCEInstrumenterOnCode(Code, false));
  compare_code(formatCode(Code), runDCEInstrumenterOnCode(Code, true));
}
