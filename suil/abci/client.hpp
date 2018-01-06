//
// Created by dc on 10/12/17.
//

#ifndef SUIL_CLIENT_HPP
#define SUIL_CLIENT_HPP

#include <suil/abci/abci.hpp>
#include <suil/wire.hpp>

namespace suil {

    namespace tdmabci {

        using namespace types;

        define_log_tag(TDM_ABCICLI);

        struct base_client : LOGGER(dtag(TDM_ABCICLI)) {
            Result echo(zcstring&& msg);
            Result flush();

            Result info(ResponseInfo& rinfo);

            Result setoption(zcstring&& key, zcstring&& value);

            template <size_t N>
            inline Result delivertx(blob_t<N> data, ResponseDeliverTx& rsdt) {
                return delivertx(&data[0], data.size(), rsdt);
            }

            template <size_t N>
            inline Result checktx(blob_t<N> data, ResponseCheckTx& rscx) {
                return checktx(&data[0], data.size(), rscx);
            }

            inline Result delivertx(const breadboard& data, ResponseDeliverTx& rsdt) {
                auto  raw = data.raw();
                return delivertx(raw.first, raw.second, rsdt);
            }

            inline Result checktx(const breadboard& data, ResponseCheckTx& rscx) {
                auto  raw = data.raw();
                return checktx(raw.first, raw.second, rscx);
            }

            Result query(RequestQuery& , ResponseQuery& rsq);

            Result commit(ResponseCommit& rc);

            Result initchain(RequestInitChain& ric);

            Result beginblock(RequestBeginBlock& rbb);

            Result endblock(RequestEndBlock& rqeb, ResponseEndBlock& rseb);

        protected:

            base_client(SocketAdaptor& adaptor)
                : adaptor(adaptor)
            {}

        private:
            Result delivertx(const uint8_t bytes[], size_t nsz, ResponseDeliverTx& rsdt);
            Result checktx(const uint8_t bytes[],   size_t nsz, ResponseCheckTx& rscx);

            void process(Result& res,  types::Request &req, Response &resp);
            void sendreq(Result& res,  types::Request& req);
            void recvresp(Result& res, types::Response& resp);
            SocketAdaptor& adaptor;
        };

        template <typename Proto = TcpSock>
        struct client : base_client {
            client(zcstring&& host, int port)
                : base_client(sock),
                  appaddr(ipremote(host(), port, 0, -1))
            {}

            Result connect() {
                trace("connecting application at %s", ipstr(appaddr));
                Result res;
                if (!sock.connect(appaddr, 3000)) {
                    // Connection to application failed
                    res(CodeType::InternalError)
                        .appendf("connecting to application failed: %s", errno_s);
                    return std::move(res);
                }

                ResponseInfo rsif;
                res = std::move(Ego.info(rsif));

                if (res.Ok()) {
                    inotice("connected to application %s, version: %s",
                            ipstr(appaddr), rsif.version().c_str());
                }

                return std::move(res);
            }

            void disconnect() {
                trace("disconnecting client...");
                // close socket will disconnect the client
                sock.close();
            }

            ~client() {
                disconnect();
            }
        private:
            Proto  sock;
            ipaddr appaddr{nullptr};
        };
    }
}
#endif //SUIL_CLIENT_HPP
