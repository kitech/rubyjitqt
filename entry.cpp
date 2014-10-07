
#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include <QtNetwork>
#include <QtWebSockets>

#include <cassert>
#include <functional>
#include <typeinfo>


#include <llvm/ExecutionEngine/GenericValue.h>

#include "debugoutput.h"

#include "ruby.hpp"
#include "qom.h"

#include "utils.h"
#include "entry.h"

#include "clvm.h"

// #include "rcoreapplication.h"

#include "metalize/kitech_qtcore.h"
#include "metalize/kitech_qtnetwork.h"
#include "metalize/metar_classes_qtcore.h"
#include "metalize/metas.h"


// extern "C" {

// #define SAVE_CI2(type, value) Qom::inst()->objs[rb_hash(self)] = (QObject*)value
// ci == cpp instance 
// #define GET_CI2(type) type *ci = (type*)Qom::inst()->objs[rb_hash(self)]


// #define FUNVAL (VALUE (*) (...))


/*
static VALUE x_QString_destructor(VALUE id)
{
    // qDebug()<<__FUNCTION__;

    VALUE os = rb_const_get(rb_cModule, rb_intern("ObjectSpace"));
    VALUE self = rb_funcall(os, rb_intern("_id2ref"), 1, id);
    // VALUE self = ID2SYM(id);
    GET_CI3(QString);
    qDebug()<<__FUNCTION__<<ci<<NUM2ULONG(id)<<QString("%1").arg(TYPE(self));
    delete ci;

    return Qnil;
}

static VALUE x_QString_init(VALUE self)
{
    yQString *s = NULL;
    s = new yQString();
    qDebug()<<s;

    // rb_iv_set(self, "@_ci", (VALUE)s);
    // Qom::inst()->objs[rb_to_id(self)] = (QObject*)s;
    SAVE_CI2(QString, s);
    // qDebug()<<TYPE(self);
    // rb_hash(self);

    VALUE free_proc = rb_proc_new((VALUE (*) (...)) x_QString_destructor, 0);
    rb_define_finalizer(self, free_proc);

    return self;
}

static QString * x_QString_to_q(VALUE self)
{
    GET_CI3(QString);
    return ci;
}

static VALUE x_QString_to_s(VALUE self)
{
    // QString *s = (QString*)rb_iv_get(self, "@_ci");
    GET_CI3(QString);

    QString to_s;
    QDebug out(&to_s);
    out << ci << "<--"<< ci->length();

    return (VALUE)rb_str_new2(to_s.toLatin1().data());
}

static VALUE x_QString_append(VALUE self, VALUE obj)
{
    // QString *ci = (QString*)rb_iv_get(self, "@_ci");
    GET_CI3(QString);
    
    if (TYPE(obj) == RUBY_T_STRING) {
        ci->append(RSTRING_PTR(obj));
    } else {
        qDebug()<<TYPE(obj);
        ci->append(RSTRING_PTR(x_QString_to_s(obj)));
    }

    return self;
}
*/

// #define VariantOrigValue(v)                         
#define VOValue(v)                            \
    (                                                  \
     (v.type() == QVariant::Invalid) ? QVariant() :    \
     (v.type() == QVariant::Bool) ? v.toBool() :       \
     (v.type() == QVariant::Int) ? v.toInt() :         \
     (v.type() == QVariant::Double) ? v.toDouble() :   \
     (v.type() == QVariant::Char) ? v.toChar() :       \
     (v.type() == QVariant::String) ? v.toString() :   \
     (v.type() == QVariant::Point) ? v.toPoint() :     \
     QVariant())


static void abc()
{
    QVariant v;

    // error, error: non-pointer operand type 'QPoint' incompatible with nullptr
    // 同一表达式的返回值类型应该相同啊。
    //*
    (
     (v.type() == QVariant::Invalid) ? QVariant() : 
     (v.type() == QVariant::Bool) ? v.toBool() :
     (v.type() == QVariant::Int) ? v.toInt() :
     (v.type() == QVariant::Double) ? v.toDouble() :
     (v.type() == QVariant::Char) ? v.toChar() :
     (v.type() == QVariant::String) ? v.toString() :
     (v.type() == QVariant::Point) ? v.toPoint() : QVariant()
     );
        // */
}

typedef struct {
    int iretval;
    bool bretval;
    QString sretval;
} ReturnStorage;

typedef struct {
    int iretval;
    bool bretval;
    QString sretval;

    // 最大支持10个参数，参数的临时值放在这
    int ival[10];
    bool bval[10];
    QString sval[10];
} InvokeStorage;

