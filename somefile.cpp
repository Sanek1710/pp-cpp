begin

  g   
       k
" \\this \ 
is\ 
 a\  
 str\aing\
" inbetween

// and\
here\    
is \  
the\
comment
g
end

#define RAW R"(some\
raw\
string\
)"

// RAW

#define Y R
#define M Y##""
#define R M
R