#ifndef VMGO_H
#define VMGO_H

#include <ruby/ruby.h>



#ifdef __cplusplus
extern "C" {
#endif

    void Init_forgo();
    void *gx_Qt_class_new(int argc,  void *argv, void *self);
    void *gx_Qt_class_method_missing(int argc, void *argv, void *obj);
    
#ifdef __cplusplus
};
#endif


#endif /* VMGO_H */