static QGenericReturnArgument makeRetArg(int type, ReturnStorage &rs)
{
    QGenericReturnArgument retval;

    switch (type) {
    case QMetaType::Int: retval = Q_RETURN_ARG(int, rs.iretval); break;
    case QMetaType::Bool: retval = Q_RETURN_ARG(bool, rs.bretval); break;
    case QMetaType::QString: retval = Q_RETURN_ARG(QString, rs.sretval); break;
    }

    return retval;
}

static VALUE retArg2Value(int type, QGenericReturnArgument retarg)
{
    VALUE retval = Qnil;
    ReturnStorage rs;

    switch (type) {
    case QMetaType::Int:
        rs.iretval = *((int*)retarg.data());
        retval = INT2NUM(rs.iretval);
        break;
    case QMetaType::QString:
        rs.sretval = *((QString*)retarg.data());
        retval = rb_str_new2(rs.sretval.toLatin1().data());
        break;
    case QMetaType::Bool:
        rs.bretval = *((bool*)retarg.data());
        retval = rs.bretval ? Qtrue : Qfalse;
        break;
    case QMetaType::Void:
        
        break;
    default:
        qDebug()<<"unknown ret type:"<<type<<retarg.name()<<retarg.data();
        break;
    };

    return retval;
}

static QGenericArgument Variant2Arg(int type, QVariant &v, int idx, InvokeStorage &is)
{
    QGenericArgument valx;

    switch (type) {
    case QMetaType::Int: 
        is.ival[idx] = v.toInt();
        valx = Q_ARG(int, is.ival[idx]);
        break;
    case QMetaType::QString: 
        is.sval[idx] = v.toString();
        valx = Q_ARG(QString, is.sval[idx]); 
        break;
    case QMetaType::Bool:
        is.bval[idx] = v.toBool();
        valx = Q_ARG(bool, is.bval[idx]);
        break;
    }

    return valx;
}

static QVariant VALUE2Variant(VALUE v)
{
    QVariant rv;

    switch (TYPE(v)) {
    case T_NONE:  rv = QVariant(); break;
    case T_FIXNUM: rv = (int)FIX2INT(v); break;
    case T_STRING:  rv = RSTRING_PTR(v); break;
    case T_FLOAT:  rv = RFLOAT_VALUE(v); break;
    case T_NIL:   rv = 0; break;
    case T_TRUE:  rv = true; break;
    case T_FALSE: rv = false; break;
    case T_CLASS:
        
    default:
        qDebug()<<"unknown VALUE type:"<<TYPE(v);
        break;
    }

    return rv;
}


/*
  解决类中的enum的处理
  TODO:
  使用staticMetaObject检测enum变量。
 */
VALUE x_Qt_meta_class_const_missing(int argc, VALUE *argv, VALUE obj)
{
    qDebug()<<argc<<TYPE(obj)<<TYPE(argv[0]);
    qDebug()<<QString(rb_id2name(SYM2ID(argv[0])))<<rb_class2name(obj);
    QString klass_name = QString(rb_class2name(obj));
    QString const_name = QString(rb_id2name(SYM2ID(argv[0])));
    QString yconst_name = "y" + const_name;
    // qDebug()<<klass_name<<const_name;

    if (klass_name == "Qt5::QHostAddress") {
        //  if (const_name == "AnyIPv4") return INT2NUM(QHostAddress::AnyIPv4);
    }
    
    VALUE *targv = NULL;
    int targc = 0;
    VALUE self = rb_class_new_instance(targc, targv, obj);
    GET_CI0();
    const QMetaObject *mo = ci->metaObject();
    // qDebug()<<"===="<<ci<<mo<<"----";

    // qDebug()<<"ec:"<<mo->enumeratorCount();
    for (int i = 0; i < mo->enumeratorCount(); i++) {
        QMetaEnum me = mo->enumerator(i);
        bool ok = false;
        // qDebug()<<"enum:"<<i<<""<<me.name();
        // qDebug()<<"enum2:"<<me.keyCount()<<me.keyToValue(const_name.toLatin1().data(), &ok)<<ok;
        int enum_val = me.keyToValue(yconst_name.toLatin1().data(), &ok);
        if (ok) {
            rb_gc_mark(self);
            return INT2NUM(enum_val);
        }

        continue; // below for debug/test purpose
        for (int j = 0; j < me.keyCount(); j++) {
            qDebug()<<"enum2:"<<j<<me.key(j);

            if (QString(me.key(i)) == yconst_name) {
                
            }
        }
    }

    // not found const
    VALUE exception = rb_eException;
    rb_raise(exception, "NameError: uninitialized yconstant %s::%s.\n",
             klass_name.toLatin1().data(), const_name.toLatin1().data());
    /*
    rb_fatal("NameError: uninitialized constant %s::%s.\n",
             klass_name.toLatin1().data(), const_name.toLatin1().data());
    */

    return Qnil;
}

