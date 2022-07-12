#pragma once

#pragma comment( lib, "common.lib" )

#ifdef DEEP_DEBUG
#pragma comment( lib, "../../sdk/nw-gyp/ia32/debug/nw.dll.lib" )
#pragma comment( lib, "../../sdk/nw-gyp/ia32/debug/node.dll.lib" )
#pragma comment( lib, "../../sdk/nw-gyp/ia32/debug/v8.dll.lib" )
#pragma comment( lib, "../../sdk/nw-gyp/ia32/debug/v8_inlines.lib" )
#else // DEEP_DEBUG
#pragma comment( lib, "../../sdk/nw-gyp/ia32/nw.lib" )
#pragma comment( lib, "../../sdk/nw-gyp/ia32/node.lib" )
#pragma comment( lib, "../../sdk/nw-gyp/ia32/v8_inlines.lib" )
#endif // DEEP_DEBUG

#include <string>
#include <mutex>
#include <boost/core/noncopyable.hpp>

namespace TWD {
namespace common {

using namespace v8;
using namespace std;

class JSCall : private boost::noncopyable
{
public:
    JSCall() = default;
    virtual ~JSCall() = default;
    virtual void Prepare() {};
    virtual void DoRaw();
    virtual void Do(Isolate* isolate, Local<Context>& context) {};
    static void Run(const shared_ptr < JSCall >&);
};

typedef shared_ptr<Persistent<Function>> callback_t;

#ifndef callback_t_nullptr
    #define callback_t_nullptr callback_t{nullptr}
#endif

#ifndef toJS
    #define toJS JSCallSingleton::getInstance()->run
#endif

class JSCallSingleton
{
private:
  static JSCallSingleton* p_instance;
  // Конструкторы и оператор присваивания недоступны клиентам
  JSCallSingleton() {}
  JSCallSingleton(const JSCallSingleton&) {};
  JSCallSingleton& operator=(JSCallSingleton&) {};
public:
  static JSCallSingleton* getInstance() {
    if (!p_instance) {
      p_instance = new JSCallSingleton();
    }
    return p_instance;
  }

  void run(const shared_ptr<JSCall>& this_);

private:
  mutex m_callMutex;
};

class JSCallWithParameters : public JSCall
{
public:
  JSCallWithParameters(const callback_t& callback);
  virtual ~JSCallWithParameters();

  template < class T > void SetParameter(const unsigned n, Local<T>& v) {
    Reserve(n);
    m_argv[n].Clear();
    m_argv[n] = v;
  }

