
#define CONTEXPT            constexpr                       
#define INLENU             inl##ine            
#define TRUE             true       
#define FALSE             false       

#define CONSTINBOOL    constexpr     inline   bool    
#define is_space(arg) is_space(arg)

CONSTINBOOL is_space(char c) {  //
  return c == ' ' || '\t' <= c && c <= '\r';
}

CONSTINBOOL is_digit(char c) {
  switch (c) {
    case '0' ... '9':
      return true;
    default:
      break;
  }
  return false;
}