static VALUE x_Qt_meta_class_dtor_jit(VALUE id)
{
    VALUE os = rb_const_get(rb_cModule, rb_intern("ObjectSpace"));
    VALUE self = rb_funcall(os, rb_intern("_id2ref"), 1, id);
    QString klass_name = QString(rb_class2name(RBASIC_CLASS(self)));
    klass_name = klass_name.split("::").at(1);
    qDebug()<<"dtor:"<<klass_name;

    void *ci = Qom::inst()->jdobjs[rb_hash(self)];
    qDebug()<<"herhe:"<<ci;

    // TODO
    // 函数为void*时程序崩溃，这是llvm的bug还是使用有问题。
    QString code_src = QString("#include <QtCore>\n"
                               "void jit_main(int a, void *ci) {"
                               "qDebug()<<\"test int:\"<<a;"
                               "qDebug()<<\"in jit:\"<<ci;\n"
                               "delete (%1*)ci;}").arg(klass_name);
    QVector<llvm::GenericValue> gvargs;
    llvm::GenericValue ia;
    ia.IntVal = llvm::APInt(32, 567);
    gvargs.push_back(ia);
    gvargs.push_back(llvm::PTOGV(ci));
    qDebug()<<"view conv back:"<<llvm::GVTOP(gvargs.at(0));

    // delete (QString*)ci;
    llvm::GenericValue gvret = vm_execute(code_src, gvargs);

    return Qnil;
}

/*
  类实例的析构函数  
  TODO:
  使用通用方法之后，宏SAVE_XXX和GET_XXX就可以不需要了。
 */
static VALUE x_Qt_meta_class_dtor(VALUE id)
{
    VALUE os = rb_const_get(rb_cModule, rb_intern("ObjectSpace"));
    VALUE self = rb_funcall(os, rb_intern("_id2ref"), 1, id);

    GET_CI0();
    delete ci;

    return Qnil;
}

/*
  通过Qt类的初始化函数
  获取要实例化的类名，从staticMetaObject加载类信息，
  使用Qt的QMetaObject::newInstance创建新的实例对象。
  TODO:
  处理初始化时的参数。
 */
VALUE x_Qt_meta_class_init_jit(int argc, VALUE *argv, VALUE self)
{
    qDebug()<<argc<<TYPE(self);
    QString klass_name = QString(rb_class2name(RBASIC_CLASS(self)));
    klass_name = klass_name.split("::").at(1);
    QString yklass_name = QString("y%1").arg(klass_name);
    qDebug()<<"class name:"<<klass_name;
    
    auto code_templater = [] () -> QString * {
        return new QString();
    };

    QString code_src = QString("#include <stdio.h>\n"
                               "#include <QtCore>\n"
                               "%1 * jit_main() {\n"
                               "%1* ci = new %1(); qDebug()<<\"in jit:\"<<ci; return ci; }")
        .arg(klass_name);
    
    const char *code = "#include <stdio.h>\n"
        "#include <QtCore>\n"
        "\nint main() { printf (\"hello IR JIT from re.\"); QString abc; abc.append(\"123\"); qDebug()<<abc; return 56; }"
        "\nint yamain() { QString abc; abc.append(\"123\"); qDebug()<<abc; return main(); }";

    QVector<QVariant> args;
    void *jo = jit_vm_new(klass_name, args);
    qDebug()<<jo;
    Qom::inst()->jdobjs[rb_hash(self)] = jo;

    if (0) {
        QVector<llvm::GenericValue> envp;
        // void *vret = vm_execute(QString(code), envp);

        llvm::GenericValue gvret = jit_vm_execute(code_src, envp);
        void *ci = llvm::GVTOP(gvret);
        Qom::inst()->jdobjs[rb_hash(self)] = ci;
        qDebug()<<"newed ci:"<<ci;
    }

    VALUE free_proc = rb_proc_new(FUNVAL x_Qt_meta_class_dtor_jit, 0);
    // rb_define_finalizer(self, free_proc);

    if (0) {
        // QString *str = (QString*)llvm::GVTOP(gvret);
        // str->append("123456");
    
        // QString str2(*str);
        // qDebug()<<code_src<<*str<<str->length();
        // delete str; str = NULL;
    }

    return self;
}

