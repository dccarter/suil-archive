//
// Created by dc on 10/12/17.
//

#include <suil/varint.h>

#include "server.h"

namespace suil::tmsp {

  int AbciConn::receiveLen(const char *dbg)
  {
    uint8_t  lsz{0};
    size_t   size{1};
    VarInt   len{0};

    if (!sock.receive(&lsz, size)) {
        // receiving lsz failed
        idebug("%s - receiving message size failed: ", dbg, errno_s);
        return -errno;
    }

    if (lsz ==0 || lsz > 8) {
        // length will overflow varint buffer
        idebug("%s - received message length (%hhu) overflows varint", dbg, lsz);
        return 0;
    }

    size = lsz;

    if (!sock.receive(&len.raw()[8-size], size)) {
        // receiving lsz failed
        idebug("%s - receiving message %u/%u failed: %s",
              dbg, lsz, size, errno_s);
        return -errno;
    }

    // cast the read length to an integer
    return len.read<int>();
  }

  bool receiveMsg(OBuffer& rxb, size_t len, const char *dbg)
  {
    char *data = rxb.data();
    size_t trd = 0, total = 0;
    do {
        trd = MIN(8912, (size_t) (len-total));
        if (!sock.read(&data[total], trd, 1000)) {
            idebug("%s - receiving %d bytes failed: %s",
                  dbg, trd, errno_s);
            return false;
        }
        total += trd;
    } while(total < len);
    // seek to the end of the buffer
    rxb.seek(total);
    return true;
  }

  bool AbciConn::sendMsg(const OBuffer& ob, const char* dbg)
  {
    char *data = ob.data();
    size_t tsd = 0, total = 0, sent = 0;
    VarInt vit{ob.size()};
    uint8_t vitlen{vit.length()};

    // send size of message
    sent = sock.send(&vitlen, 1, 500);
    if (sent != 1) {
        // failed to size of variable size buffer
        trace("%s - sending Response header failed: ", dbg, errno_s);
        return false;
    }

    sent = sock.send(&vit.raw()[8-vitlen], vitlen, 500);
    if (sent != vitlen) {
        // failed to send variable size
        trace("%s - sending Response header failed %lu/%lu: ",
              dbg, sent, vitlen, errno_s);
        return false;
    }

    do {
        tsd = MIN(8912, rxb.size()-total);
        sent = sock.send(&data[total], tsd, 1500);
        if (sent != tsd) {
            // sending message failure
            trace("%s - sending %lu/%lu failed: %s",
                  dbg, sent, tsd, errno_s);
            return false;
        }
        total += sent;
    } while(total < rxb.size());

    // flush socket
    sock.flush(1500);

    return true;
  }

  void AbciConn::start()
  {
    zcstring addr(utils::catstr(ipstr(sock.addr()), ":", sock.port()));
    // enter receive loop
    zbuffer rxb;
    while (sock.isopen()) {
        // receive the expected message length
        int msglen = recvlen(addr());
        if (msglen <= 0) {
            // violation abort connect
            if (!msglen != -EPROTO && msglen != -ECONNRESET)
                ierror("%s - invalid Request length", addr());
            break;
        }

        // receive message
        rxb.reserve((size_t)(msglen+2))
        if (!receiveMsg(rxb, (size_t) msglen, addr())) {
            // receiving message  fail, continue
            break;
        }

        types::Request  req;
        types::Response resp;

        if (!req.ParseFromArray(rxb.data(), msglen)) {
            // parsing received buffer failed
            ierror("%s - invalid Request data", addr());
            break;
        }

        try {
            handle(req, resp);
        }
        catch (...) {
            // log unhandled errors and abort
            ierror("un handled application error: %s",
                    Exception::fromCurrent().what());
            break;
        }

        // reset buffer, this will likely re-use the buffer
        msglen = resp.ByteSize();
        rxb.reset((size_t)(msglen+2), true);
        if (!resp.SerializeToArray(rxb.data(), (int) rxb.capacity())) {
            // serializing Response buffer failed
            ierror("%s - serializing Response failed", addr());
            break;
        }
        rxb.seek(msglen);

        if (!sendMsg(rxb, addr())) {
            // send message failure
            ierror("%s - sending message failed", addr());
            break;
        }

        // reset buffer for reuse
        rxb.reset(0, true);
    }
  }

