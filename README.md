# suil 
[![build status](http://suilteam.ddns.net:10080/software/suil/badges/master/build.svg)](http://suilteam-gitlab/software/suil/commits/master)

A lightweight mordern `C++` micro service development framework

##### Supported Features

*  Tag based system logger with support for custom log sink
*  Memory pool supporting buffer sizes of up to 8912
*  Integrates libmill's coroutines which is used as the base library for asynchronous calls
*  Dynamic buffer supporting `printf(...)` like functions and stream operator overloads
*  *Zero Copy* strings which can be help speed up operations on strings
*  Integrates IOD framework which is a great tool when it comes to meta programming
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
