#include "pch.h"

#include "js.h"

namespace TWD {
namespace common {

using namespace v8;
using namespace std;

JSCallSingleton* JSCallSingleton::p_instance = nullptr;

class JSCallRunner : private boost::noncopyable
{

public:
  JSCallRunner(const shared_ptr < JSCall >& data);
  virtual ~JSCallRunner() = default;

public:
  shared_ptr<JSCall> m_data;
  uv_work_t m_req;
  bool m_bIsFinished;
};

static void JSCallRunner_after(uv_work_t* req, int status)
{
    JSCallRunner* this_ = reinterpret_cast <JSCallRunner*>(req->data);
    if (this_->m_data) {
        this_->m_data->DoRaw();
    }

    this_->m_bIsFinished = true;
    //delete this_;
}

static void JSCallRunner_work(uv_work_t* req)
{
    JSCallRunner* this_ = reinterpret_cast <JSCallRunner*>(req->data);
    if (this_->m_data) {
        this_->m_data->Prepare();
    }
}

JSCallRunner::JSCallRunner(const shared_ptr < JSCall >& data)
    : m_data(data)
{
    m_bIsFinished = false;
    m_req.data = this;
    uv_queue_work(uv_default_loop(), &m_req, JSCallRunner_work, JSCallRunner_after);
}

void JSCall::DoRaw()
{
    Isolate* isolate = Isolate::GetCurrent();
    if (isolate != NULL && !isolate->IsDead() && isolate->InContext()) {
        //Locker isolate_locker( isolate );
        isolate->Enter();
        HandleScope handleScope(isolate);
        Local<Context> context = isolate->GetCurrentContext();
        if (!context.IsEmpty()) {
            Do(isolate, context);
        }
        isolate->Exit();
    }
}

void JSCall::Run(const shared_ptr < JSCall >& this_)
{
    if (this_) {
        new JSCallRunner(this_);
    }
}

JSCallWithParameters::JSCallWithParameters( const callback_t& callback )
    : m_callback{ callback }
{
}

JSCallWithParameters::~JSCallWithParameters()
{
    DoClear();
}

void JSCallWithParameters::DoClear()
{
    for (unsigned i = 0; i < m_argc; i++) {
        m_argv[i].Clear();
    }
}

void JSCallWithParameters::Reserve(const unsigned n)
{
    if (n >= m_argc) {
        unsigned new_argc = n + 1;
        shared_ptr < Local<Value>[] > new_argv(new Local<Value>[new_argc]);
        if (m_argc > 0) {
            for (unsigned i = 0; i < m_argc; i++) {
                new_argv[i] = m_argv[i];
            }
            DoClear();
        }
        m_argc = new_argc;
        m_argv = new_argv;
    }
}

void JSCallWithParameters::SetStringParameter(const unsigned n, const string& s)
{
    while (n >= (int)m_s.size()) {
        m_s.push_back(NULL);
    }
    m_s[n] = make_unique < string >(s);
}

void JSCallWithParameters::FillParametersFromStrings(Isolate* isolate)
{
    for (size_t i = 0; i < m_s.size(); i++) {
        if (m_s[i]) {
            SetParameter(i, String::NewFromUtf8(isolate, m_s[i]->c_str()).ToLocalChecked());
        }
    }
    m_s.clear();
}

void JSCallWithParameters::Do(Isolate* isolate, Local<Context>& context)
{
    if (m_callback && !m_callback->IsEmpty())
    {
        FillParametersFromStrings(isolate);
        if (PrepareParameters(isolate, context))
        {
            FillParametersFromStrings(isolate); // again; may be new strings are added in PrepareParameters
            MaybeLocal<Value> v = m_callback->Get(isolate)->Call(context, Null(isolate), m_argc, m_argv.get());
            (void)v;
        }
    }
}

bool JSCallWithParameters::PrepareParameters(Isolate* isolate, Local<Context>& context)
{
    return true;
}


void JSCallSingleton::run(const shared_ptr<JSCall>& this_)
{
    if (this_)
    {
    m_callMutex.lock();
        JSCallRunner *runner = new JSCallRunner(this_);

        while(!runner->m_bIsFinished) {
            ::SleepEx(1, false);
        }
        delete runner;

        m_callMutex.unlock();
    }
}

class ConsoleLogData : public JSCall
{

protected:
    string m_s;
    bool m_bError;

public:
    ConsoleLogData(const string& s, const bool bError)
        : m_s(s)
        , m_bError(bError) {
    }
    virtual void Do(Isolate* isolate, Local<Context>& context) {
        do {
            Local<Object> global = context->Global();
            if (global.IsEmpty()) break;
            Local<Object> window = Local<Object>::Cast(global->Get(context, String::NewFromUtf8(isolate, "window").ToLocalChecked()).ToLocalChecked());
            if (window.IsEmpty()) break;
            Local<Object> console = Local<Object>::Cast(window->Get(context, String::NewFromUtf8(isolate, "console").ToLocalChecked()).ToLocalChecked());
            if (console.IsEmpty()) break;
            Local<Function> console_log = Local<Function>::Cast(console->Get(context, String::NewFromUtf8(isolate, "log").ToLocalChecked()).ToLocalChecked());
            if (console_log.IsEmpty()) break;
            Local<Function> console_error = Local<Function>::Cast(console->Get(context, String::NewFromUtf8(isolate, "error").ToLocalChecked()).ToLocalChecked());
            if (console_error.IsEmpty()) break;
            Local<Value> args[] = { String::NewFromUtf8(isolate, m_s.c_str()).ToLocalChecked() };
            MaybeLocal<Value> v = (m_bError ? console_error : console_log)->Call(context, console, 1, args);
            (void)v;
        } while (0);
    }
};

static void do_console_log(const string& s, const bool bError)
{
    JSCall::Run(make_shared< ConsoleLogData >("TDM: " + s, bError));
}

void console_log(const string& s)
{
    do_console_log(s, false);
}

void console_error(const string& s)
{
    do_console_log(s, true);
}

class AlertData : public JSCall
{

protected:
    string m_s;

public:
    AlertData(const string& s)
        : m_s(s) {
    }
    virtual void Do(Isolate* isolate, Local<Context>& context) {
        do {
            Local<Object> global = context->Global();
            if (global.IsEmpty()) break;
            Local<Object> window = Local<Object>::Cast(global->Get(context, String::NewFromUtf8(isolate, "window").ToLocalChecked()).ToLocalChecked());
            if (window.IsEmpty()) break;
            Local<Function> alert = Local<Function>::Cast(window->Get(context, String::NewFromUtf8(isolate, "alert").ToLocalChecked()).ToLocalChecked());
            if (alert.IsEmpty()) break;
            Local<Value> args[] = { String::NewFromUtf8(isolate, m_s.c_str()).ToLocalChecked() };
            MaybeLocal<Value> v = alert->Call(context, window, 1, args);
            (void)v;
        } while (0);
    }
};

void alert(const string& s)
{
    JSCall::Run(make_shared< AlertData >("TDM: " + s));
}

} // namespace common
} // namespace TWD