VALUE x_Qt_meta_class_init(int argc, VALUE *argv, VALUE self)
{
    qDebug()<<argc<<TYPE(self);
    QString klass_name = QString(rb_class2name(RBASIC_CLASS(self)));
    klass_name = klass_name.split("::").at(1);
    QString yklass_name = QString("y%1").arg(klass_name);
    qDebug()<<"class name:"<<klass_name;
    
    if (!__rq_metas.contains(yklass_name)) {
        qDebug()<<"not supported class:"<<klass_name;
        return Qnil;
    }

    const QMetaObject *mo = __rq_metas.value(yklass_name);
    QObject * ci = mo->newInstance();
    SAVE_CI0(ci);

    VALUE free_proc = rb_proc_new(FUNVAL x_Qt_meta_class_dtor, 0);
    rb_define_finalizer(self, free_proc);

    return self;
    return Qnil;
}

// test method
/* 
// good test code for trace llvm ir call 
QString &test_ir_objref(YaQString *pthis, QString &str)
{
    qDebug()<<"pthis:"<<pthis;
    qDebug()<<"&str:"<<&str;
    qDebug()<<"str:"<<str;
    return str;
}
*/

/*
  stack structure:
  [0] => SYM function name
  [1] => arg0
  [2] => arg1
  [3] => arg2
  ...
 */
VALUE x_Qt_meta_class_method_missing_jit(int argc, VALUE *argv, VALUE self)
{
    void *jo = Qom::inst()->jdobjs[rb_hash(self)];
    void *ci = jo;
    qDebug()<<ci;
    assert(ci != 0);
    QString klass_name = QString(rb_class2name(RBASIC_CLASS(self)));
    klass_name = klass_name.split("::").at(1);
    QString method_name = QString(rb_id2name(SYM2ID(argv[0])));
    qDebug()<<"calling:"<<klass_name<<method_name<<argc<<(argc > 1);
    assert(argc >= 1);

    QVector<QVariant> args;
    for (int i = 0; i < argc; i ++) {
        if (i == 0) continue;
        if (i >= argc) break;

        qDebug()<<"i == "<< i << (i<argc) << (i>argc);

        args << VALUE2Variant(argv[i]);
        qDebug()<<"i == "<< i << (i<argc) << (i>argc)<<VALUE2Variant(argv[i]);
    }

    // fix try_convert(obj) → array or nil
    if (method_name == "to_ary") {
        return Qnil;
    }

    if (method_name == "to_s") {
        return Qnil;
    }

    YaQString *ts = (YaQString*)ci;
    ts->append("1234abc");
    qDebug()<<(*ts)<<ts->toUpper()<<ts->startsWith(QChar('r'))
            <<ts->lastIndexOf("876");

    QVariant gv = jit_vm_call(ci, klass_name, method_name, args);
    qDebug()<<"gv:"<<gv;

    return Qnil;
}

/*
  stack structure:
  [0] => SYM function name
  [1] => arg0
  [2] => arg1
  [3] => arg2
  ...
 */
