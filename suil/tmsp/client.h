//
// Created by dc on 10/12/17.
//

#ifndef SUIL_TMSP_CLIENT_H
#define SUIL_TMSP_CLIENT_H

#include <suil/net.h>
#include <suil/tmsp/abci.h>

namespace suil::tmsp {

  struct BaseClient: LOGGER(TMSP) {

  protected:
    BaseClient(SocketAdaptor& sock)
      : sock(sock)
    {}

    Result echo(String&& msg);

    Result flush();

    Result info(types::ResponseInfo& rinfo);

    Result setOption(String&& key, String&& value);

    inline Result deliverTx(const Data& data, types::ResponseDeliverTx& rsdt) {
        return deliverTx(data.data(), data.size(), rsdt);
    }

    inline Result checkTx(const Data& data, types::ResponseCheckTx& rscx) {
        return checkTx(data.data(), data.size(), rscx);
    }

    Result query(types::RequestQuery& , types::ResponseQuery& rsq);

    Result commit(types::ResponseCommit& rc);

    Result initChain(types::RequestInitChain& ric);

    Result beginBlock(types::RequestBeginBlock& rbb);

    Result endBlock(types::RequestEndBlock& rqeb, types::ResponseEndBlock& rseb);

  private:
    Result deliverTx(const uint8_t bytes[], size_t nsz, types::ResponseDeliverTx& rsdt);
    Result checkTx(const uint8_t bytes[],   size_t nsz, types::ResponseCheckTx& rscx);

    void process(Result& res,  types::Request &req, types::Response &resp);
    void sendRequest(Result& res,  types::Request& req);
    void receiveResp(Result& res, types::Response& resp);


  private:
    SocketAdaptor& adaptor;
  };

  template <typename Proto = TcpSock>
  struct Client : BaseClient {
      Client(String&& host, int port)
          : BaseClient(sock),
            appaddr(ipremote(host(), port, 0, -1))
      {}

      Result connect() {
          trace("connecting application at %s", ipstr(appaddr));
          Result res;
          if (!sock.connect(appaddr, 3000)) {
              // Connection to application failed
              res(Codes::InternalError)
                  .appendf("connecting to application failed: %s", errno_s);
              return std::move(res);
          }

          types::ResponseInfo rsif;
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

      ~Client() {
          disconnect();
      }
  private:
      Proto  sock;
      ipaddr appaddr{nullptr};
  };
}

#endif // SUIL_TMSP_CLIENT_H