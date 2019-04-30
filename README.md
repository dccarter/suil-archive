# suil
A lightweight mordern `C++` micro service development framework
##### [See Suil Examples](https://github.com/dccarter/suil-examples) for examples

##### Supported Features

*  Tag based system logger with support for custom log m_sink
*  Memory pool supporting buffer sizes of up to 8912
*  Integrates [libmill's](https://github.com/sustrik/libmill) coroutines which is used as the base library for asynchronous calls
*  Dynamic buffer supporting `printf(...)` like functions and stream operator overloads
*  *Zero Copy* strings which can be help speed up operations on strings
*  Integrates [IOD](https://github.com/matt-42/iod) framework which is a great tool when it comes to meta programming
*  Sockets (minimal SSL support) API using coroutines
*  HTTP/1.1 server
   *  Support for static and dynamic route handling
   *  Static compilation of static routes
   *  Support for Middlewares registered at compilation time via generics
      *  SQL/Database middleware included as a template
   *  File server support
      *  Supported mime types can be extended and customized
      *  Smaller files are cached in memory, larger files are memory mapped
      *  Support for `sendfile` system call if enabled.
      *  HTTP cache headers supported
      *  Range requests parsing supported
   *  Route attributes
   *  Response content type decoded from type of response if returned
   *  Request form parser, multi-part & url-encoded forms supported
   *  Disk offload for large body (Partially supported or not tested)
*  HTTP/1.1 Client
   *  Session based HTTP client
   *  Easy to use API
   *  Customizable response readers
   *  SSL support
*  Web Socket server support (Version 13) through HTTP upgrade.
*  Database support
   *  Generic SQL ORM 
   *  SQLite3 driver
   *  PGSQL client
      *  Asynchronous or non-blocking in a coroutine way
      *  Prepared statements
      *  Cached connections
*  JWT creation and decoding
   *  Simple authentication middleware issuing web tokens (Not tested)

*  `JSON RPC 2.0` Server and Client
   *  Support for a JSON RPC server and client enabled
   *  Easy to use JSON RPC API, the API is declared in suil's own custom scc` formart and both the service and client are generated from the `scc`

*  Support for custom code format (`scc`)
   *  scc` is used to declare data and service types with syntax resembling that of `C++`
   *  `scc` source's are transpiled to `c++` source's which can then be used in project's
   *  `scc` can be used to generate reflectable type's (for IOD), suil/JSON RPC server/client's see [scc source](https://github.com/dccarter/suil-examples/blob/master/rpc/rpc.scc), [implementing service](https://github.com/dccarter/suil-examples/blob/master/rpc/src/calculator.cpp) and [using service/client](https://github.com/dccarter/suil-examples/blob/master/rpc/src/main.cc)
   *  `cmake` macro to generate C++ source's from `scc` sources [see rpc example](https://github.com/dccarter/suil-examples/blob/master/rpc/CMakeLists.txt)

##### Build Requirements or Development Dependencies on Ubuntu
When building or developing an application that uses this project on ubuntu, the
following libraries should be installed.

* SQLite
  `sudo apt-get install sqlite3 libsqlite3-dev`
* PostgresSQL
  `sudo apt-get install libpq-dev postgresql postgresql-server-dev-all`
* UUID
  `sudo apt-get install uuid-dev`
* OpenSSL
  `sudo apt-get install openssl libssl-dev`