VALUE x_Qt_meta_class_method_missing(int argc, VALUE *argv, VALUE self)
{
    GET_CI0();
    qDebug()<<ci;
    assert(ci != 0);
    const QMetaObject *mo = ci->metaObject();
    QString klass_name = "QString";
    QString method_name = QString(rb_id2name(SYM2ID(argv[0])));
    qDebug()<<"calling:"<<klass_name<<method_name<<argc<<(argc > 1);
    assert(argc >= 1);

    QVector<QVariant> args;
    for (int i = 0; i < argc; i ++) {
        if (i == 0) continue;
        if (i >= argc) break;

        qDebug()<<"i == "<< i << (i<argc) << (i>argc);

        args << VALUE2Variant(argv[i]);
        qDebug()<<"i == "<< i << (i<argc) << (i>argc)<<VALUE2Variant(argv[i]);
    }

    // fix try_convert(obj) → array or nil
    if (method_name == "to_ary") {
        return Qnil;
    }

    if (method_name == "to_s") {
        return Qnil;
    }
    
    // auto meta invoke
    QString msig;
    for (QVariant &item : args) {
        msig.append(item.typeName()).append(',');
    }

    msig[msig.length()-1] = msig[msig.length()-1] == ',' ? QChar(' ') : msig[msig.length()-1];
    msig = msig.trimmed();
    msig = QString("%1(%2)").arg(method_name).arg(msig);

    int midx = mo->indexOfMethod(msig.toLatin1().data());
    if (midx == -1) {
        for (int i = 0; i < mo->methodCount(); i ++) {
            QMetaMethod mm = mo->method(i);
            qDebug()<<"mmsig:"<<mm.returnType()<<mm.typeName()<<mm.methodSignature();
            if (QString(mm.methodSignature()).replace("const ", "") == msig) {
                midx = i;
                break;
            }
        }
    } 

    if (midx == -1) {
        qDebug()<<"method not found:"<<msig;
    } else {        
        QMetaMethod mm = mo->method(midx);
        // qDebug()<<"mmsig:"<<mm.methodSignature();
        ReturnStorage rs;
        InvokeStorage is;
        QGenericReturnArgument retval;
        QGenericArgument val0, val1, val2, val3, val4, val5, val6, val7, val8, val9;
        int rargc = argc - 1;
        qDebug()<<"retun type:"<<mm.returnType()<<mm.typeName()<<mm.parameterCount()<<rargc;

        if (argc - 1 > mm.parameterCount()) {
            qDebug()<<"maybe you passed too much parameters:"
                    <<QString("need %1, given %2.").arg(mm.parameterCount()).arg(rargc);
        }

        retval = makeRetArg(mm.returnType(), rs);

        bool bret = false;
        // QString tmpv0;
        switch (rargc) {
        case 0:
            // ci->append("123");
            bret = mm.invoke(ci, retval);//Q_RETURN_ARG(int, retval)); 
            break;
        case 1:
            val0 = Variant2Arg(mm.parameterType(0), args[0], 0, is);
            bret = mm.invoke(ci, retval, val0);
            
            // tmpv0 = args[0].toString(); // 如果使用在Q_ARG中直接使用这个，这个临时对象地址会消失
            // val0 = Q_ARG(QString, tmpv0);
            // bret = QMetaObject::invokeMethod(ci, "append", val0);
            // bret = mm.invoke(ci, retval, val0);
            break;
        case 2:
            val0 = Variant2Arg(mm.parameterType(0), args[0], 0, is);
            val1 = Variant2Arg(mm.parameterType(1), args[1], 1, is);
            bret = mm.invoke(ci, retval, val0, val1);
            break;
        case 3:
            val0 = Variant2Arg(mm.parameterType(0), args[0], 0, is);
            val1 = Variant2Arg(mm.parameterType(1), args[1], 1, is);
            val2 = Variant2Arg(mm.parameterType(2), args[2], 2, is);
            bret = mm.invoke(ci, retval, val0, val1, val2);
            break;
        case 4:
            qDebug()<<"invokkkk:"<<rargc<<4;
            val0 = Variant2Arg(mm.parameterType(0), args[0], 0, is);
            val1 = Variant2Arg(mm.parameterType(1), args[1], 1, is);
            val2 = Variant2Arg(mm.parameterType(2), args[2], 2, is);
            val3 = Variant2Arg(mm.parameterType(3), args[3], 3, is);
            bret = mm.invoke(ci, retval, val0, val1, val2, val3);
            break;
        case 5:
            val0 = Variant2Arg(mm.parameterType(0), args[0], 0, is);
            val1 = Variant2Arg(mm.parameterType(1), args[1], 1, is);
            val2 = Variant2Arg(mm.parameterType(2), args[2], 2, is);
            val3 = Variant2Arg(mm.parameterType(3), args[3], 3, is);
            val4 = Variant2Arg(mm.parameterType(4), args[4], 4, is);
            bret = mm.invoke(ci, retval, val0, val1, val2, val3, val4);
            break;
        default:
            qDebug()<<"not impled"<<argc;
            break;
        };
        qDebug()<<bret<<","<<retval.name()<<retval.data();

        VALUE my_retval = retArg2Value(mm.returnType(), retval);
        return my_retval;
    }

    return Qnil;
}