  void AbciConn::handle(const types::Request& req, types::Response& resp) {
    switch (req.value_case()) {
        case types::Request::ValueCase::kEcho: {
            // echo back the message
            resp.mutable_echo()->set_message(req.echo().message());
            break;
        }
        case types::Request::ValueCase::kFlush: {
            // ask application to flush
            app.flush();
            break;
        }
        case types::Request::ValueCase::kInfo: {
            // ask information from application
            app.getinfo(*resp.mutable_info());
            break;
        }
        case types::Request::ValueCase::kSetOption: {
            // pass Request to application
            types::ResponseSetOption &rsopt = *resp.mutable_set_option();
            const types::RequestSetOption &rqopt = req.set_option();
            String  key{rqopt.key().data(), rqopt.key().size(), false};
            String  val{rqopt.value().data(), rqopt.value().size(), false};
            auto res = app.setOption(key, val, rsopt);
            rsopt.set_code(res.Code);
            if (!res.Ok())
              rsopt.set_log(res.data(), res.size());
            break;
        }
        case types::Request::ValueCase::kDeliverTx: {
            // handle DeliverTx
            const types::RequestDeliverTx &rqdtx = req.deliver_tx();
            types::ResponseDeliverTx &rsdtx = *resp.mutable_deliver_tx();
            Data tx{(uint8_t*)rqdtx.tx().data(), rqdtx.tx().size()};
            auto res = app.deliverTx(tx, rsdtx);
            rsdtx.set_code(res.Code);
            if (!res.Ok())
              rsdtx.set_log(res.data(), res.size());
            break;
        }
        case types::Request::ValueCase::kCheckTx: {
            // handle CheckTx
            const types::RequestCheckTx &rqctx = req.check_tx();
            types::ResponseCheckTx &rsctx = *resp.mutable_check_tx();
            Data tx{(uint8_t*)rqdtx.tx().data(), rqdtx.tx().size()};
            auto res = app.checkTx(tx, rsctx);
            rsctx.set_code(res.Code);
            if (!res.Ok())
              rsctx.set_log(res.data(), res.size());
            break;
        }
        case types::Request::ValueCase::kCommit: {
            // handle Commit
            app.commit(*resp.mutable_commit());
            break;
        }
        case types::Request::ValueCase::kQuery: {
            // handle Query
            const types::RequestQuery &rqq = req.query();
            types::ResponseQuery &rsq = *resp.mutable_query();
            auto res = app.query(rqq, rsq);
            rsq.set_code(res.Code);
            if (!res.Ok())
              rsq.set_log(res.data(), res.size());
            break;
        }
        case types::Request::ValueCase::kInitChain: {
            // handle InitChain
            const types::RequestInitChain &rqic = req.init_chain();
            types::ResponseInitChain &rsic = *resp.mutable_init_chain();
            app.initChain(rqic, rsic);
            break;
        }
        case types::Request::ValueCase::kBeginBlock: {
            // handle InitChain
            const types::RequestBeginBlock &rqbb = req.begin_block();
            types::ResponseBeginBlock &rsbb = *resp.mutable_begin_block();
            app.beginBlock(rqbb, rsbb);
            break;
        }
        case types::Request::ValueCase::kEndBlock: {
            // handle InitChain
            const types::RequestEndBlock &rqeb = req.end_block();
            types::ResponseEndBlock &rseb = *resp.mutable_end_block();
            app.endBlock(rqeb, rseb);
            break;
        }
        default: {
            serror("unknown message type %d", req.value_case());
            resp.mutable_exception()->set_error("Unknown Request");
            break;
        }
    }
}
}