//
// Created by dc on 10/12/17.
//

#include "server.hpp"

namespace suil {
    namespace tdmabci {

        void abci_conn::start() {
            zcstring addr(utils::catstr(ipstr(sock.addr()), ":", sock.port()));
            // enter receive loop
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
                zbuffer rxb((uint32_t)(msglen+2));
                if (!recvmsg(rxb, msglen, addr())) {
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
                           SuilError::getmsg(std::current_exception()));
                    break;
                }

                // reset buffer, this will likely re-use the buffer
                msglen = resp.ByteSize();
                rxb.reset((uint32_t)(msglen+2), true);
                if (!resp.SerializeToArray(rxb.data(), (int) rxb.capacity())) {
                    // serializing Response buffer failed
                    ierror("%s - serializing Response failed", addr());
                    break;
                }
                rxb.seek(msglen);

                if (!sendmsg(rxb, addr())) {
                    // send message failure
                    ierror("%s - sending message failed", addr());
                    break;
                }

                // reset buffer for reuse
                rxb.reset(0, true);
            }
        }

        inline void abci_conn::handle(const types::Request& req, types::Response& resp) {
            switch (req.value_case()) {
                case types::Request::ValueCase::kEcho: {
                    // echo back the message
                    resp.mutable_echo()->set_message(req.echo().message());
                    break;
                }
                case types::Request::ValueCase::kFlush: {
                    // ask application to flush
                    app.flush(*resp.mutable_flush());
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
                    zcstring  key{rqopt.key().data(), rqopt.key().size(), false};
                    zcstring  val{rqopt.value().data(), rqopt.value().size(), false};
                    rsopt.set_code(app.setoption(key, val, rsopt));
                    break;
                }
                case types::Request::ValueCase::kDeliverTx: {
                    // handle DeliverTx
                    const types::RequestDeliverTx &rqdtx = req.deliver_tx();
                    types::ResponseDeliverTx &rsdtx = *resp.mutable_deliver_tx();
                    heapboard hb{(uint8_t*)rqdtx.tx().data(), rqdtx.tx().size()};
                    rsdtx.set_code(app.delivertx(hb, rsdtx));
                    break;
                }
                case types::Request::ValueCase::kCheckTx: {
                    // handle CheckTx
                    const types::RequestCheckTx &rqctx = req.check_tx();
                    types::ResponseCheckTx &rsctx = *resp.mutable_check_tx();
                    heapboard hb{(uint8_t*)rqctx.tx().data(), rqctx.tx().size()};
                    rsctx.set_code(app.checktx(hb, rsctx));
                    break;
                }
                case types::Request::ValueCase::kCommit: {
                    // handle Commit
                    types::ResponseCommit &rssv = *resp.mutable_commit();
                    rssv.set_code(app.commit(rssv));
                    break;
                }
                case types::Request::ValueCase::kQuery: {
                    // handle Query
                    const types::RequestQuery &rqq = req.query();
                    types::ResponseQuery &rsq = *resp.mutable_query();
                    rsq.set_code(app.query(rqq, rsq));
                    break;
                }
                case types::Request::ValueCase::kInitChain: {
                    // handle InitChain
                    const types::RequestInitChain &rqic = req.init_chain();
                    types::ResponseInitChain &rsic = *resp.mutable_init_chain();
                    app.initchain(rqic, rsic);
                    break;
                }
                case types::Request::ValueCase::kBeginBlock: {
                    // handle InitChain
                    const types::RequestBeginBlock &rqbb = req.begin_block();
                    types::ResponseBeginBlock &rsbb = *resp.mutable_begin_block();
                    app.beginblock(rqbb, rsbb);
                    break;
                }
                case types::Request::ValueCase::kEndBlock: {
                    // handle InitChain
                    const types::RequestEndBlock &rqeb = req.end_block();
                    types::ResponseEndBlock &rseb = *resp.mutable_end_block();
                    app.endblock(rqeb, rseb);
                    break;
                }
                default: {
                    serror("unknown message type %d", req.value_case());
                    resp.mutable_exception()->set_error("Unknown Request");
                    break;
                }
            }
        }

        int abci_conn::recvlen(const char *dbg) {
            uint8_t lsz{0};
            size_t  size = 1;
            varint len;

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

        bool abci_conn::recvmsg(zbuffer& rxb, int len, const char *dbg) {
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

        bool abci_conn::sendmsg(zbuffer& rxb, const char *dbg) {
            char *data = rxb.data();
            size_t tsd = 0, total = 0, sent = 0;
            varint vit(rxb.size());
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
    }
}