// static VALUE x_QString_method_missing_test(int argc, VALUE *argv, VALUE self);
/*
static VALUE x_QString_method_missing(int argc, VALUE *argv, VALUE self)
{
    GET_CI3(QString);
    qDebug()<<ci;
    
    QString method_name = QString(rb_id2name(SYM2ID(argv[0])));
    QString klass_name = "QString"; // TODO, dynamic way
    qDebug()<<"calling:"<<klass_name<<method_name<<argc<<(argc > 1);

    QVector<QVariant> args;
    for (int i = 0; i < argc; i ++) {
        if (i == 0) continue;
        if (i >= argc) break;

        qDebug()<<"i == "<< i << (i<argc) << (i>argc);

        args << VALUE2Variant(argv[i]);
        qDebug()<<"i == "<< i << (i<argc) << (i>argc)<<VALUE2Variant(argv[i]);
    }

    // fix try_convert(obj) → array or nil
    if (method_name == "to_ary") {
        return Qnil;
    }

    if (method_name == "to_s") {
        return Qnil;
    }

    // auto meta invoke
    QString msig;
    for (QVariant &item : args) {
        msig.append(item.typeName()).append(',');
    }

    msig[msig.length()-1] = msig[msig.length()-1] == ',' ? QChar(' ') : msig[msig.length()-1];
    msig = msig.trimmed();
    msig = QString("%1(%2)").arg(method_name).arg(msig);

    const QMetaObject *mo = ci->metaObject();
    int midx = mo->indexOfMethod(msig.toLatin1().data());
    if (midx == -1) {
        for (int i = 0; i < mo->methodCount(); i ++) {
            QMetaMethod mm = mo->method(i);
            qDebug()<<mm.methodSignature();
            if (QString(mm.methodSignature()).replace("const ", "") == msig) {
                midx = i;
                break;
            }
        }
    } 

    if (midx == -1) {
        qDebug()<<"method not found:"<<msig;
    } else {
        QMetaMethod mm = mo->method(midx);
        ReturnStorage rs;
        InvokeStorage is;
        QGenericReturnArgument retval;
        QGenericArgument val0, val1, val2, val3, val4, val5, val6, val7, val8, val9;
        // QVariant retval;
        int iretval = 1234567;
        QString sretval;
        QString sval[10] = {0};
        int ival[10] = {0};
        qDebug()<<"retun type:"<<mm.returnType()<<mm.typeName()<<mm.parameterCount();

        if (argc - 1 > mm.parameterCount()) {
            qDebug()<<"maybe you passed too much parameters:"
                    <<QString("need %1, given %2.").arg(mm.parameterCount()).arg(argc - 1);
        }

        retval = makeRetArg(mm.returnType(), rs);

        bool bret = false;
        switch (argc - 1) {
        case 0:
            // ci->append("123");
            bret = mm.invoke(ci, retval);//Q_RETURN_ARG(int, retval)); 
            break;
        case 1:
            val0 = Variant2Arg(mm.parameterType(0), args[0], 0, is);
            bret = mm.invoke(ci, retval, val0);
            break;
        case 2:
            val0 = Variant2Arg(mm.parameterType(0), args[0], 0, is);
            val1 = Variant2Arg(mm.parameterType(1), args[1], 1, is);
            bret = mm.invoke(ci, retval, val0, val1);
            break;
        case 3:
            val0 = Variant2Arg(mm.parameterType(0), args[0], 0, is);
            val1 = Variant2Arg(mm.parameterType(1), args[1], 1, is);
            val2 = Variant2Arg(mm.parameterType(2), args[2], 2, is);
            bret = mm.invoke(ci, retval, val0, val1, val2);
            break;
        case 4:
            val0 = Variant2Arg(mm.parameterType(0), args[0], 0, is);
            val1 = Variant2Arg(mm.parameterType(1), args[1], 1, is);
            val2 = Variant2Arg(mm.parameterType(2), args[2], 2, is);
            val3 = Variant2Arg(mm.parameterType(3), args[3], 3, is);
            bret = mm.invoke(ci, retval, val0, val1, val2, val3);
            break;
        case 5:
            val0 = Variant2Arg(mm.parameterType(0), args[0], 0, is);
            val1 = Variant2Arg(mm.parameterType(1), args[1], 1, is);
            val2 = Variant2Arg(mm.parameterType(2), args[2], 2, is);
            val3 = Variant2Arg(mm.parameterType(3), args[3], 3, is);
            val4 = Variant2Arg(mm.parameterType(4), args[4], 4, is);
            bret = mm.invoke(ci, retval, val0, val1, val2, val3, val4);
            break;
        default:
            qDebug()<<"not impled"<<argc;
            break;
        };
        qDebug()<<bret<<","<<retval.name()<<retval.data()<<iretval
                <<((QString*)ci)->toLatin1()<<((QString*)ci)->length();

        VALUE my_retval = retArg2Value(mm.returnType(), retval);
        return my_retval;
    }

    // x_QString_method_missing_test(argc, argv, self);    
    return Qnil; // very importent, no return cause crash
}

static VALUE x_QString_method_missing_test(int argc, VALUE *argv, VALUE self)
{
    GET_CI3(QString);
    
    QString method_name = QString(rb_id2name(SYM2ID(argv[0])));
    QString klass_name = "QString"; // TODO, dynamic way
    qDebug()<<"calling:"<<klass_name<<method_name;

    QVector<QVariant> args;

    for (int i = 1; i < argc; i++) {
        args << VALUE2Variant(argv[i]);
    }

    const QMetaObject * mo = NULL;
    // YaQString ystr;
    // mo = ystr.metaObject();
    // qDebug()<<"ycall:"<<ystr.append("123");
    
    QString msig = "append(";
    for (int i = 1; i < argc; i++) {
        msig += QString(args[i-1].typeName());
        if (i < argc - 1) msig += ",";
    }
    msig += ")";
    // no space allowed in msig string. also no ref char &, but have const keyword.but dont need return value
    msig = "append(const char*)";
    qDebug()<<"method sig:"<<msig << mo->indexOfMethod(msig.toLatin1().data());

    for (int i = 0; i < mo->methodCount(); i++) {
        QMetaMethod mi = mo->method(i);
        qDebug()<< i<< mo->method(i).methodSignature()
                <<mi.returnType()
                <<QMetaObject::checkConnectArgs(msig.toLatin1().data(), mi.methodSignature());
    }


    if (method_name == "append") {
        switch (argc - 1) {
            // case 1: ci->append(VOValue(args[0])); break;
            // callrr(ci, method_name.toLatin1().data(), VOValue(args[0]));
            // case 2: ci->append(VOValue(args[0]), VOValue(args[1])); break;
        default:
            break;
        }
    }

    
    if (method_name == "length") {
        int len = std::bind(&QString::length, ci)();
        qDebug()<<method_name<<"=>"<<len;
        return rb_int2inum(len);
    }


    // void **pfun = Qom::inst()->getfp(klass_name, method_name);
    // assert(pfun != NULL);

    // qDebug()<<pfun;


    QString pieceCode = "";

    pieceCode += klass_name+"::"+"append(";
    for (int i = 1; i < argc; i++) {
        switch (TYPE(argv[i])) {
        case T_FIXNUM:
            pieceCode += "int ";
            break;
        case T_STRING:
            pieceCode += "const char * " + QString(RSTRING_PTR(argv[i])) + ",";
            break;
        }
    }
    if (pieceCode.at(pieceCode.length() - 1) == ',') 
        pieceCode = pieceCode.left(pieceCode.length() - 1);
    pieceCode += ");";


    qDebug()<<"auto gen code:" << pieceCode;

    // std::function<void()> ftor = std::bind(*pfun, ci);
    // ftor();
    // ci->*(*pfun)();
    // int (QString::*p)() const = (int (QString::*)())(*pfun);

    for (int i = 0; i < argc; i++) {
        qDebug()<<__FUNCTION__<<TYPE(argv[i]);
        switch (TYPE(argv[i])) {
        case T_STRING:
            qDebug()<<i<<"is string:"<<RSTRING_PTR(argv[i]);
            break;
        case T_SYMBOL:
            qDebug()<<i<<"is symbol:"<<rb_id2name(SYM2ID(argv[i]));
            break;
        case T_FIXNUM:
            qDebug()<<i<<"is num:"<<FIX2INT(argv[i]);
            break;
        default:
            qDebug()<<__FUNCTION__<<i<<",unknown arg type:"<<TYPE(argv[i]);
            break;
        }
    }

    // typeid(ci);

    qDebug()<<__FUNCTION__<<TYPE(self)<<TYPE(argv[0])<<argc;
    // qDebug()<<"'"<<rb_id2name(argv[0])<<"'";

    // Qom::inst()->testParser();
    Qom::inst()->testIR();

    if (0) {
    QString methodName = "startsWith";
    QString str = "rubyoo afwefafe";

    QLibrary core("/usr/lib/libQt5Core.so");
    QFunctionPointer fsym = core.resolve(methodName.toLatin1().data());
    }

    // bool t = fsym()(&str, "ruby");
    // qDebug()<<fsym<<t;


    if (0) {
         str.methodName(args);
         bind(fsym, &str);
         QVariant ret = std::bind(&QString::methodName, &str, _1, _2, _3 ...)();
         return ret;
      }
    



    return Qnil;
}
*/

