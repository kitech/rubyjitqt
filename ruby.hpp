#ifndef _CPP_RUBY_H
#define _CPP_RUBY_H

extern "C" {
    #include <ruby.h>
}; /* satisfy cc-mode */

/*
  实现ruby.h的C++封装      
*/
class CppRuby
{
public:
    CppRuby();
    virtual ~CppRuby();

private:
    
};

#endif /* _CPP_RUBY_H */

