//
// Created by dc on 09/12/17.
//

#ifndef SUIL_SERVER_HPP
#define SUIL_SERVER_HPP

#include <suil/net.hpp>
#include <suil/abci/abci.pb.h>
#include <suil/wire.hpp>

namespace suil {

    using namespace types;

    define_log_tag(TDM_ABCISRV);

    // ethermint abci
    namespace tdmabci {

        struct appication: LOGGER(dtag(TDM_ABCISRV)) {
            virtual void getinfo(ResponseInfo& rsinfo)  = 0;

            virtual void flush(ResponseFlush& rsflush) {
                trace("app::flush not supported");
            }

            virtual CodeType setoption(const zcstring& key, const zcstring& value, ResponseSetOption& rsopt) {
                trace("app::setoption not supported");
                return CodeType::Unsupported;
            }

            virtual CodeType delivertx(suil::heapboard& tx, ResponseDeliverTx& rsdtx) {
                trace("app::delivertx not supported");
                return CodeType::OK;
            }

            virtual CodeType checktx(suil::heapboard& tx, ResponseCheckTx& rsctx) {
                trace("app::checktx not supported");
                return CodeType::OK;
            }

            virtual CodeType commit(ResponseCommit& rscm) {
                trace("app::commit not supported");
                return CodeType::OK;
            }

            virtual CodeType query(const RequestQuery& rqq, ResponseQuery& rsq) {
                trace("app::query not supported");
                return CodeType::OK;
            }

            virtual CodeType initchain(const RequestInitChain& rqic, ResponseInitChain& rsic) {
                trace("app::initchain not supported");
                return CodeType::OK;
            }

            virtual CodeType beginblock(const RequestBeginBlock& rqbb, ResponseBeginBlock& rsbb) {
                trace("app::beginblock not supported");
                return CodeType::OK;
            }

            virtual CodeType endblock(const RequestEndBlock& rqeb, ResponseEndBlock& rseb) {
                trace("app::endblock not supported");
                return CodeType::OK;
            }
        };

        struct abci_conn : LOGGER(dtag(TDM_ABCISRV)) {

            abci_conn(appication& app, SocketAdaptor& sock)
                : app(app),
                  sock(sock)
            {}

            void start();

            ~abci_conn() {
                // flush the socket
                sock.flush();
            }

        private:

            inline void handle(const types::Request&, types::Response&);

            int  recvlen(const char *dbg);

            bool recvmsg(zbuffer& rxb, int len, const char *dbg);

            bool sendmsg(zbuffer& rxb, const char *dbg);

            suil::SocketAdaptor& sock;
            appication&         app;
        };

        template <typename Backend = suil::TcpSs>
        struct abci_server : LOGGER(dtag(TDM_ABCISRV)) {

            typedef abci_server<Backend> __server_t;

            struct abci_handler {
                void operator()(SocketAdaptor& sock, __server_t *s) {
                    ldebug(s, "handling abci Connection %s:%d",
                                    ipstr(sock.addr()), sock.port());

                    abci_conn conn(s->getapp(), sock);
                    conn.start();

                    ldebug(s, "done handling abci Connection %s:%d",
                           ipstr(sock.addr()), sock.port());
                }
            };

            template<typename... __Opts>
            abci_server(appication &app, __Opts... opts)
                : backend(config, config, this),
                  app(app)
            {
                utils::apply_config(config, opts...);
            }

            appication& getapp() {
                return app;
            }

            int start() {
                // start server
                iinfo("abci server starting %s:%d", config.name.c_str(), config.port);

                int status = EXIT_FAILURE;

                try {
                    status = backend.run();
                }
                catch (SuilError& ser) {
                    ierror("critical abci server error: %s", ser.what());
                }
                catch (...) {
                    ierror("critical error: %s",
                          SuilError::getmsg(std::current_exception()));
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
            ServerConfig       config;
        };

        using sslabci = abci_server<SslSs>;
        using rawabci = abci_server<TcpSs>;
    }
}
#endif //SUIL_SERVER_HPP