// like class's static method
VALUE x_Qt_meta_class_singleton_method_missing(int argc, VALUE *argv, VALUE obj)
{
    qDebug()<<argc<<TYPE(argv[0])<<TYPE(obj);

    // const QMetaObject *mo = &yQObject::staticMetaObject;
    
    // qDebug()<<"qApp:"<<qApp;
    // for (int i = 0; i < mo->methodCount(); i++) {
    //     QMetaMethod mi = mo->method(i);
    //     qDebug()<< i<< mo->method(i).methodSignature()
    //             <<mi.returnType();
    // }

    // QString klass_name = QString(rb_class2name(obj));
    // QString const_name = QString(rb_id2name(SYM2ID(argv[0])));
    // qDebug()<<klass_name<<const_name;
    // qDebug()<<SLOT(on_emu_signal(int))<<SLOT(on_emu_signal())
    //         <<SIGNAL(emu_signal(int))<<SIGNAL(emu_signal());
    // // 1on_emu_signal(int) 1on_emu_signal()

    // yQObject *to = new yQObject;
    // if (klass_name == "Qt5::QObject") {
    //     if (const_name == "connect") {
    //         // standard connect
    //         QString slot_sig = QString("1%1").arg(RSTRING_PTR(argv[4]));
    //         QString signal_sig = QString("2%1").arg(RSTRING_PTR(argv[2]));
    //         QObject *slot_o = Qom::inst()->objs[rb_hash(argv[3])];
    //         QObject *signal_o = Qom::inst()->objs[rb_hash(argv[1])];
    //         qDebug()<<"slotooo:"<<slot_sig<<signal_sig<<slot_o<<signal_o;

    //         QObject::connect(signal_o, signal_sig.toLatin1().data(),
    //                          slot_o, slot_sig.toLatin1().data());

    //         // 
    //         // 
    //         // emit(signal_o->emu_signal(3));
    //         // another
    //     } else { // emit
    //         qDebug()<<"calling emit";
    //         const QMetaObject * mo = &yQObject::staticMetaObject;
    //         QObject *io = mo->newInstance();
    //         QObject *io2 = (decltype(io))io;
    //         qDebug()<<io<<io2;
    //     }
    // }

    return Qnil;
}


