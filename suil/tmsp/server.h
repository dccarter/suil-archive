//
// Created by dc on 09/11/18.
//

#ifndef SUIL_TMSP_SERVER_H
#define SUIL_TMSP_SERVER_H

#include <suil/net.h>
#include <suil/tmsp/abci.h>

namespace suil::tmsp {

  struct AbciConn : LOGGER(TSMP) {
    AbciConn(Application& app, SocketAdaptor& sock)
      : app(app),
        sock(sock)
    {}

    void start();

    ~AbciConn();

  private:
    void handle(const types::Request& req, types::Response& resp);

    int  receiveLen(const char *dbg);

    bool receiveMsg(OBuffer& rxb, size_t len, const char *dbg);

    bool sendMsg(const OBuffer& ob, const char* dbg);

  private:
    Application&    app;
    SocketAdaptor&  sock;
  };

  template <typename Backend = suil::TcpSs>
  struct AbciServer : LOGGER(TSMP) {

      typedef abci_server<Backend> __server_t;

      struct abci_handler {
          void operator()(SocketAdaptor& sock, __server_t *s) {
              ldebug(s, "handling abci Connection %s:%d",
                              ipstr(sock.addr()), sock.port());

              AbciConn conn(s->getapp(), sock);
              conn.start();

              ldebug(s, "done handling abci Connection %s:%d",
                      ipstr(sock.addr()), sock.port());
          }
      };

      template<typename... __Opts>
      AbciServer(Application &app, __Opts... opts)
          : backend(config, config, this),
            app(app)
      {
          utils::apply_config(config, opts...);
      }

      Application& getapp() {
          return app;
      }

      int start() {
          // start server
          iinfo("abci server starting %s:%d", config.name.c_str(), config.port);

          int status = EXIT_FAILURE;

          try {
              status = backend.run();
          }
          catch (Exception& ser) {
              ierror("critical abci server error: %s", ser.what());
          }
          catch (...) {
              ierror("critical error: %s",
                    Exception::fromCurrent().what());
          }

          iinfo("abci server exiting with status %d", status);
          return status;
      }

      void stop() {
          // stop server
          iinfo("abci server terminating!");
          backend.stop();
      }

  private:

      void init() {
          // initialize backend
          backend.init();
      }

      typedef server<abci_handler, Backend, __server_t> abci_backend_t;
      abci_backend_t      backend;
      appication&         app;
      ServerConfig        config;
  };

  using AbciSslServer = AbciServer<SslSs>;
  using AbciTcpServer = AbciServer<TcpSs>;
}

#endif  // SUIL_TMSP_SERVER_H