Node.js native module parts.

- [CJSComm.cpp](CJSComm.cpp), [CJSComm.h](CJSComm.h)

  An implementation of C++ device communication/transport layer proxying commands to the JS implementation.
  
  Historically, all device interaction logic code is in C++. But we would like to start coding communication/transport layer variants using Node.js.
  
- [js.cpp](js.cpp), [js.h](js.h)

  Helpers for marshalling function calls and callbacks between C++ and V8 JS engine (in the nw.js environment).