static VALUE x_QApplication_init(VALUE self)
{
    int argc = 1;
    char *argv[] = {(char*)"handby"};
    QApplication *app = new QApplication(argc, argv);

    SAVE_CI2(QApplication, app);

    return self;
}

static VALUE x_QApplication_exec(VALUE self)
{
    GET_CI2(QApplication);

    int n = ci->exec();

    return n;
}

static VALUE x_Qt_Constant_missing(int argc, VALUE* argv, VALUE self)
{
    qDebug()<<"hehhe constant missing."<<argc<<TYPE(self);

    for (int i = 0; i < argc; i++) {
        qDebug()<<__FUNCTION__<<TYPE(argv[i]);
        switch (TYPE(argv[i])) {
        case T_STRING:
            qDebug()<<i<<"is string:"<<RSTRING_PTR(argv[i]);
            break;
        case T_SYMBOL:
            qDebug()<<i<<"is symbol:"<<rb_id2name(SYM2ID(argv[i]));
            break;
        case T_FIXNUM:
            qDebug()<<i<<"is num:"<<FIX2INT(argv[i]);
            break;
        default:
            qDebug()<<__FUNCTION__<<i<<",unknown arg type:"<<TYPE(argv[i]);
            break;
        }
    }

    return 0;
}

static VALUE x_Qt_Method_missing(int argc, VALUE* argv, VALUE self)
{
    qDebug()<<"hehhe method missing."<< RSTRING_PTR(argv[0]);
    return 0;
}


extern "C" {
    // VALUE cQString;
    VALUE cQApplication;
    VALUE cModuleQt;

    void Init_handby()
    {
        qInstallMessageHandler(myMessageOutput);
        init_class_metas();

        cModuleQt = rb_define_module("Qt5");
        rb_define_module_function(cModuleQt, "const_missing", FUNVAL x_Qt_Constant_missing, -1);
        // rb_define_module_function(cModuleQt, "method_missing", FUNVAL x_Qt_Method_missing, -1);

        // register_QCoreApplication_methods(cModuleQt);
        register_qtcore_methods(cModuleQt);
        register_qtnetwork_methods(cModuleQt);

        // cQByteArray = rb_define_class("QByteArray", rb_cObject);
        // rb_define_method(cQByteArray, "initialize", FUNVAL x_QByteArray_init, 0);

        // cQThread = rb_define_class("QThread", rb_cObject);
        // rb_define_method(cQThread, "initializer", FUNVAL x_QThread_init, 0);

        // cQWidget = rb_define_class("QWidget", rb_cObject);
        // rb_define_method(cQWidget, "initialize", FUNVAL x_QWidget_init, 0);


        // 
        /*
        cQString = rb_define_class("QString", rb_cObject);
        rb_define_method(cQString, "initialize", (VALUE (*) (...)) x_QString_init, 0);
        // rb_define_method(cQString, "append", (VALUE (*) (...)) x_QString_append, 1);
        rb_define_method(cQString, "to_s", (VALUE (*) (...)) x_QString_to_s, 0);
        rb_define_method(cQString, "method_missing", FUNVAL x_QString_method_missing, -1);
        */

        // 
        cQApplication = rb_define_class("QApplication", rb_cObject);
        rb_define_method(cQApplication, "initialize", FUNVAL x_QApplication_init, 0);
        rb_define_method(cQApplication, "exec", FUNVAL x_QApplication_exec, 0);

    }

};// end extern "C"


