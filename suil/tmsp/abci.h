//
// Created by dc on 09/11/18.
//

#ifndef SUIL_TMSP_ABCI_H
#define SUIL_TMSP_ABCI_H

#include <suil/logging.h>
#include <suil/result.h>

#include <suil/tmsp/types.pb.h>

namespace suil::tmsp {

  define_log_tag(TMSP);

  enum Codes: int {
    Ok                = 0,
    NotSupported,
    InternalError,
    EncodingError
  };

  using namespace types;

  struct Application: LOGGER(TMSP) {

    virtual void flush() {
      trace("app::flush not implemented");
    }

    virtual void getInfo(ResponseInfo& info) = 0;

    virtual Result setOption(const String& key, const String& value, ResponseSetOption& resp) {
      trace("app::setOption not supported");
      return Result{Codes::NotSupported};
    }

    virtual Result deliverTx(const suil::Data& tx, ResponseDeliverTx& resp) {
      trace("app::deliverTx not implmented");
      return Result{Codes::Ok};
    }

    virtual Result checkTx(const Data& tx, ResponseCheckTx& resp) {
      trace("app::checkTx not implemented");
      return Result{Codes::Ok};
    }

    virtual void commit(ResponseCommit& resp) {
      trace("app::commit not implemented");
    }

    virtual Result query(const RequestQuery& req, ResponseQuery& resp) {
      trace("app::query not implmented");
      return Result{Codes::Ok};
    }

    virtual void initChain(const RequestInitChain& req, ResponseInitChain& resp) {
      trace("app::initChain not implemented");
    }

    virtual void beginBlock(const RequestBeginBlock& req, ResponseBeginBlock& resp) {
      trace("app::beginBlock not implemented");
    }

    virtual void endBlock(const RequestEndBlock& req, ResponseEndBlock& resp) {
      trace("app::endBlock not implemented");
    }
  };
}
#endif // SUIL_TMSP_ABCI_H