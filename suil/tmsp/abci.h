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

  struct Application: LOGGER(TMSP) {

    virtual void flush() {
      trace("app::flush not implemented");
    }

    virtual void getInfo(types::ResponseInfo& info) = 0;

    virtual Result setOption(const String& key, const String& value, types::ResponseSetOption& resp) {
      trace("app::setOption not supported");
      return Result{Codes::NotSupported};
    }

    virtual Result deliverTx(const suil::Data& tx, types::ResponseDeliverTx& resp) {
      trace("app::deliverTx not implmented");
      return Result{Codes::Ok};
    }

    virtual Result checkTx(const Data& tx, types::ResponseCheckTx& resp) {
      trace("app::checkTx not implemented");
      return Result{Codes::Ok};
    }

    virtual void commit(types::ResponseCommit& resp) {
      trace("app::commit not implemented");
    }

    virtual Result query(const types::RequestQuery& req, types::ResponseQuery& resp) {
      trace("app::query not implmented");
      return Result{Codes::Ok};
    }

    virtual void initChain(const types::RequestInitChain& req, types::ResponseInitChain& resp) {
      trace("app::initChain not implemented");
    }

    virtual void beginBlock(const types::RequestBeginBlock& req, types::ResponseBeginBlock& resp) {
      trace("app::beginBlock not implemented");
    }

    virtual void endBlock(const types::RequestEndBlock& req, types::ResponseEndBlock& resp) {
      trace("app::endBlock not implemented");
    }
  };
}
#endif // SUIL_TMSP_ABCI_H