  void SetStringParameter(const unsigned n, const std::string& s);
  virtual bool PrepareParameters(Isolate* isolate, Local<Context>& context);
  virtual void Do(Isolate* isolate, Local<Context>& context);

protected:
  void FillParametersFromStrings(Isolate* isolate);
  void Reserve(const unsigned n);
  void DoClear(void);

protected:
  shared_ptr <Local<Value>[]> m_argv;
  vector<unique_ptr<string>> m_s;
  callback_t m_callback;
  unsigned m_argc = 0 ;
};

#define js_function(name) \
void __##name( const FunctionCallbackInfo< Value > & args, Isolate * isolate, const Local<Context> & context ); \
void name( const FunctionCallbackInfo< Value > & args ) { \
  Isolate * isolate = args.GetIsolate(); \
  HandleScope scope(isolate);\
  const Local<Context> & context( isolate->GetCurrentContext() ); \
  __##name( args, isolate, context ); \
} \
void __##name( const FunctionCallbackInfo< Value > & args, Isolate * isolate, const Local<Context> & context )

#define _js_get_prop( o, T, js_name, name ) \
Local<T> name; \
if ( !o->IsNull() && !o->IsUndefined() && o->IsObject()) { \
    name = Local<T>::Cast( o->Get( context, String::NewFromUtf8( isolate, #js_name ).ToLocalChecked() ).ToLocalChecked() ); \
    if ( name->IsNull() || name->IsUndefined() || !name->Is##T() ) { \
        name.Clear(); \
    } \
} else { \
    name.Clear(); \
}

#define js_get_prop( o, T, name ) _js_get_prop( o, T, name, name )

#define js_arg( n, T, name ) \
Local<T> name; \
if ( args.Length() > n && !args[ n ].IsEmpty() && !args[ n ]->IsNull() && !args[ n ]->IsUndefined() ) { \
    name = Local<T>::Cast( args[ n ] ); \
    if ( name->IsNull() || name->IsUndefined() || !name->Is##T() ) { \
    name.Clear(); \
    } \
} else { \
    name.Clear(); \
}

#define js_function_arg( n, name ) js_arg( n, Function, name )

#define js_callback_arg( n, name ) js_function_arg( n, __nocb_##name ); \
auto name( std::make_shared < Persistent<Function> >( isolate, __nocb_##name ) )

#define js_get_function_prop( o, name ) js_get_prop( o, Function, name )

#define js_get_callback_prop( o, name ) _js_get_prop( o, Function, name, __nocb_##name ) \
auto name( std::make_shared < Persistent<Function> >( isolate, __nocb_##name ) )

// Проверяем 2 варианта: и просто коллбэк, и объект с полем .callback
#define js_callback_arg_Combo( n, name ) \
std::shared_ptr < Persistent<Function> > name;\
js_arg(n, Function, _f##name);\
if(!_f##name.IsEmpty() && _f##name->IsFunction())\
    name = std::make_shared < Persistent<Function> >( isolate, _f##name);\
else {\
    js_arg(n, Object, _o##name);\
    if(!_o##name.IsEmpty() && _o##name->IsObject()){\
        _js_get_prop( _o##name, Function, callback, _cb##name );\
        if(!_cb##name.IsEmpty() && _cb##name->IsFunction()){\
            name = std::make_shared < Persistent<Function> >( isolate, _cb##name);\
        }\
    }\
}\
if(name.get() == nullptr) { _f##name.Clear(); name = std::make_shared < Persistent<Function> >( isolate, _f##name);}


#define js_object_arg( n, name ) js_arg( n, Object, name )
#define js_array_arg( n, name ) js_arg( n, Array, name )
#define js_get_object_prop( o, name ) js_get_prop( o, Object, name )

#define js_to_string( js_name, name, def ) \
std::string name( def ); \
if ( !js_name.IsEmpty() ) { \
    name = *v8::String::Utf8Value( isolate, js_name->ToString( context ).ToLocalChecked() ); \
} 

#define _js_to_int( js_name, name, def, t ) \
t name( ( t )def ); \
if ( !js_name.IsEmpty() ) { \
    name = ( t )js_name->IntegerValue( context ).ToChecked(); \
} 

#define js_to_int( js_name, name, def ) _js_to_int( js_name, name, def, int )
#define js_to_int64( js_name, name, def ) _js_to_int( js_name, name, def, int64_t )

#define js_get_string_prop( o, name, def ) \
_js_get_prop( o, String, name, __js_##name ) \
if ( __js_##name.IsEmpty() ) { \
  _js_get_prop( o, Number, name, __js_n_##name ) \
  if ( !__js_n_##name.IsEmpty() ) { \
    __js_##name = __js_n_##name->ToString( context ).ToLocalChecked(); \
  } \
} \
js_to_string( __js_##name, name, def )

#define js_get_array_prop( o, name ) \
js_get_prop( o, Array, name)

#define js_string_arg( n, name, def ) \
js_arg( n, String, __js_##name ) \
if ( __js_##name.IsEmpty() ) { \
  js_arg( n, Number, __js_n_##name ) \
  if ( !__js_n_##name.IsEmpty() ) { \
    __js_##name = __js_n_##name->ToString( context ).ToLocalChecked(); \
  } \
} \
js_to_string( __js_##name, name, def )

#define _js_get_int_prop( o, name, def, t ) \
_js_get_prop( o, Number, name, __js_##name ) \
if ( __js_##name.IsEmpty() ) { \
  _js_get_prop( o, String, name, __js_s_##name ) \
  if ( !__js_s_##name.IsEmpty() ) { \
    __js_##name = __js_s_##name->ToInteger( context ).ToLocalChecked(); \
  } \
} \
if ( __js_##name.IsEmpty() ) { \
  _js_get_prop( o, Boolean, name, __js_s_##name ) \
  if ( !__js_s_##name.IsEmpty() ) { \
    __js_##name = __js_s_##name->ToInteger( context ).ToLocalChecked(); \
  } \
} \
_js_to_int( __js_##name, name, def, t )

#define _js_int_arg( n, name, def, t ) \
js_arg( n, Number, __js_##name ) \
if ( __js_##name.IsEmpty() ) { \
  js_arg( n, String, __js_s_##name ) \
  if ( !__js_s_##name.IsEmpty() ) { \
    __js_##name = __js_s_##name->ToInteger( context ).ToLocalChecked(); \
  } \
} \
if ( __js_##name.IsEmpty() ) { \
  js_arg( n, Boolean, __js_s_##name ) \
  if ( !__js_s_##name.IsEmpty() ) { \
    __js_##name = __js_s_##name->ToInteger( context ).ToLocalChecked(); \
  } \
} \
_js_to_int( __js_##name, name, def, t )

#define js_get_int_prop( o, name, def ) _js_get_int_prop( o, name, def, int )
#define js_get_int64_prop( o, name, def ) _js_get_int_prop( o, name, def, int64_t )
#define js_int_arg( n, name, def ) _js_int_arg( n, name, def, int )
#define js_int64_arg( n, name, def ) _js_int_arg( n, name, def, int64_t )

#define js_return(val) args.GetReturnValue().Set( val );return
#define js_return_string(s) js_return( String::NewFromUtf8( isolate, s.c_str() ).ToLocalChecked() )

#define js_throw(s) isolate->ThrowException( Exception::TypeError( String::NewFromUtf8( isolate, std::string(s).c_str() ).ToLocalChecked() ) );return

#define js_new_object( name ) Local < Object > name( Object::New( isolate ) )
#define js_set_object_prop( o, key, value ) o->Set( context, String::NewFromUtf8( isolate, std::string( key ).c_str() ).ToLocalChecked(), value )
#define js_set_object_prop_string( o, key, value ) js_set_object_prop( o, key, String::NewFromUtf8( isolate, std::string( value ).c_str() ).ToLocalChecked() )
#define js_set_object_prop_integer( o, key, value ) js_set_object_prop( o, key, Integer::New( isolate, value ) )
#define js_set_object_prop_boolean( o, key, value ) js_set_object_prop( o, key, Boolean::New( isolate, value ) )

#define js_set_object_prop_array_buffer( o, key, p, size ) \
{ \
  Local< ArrayBuffer > ___ab = v8::ArrayBuffer::New( isolate, size ); \
  memcpy( ___ab->GetContents().Data(), p, size ); \
  js_set_object_prop( o, key, ___ab ); \
}

#define js_new_array( name ) Local < Array > name( Array::New( isolate ) )
#define js_set_array_prop( o, index, value ) o->Set( context, index, value )

#define js_set_array_string( o, index, value ) o->Set( context, index, String::NewFromUtf8( isolate, std::string( value ).c_str() ).ToLocalChecked() )

#define js_get_from_array( o, T, index, name ) \
Local<T> name; \
if ( !o->IsNull() && !o->IsUndefined() && o->IsArray() ) { \
    name = Local<T>::Cast( o->Get( context, index).ToLocalChecked() ); \
    if ( name->IsNull() || name->IsUndefined() || !name->Is##T() ) { \
        name.Clear(); \
    } \
} else { \
    name.Clear(); \
}

#define _js_array_buffer_to_string( js_name, name ) \
std::string name; \
if ( !js_name.IsEmpty() ) { \
  name.resize( js_name->ByteLength() ); \
  memcpy( ( char * )name.data(), js_name->GetContents().Data(), js_name->ByteLength() ); \
}

#define js_array_buffer_arg( n, name ) \
js_arg( n, ArrayBuffer, __js_##name ) \
_js_array_buffer_to_string( __js_##name, name );

#define js_get_array_buffer_prop( n, name ) \
_js_get_prop( o, ArrayBuffer, name, __js_##name ) \
_js_array_buffer_to_string( __js_##name, name );

void console_log(const string & s);
void console_error(const string & s);
void alert(const string & s);

#define NODE_BIND_METHOD(o,name) NODE_SET_METHOD(o,#name,name)

} // namespace common
} // namespace TWD
