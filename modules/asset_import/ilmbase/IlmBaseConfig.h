//
// Define and set to 1 if the target system has c++11/14 support
// and you want IlmBase to NOT use it's features
//

#undef ILMBASE_FORCE_CXX03

//
// Define and set to 1 if the target system has POSIX thread support
// and you want IlmBase to use it for multithreaded file I/O.
//

#undef HAVE_PTHREAD

//
// Define and set to 1 if the target system supports POSIX semaphores
// and you want OpenEXR to use them; otherwise, OpenEXR will use its
// own semaphore implementation.
//

#undef HAVE_POSIX_SEMAPHORES

#undef HAVE_UCONTEXT_H

//
// Dealing with FPEs
//
#undef ILMBASE_HAVE_CONTROL_REGISTER_SUPPORT

//
// Define and set to 1 if the target system has support for large
// stack sizes.
//

#undef ILMBASE_HAVE_LARGE_STACK

//
// Current (internal) library namepace name and corresponding public
// client namespaces.
//
#undef ILMBASE_INTERNAL_NAMESPACE_CUSTOM
#undef IMATH_INTERNAL_NAMESPACE
#undef IEX_INTERNAL_NAMESPACE
#undef ILMTHREAD_INTERNAL_NAMESPACE

#undef ILMBASE_NAMESPACE_CUSTOM
#undef IMATH_NAMESPACE
#undef IEX_NAMESPACE
#undef ILMTHREAD_NAMESPACE

//
// Define and set to 1 if the target system has support for large
// stack sizes.
//

#undef ILMBASE_HAVE_LARGE_STACK

//
// Version information
//
#undef ILMBASE_VERSION_STRING
#undef ILMBASE_PACKAGE_STRING

#undef ILMBASE_VERSION_MAJOR
#undef ILMBASE_VERSION_MINOR
#undef ILMBASE_VERSION_PATCH

// Version as a single hex number, e.g. 0x01000300 == 1.0.3
#define ILMBASE_VERSION_HEX ((ILMBASE_VERSION_MAJOR << 24) | \
							 (ILMBASE_VERSION_MINOR << 16) | \
							 (ILMBASE_VERSION_PATCH << 8))

#define ILMBASE_INTERNAL_NAMESPACE_CUSTOM 0
#define IMATH_INTERNAL_NAMESPACE Imath
#define IEX_INTERNAL_NAMESPACE Iex
#define ILMTHREAD_INTERNAL_NAMESPACE IlmThread
#define IMATH_NAMESPACE Imath
#define IEX_NAMESPACE Iex
#define ILMTHREAD_NAMESPACE IlmThread