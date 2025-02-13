//This is a comment with a #define that should be ignored
  int f;
/* /* This is a multiline comment with
#define <something>
that should be ignored /**/int c;
char* str = "A string with a \
   #define that should be ignored"; int b;
 /**/#/**/define/**/REAL_MACRO  /* this is expansion*/ int a\
;   int b; /*definitely sss*/char *sss = "\
some\
string";\
m;


#define a "hello\
worlds"

       "alrighty"; int blah;
a;
int foo() { 
  REAL_MACRO;
  return str + b - str; 
}
