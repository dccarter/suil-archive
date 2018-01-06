//
// Created by dc on 10/12/17.
//
#include <suil/abci/client.hpp>

namespace suil {
    namespace tdmabci {

        Result base_client::echo(zcstring&& msg) {
            types::Request req;
            Response resp;
            req.mutable_echo()->set_message(msg(), msg.len);
            Result res;

            process(res, req, resp);
            if (!res.Ok()) {
                // sending echo failed
                res() << "Failed to process echo Request";
            }
            else {
                const std::string &ecmsg = resp.echo().message();
                size_t msglen = MIN(ecmsg.size(), msg.len);
                if (strncmp(msg(), ecmsg.c_str(), msglen) != 0) {
                    // received message does not match sent message
                    res(CodeType::BaseEncodingError)
                            .appendf("Received echo message invalid: %s", ecmsg.c_str());
                }
            }

            return std::move(res);
        }

        Result base_client::flush() {
            types::Request  req;
            Response resp;
            Result res;

            req.mutable_flush();

            process(res, req, resp);
            if (!res.Ok()) {
                // sending echo failed
                res() << "Processing flush Request failed";
            }
        }

        Result base_client::info(ResponseInfo &out) {
            types::Request  req;
            types::Response resp;
            ResponseInfo& rsif = *resp.mutable_info();
            Result res;
            req.mutable_info();

            process(res, req, resp);

            if (!res.Ok()) {
                // sending echo failed
                res() << "Processing info Request failed";
            }

            out = std::move(rsif);
            return std::move(res);
        }

        Result base_client::setoption(zcstring &&key, zcstring &&value) {
            types::Request   req;
            RequestSetOption& rqopt = *req.mutable_set_option();
            types::Response   resp;
            Result res;

            rqopt.set_key(key(), key.len);
            rqopt.set_value(value(), value.len);

            process(res, req, resp);

            if (!res.Ok()) {
                // sending echo failed
                res() << "Processing setoption Request failed";
            }
            else {
                const ResponseSetOption &rsopt = resp.set_option();
                res((CodeType)rsopt.code())
                        << rsopt.log();
            }

            return std::move(res);
        }

        Result base_client::delivertx(const uint8_t *bytes, size_t nsz, ResponseDeliverTx &rsdt) {
            types::Request   req;
            RequestDeliverTx& rqdt = *req.mutable_deliver_tx();
            types::Response   resp;
            Result res;

            rqdt.set_tx(bytes, nsz);
            process(res, req, resp);

            if (!res.Ok()) {
                // sending echo failed
                res() << "Processing delivertx Request failed";
            }
            else {
                rsdt = std::move(*resp.mutable_deliver_tx());
                res((CodeType)rsdt.code())
                        << rsdt.log();
            }
            return std::move(res);
        }

        Result base_client::checktx(const uint8_t *bytes, size_t nsz, ResponseCheckTx& rscx) {
            types::Request   req;
            RequestCheckTx&  rqcx = *req.mutable_check_tx();
            types::Response  resp;
            Result res;

            rqcx.set_tx(bytes, nsz);
            process(res, req, resp);

            if (!res.Ok()) {
                // sending echo failed
                res() << "Processing delivertx Request failed";
            }
            else {
                rscx = std::move(*resp.mutable_check_tx());
                res((CodeType) rscx.code())
                        << rscx.log();
            }

            return std::move(res);
        }

        Result base_client::query(RequestQuery& rq, ResponseQuery &rsq) {

            types::Request   req;
            types::Response  resp;
            Result res;

            req.set_allocated_query(&rq);
            process(res, req, resp);

            if (!res.Ok()) {
                // sending echo failed
                res() << "Processing query Request failed";
            }
            else {
                rsq = std::move(*resp.mutable_query());
                res((CodeType)rsq.code())
                        << rsq.log();
            }

            req.release_query();
            return std::move(res);
        }

        Result base_client::commit(ResponseCommit& rscm) {
            types::Request   req;
            types::Response  resp;
            Result res;

            req.mutable_commit();

            process(res, req, resp);
            if (!res.Ok()) {
                // sending echo failed
                res() << "processing query Request failed";
            }
            else {
                rscm = std::move(*resp.mutable_commit());
                res((CodeType) rscm.code())
                        << rscm.log();
            }

            return std::move(res);
        }

        Result base_client::initchain(RequestInitChain& ric) {
            types::Request    req;
            types::Response   resp;
            Result            res;

            req.set_allocated_init_chain(&ric);
            process(res, req, resp);

            if (!res.Ok()) {
                // sending echo failed
                res() << "processing initchain Request failed";
            }

            req.release_init_chain();
            return std::move(res);
        }

        Result base_client::beginblock(RequestBeginBlock &rqbb) {
            types::Request    req;
            types::Response   resp;
            Result            res;

            req.set_allocated_begin_block(&rqbb);

            process(res, req, resp);
            if (!res.Ok()) {
                // sending echo failed
                res() << "Processing beginblock Request failed";
            }

            req.release_begin_block();
            return std::move(res);
        }

        Result base_client::endblock(RequestEndBlock &rqeb, ResponseEndBlock& rseb) {
            types::Request    req;
            types::Response   resp;
            Result            res;
            req.set_allocated_end_block(&rqeb);

            process(res, req, resp);
            if (!res.Ok()) {
                // sending echo failed
                res() << "Processing endblock Request failed";
            }

            rseb = std::move(*resp.mutable_end_block());
            req.release_end_block();
            return std::move(res);
        }

        void base_client::sendreq(Result& res, types::Request &req) {
            int rqlen = req.ByteSize();
            zbuffer out((uint32_t)(rqlen+2));
            char *data = out.data();

            if (!req.SerializeToArray(data, rqlen)) {
                // serializing Request failed
                res(CodeType::BaseEncodingError)
                        .appendf("serializing Request failed");
                return;
            }


            size_t tsd = 0, total = 0, sent = 0;
            varint vit((uint64_t) rqlen);
            uint8_t vitlen{vit.length()};

            // send size of message
            sent = adaptor.send(&vitlen, 1, 500);
            if (sent != 1) {
                // failed to size of variable size buffer
                res(CodeType::InternalError)
                        .appendf("sending Request header failed: ", errno_s);
                return;
            }

            sent = adaptor.send(&vit.raw()[8-vitlen], vitlen, 500);
            if (sent != vitlen) {
                // failed to send variable size
                res(CodeType::InternalError)
                        .appendf("sending Response header failed %lu/%lu: ", sent, vitlen, errno_s);
                return;
            }

            do {
                tsd = MIN(8912, rqlen-total);
                sent = adaptor.send(&data[total], tsd, 1500);
                if (sent != tsd) {
                    // sending message failure
                    res(CodeType::InternalError)
                            .appendf("sending %lu/%lu failed: %s", sent, tsd, errno_s);
                    return;
                }
                total += sent;
            } while(total < rqlen);

            // flush socket
            adaptor.flush(1500);
        }

        void base_client::recvresp(Result& res, types::Response &resp) {
            uint8_t lsz{0};
            size_t  size = 1;
            varint len;

            if (!adaptor.receive(&lsz, size)) {
                // receiving lsz failed
                res(CodeType::InternalError)
                           .appendf("receiving message size failed: ", errno_s);
                return;
            }

            if (lsz ==0 || lsz > 8) {
                // length will overflow varint buffer
                res(CodeType::InternalError)
                        .appendf("received message length (%hhu) overflows varint", lsz);
                return;
            }

            size = lsz;

            if (!adaptor.receive(&len.raw()[8-size], size)) {
                // receiving lsz failed
                res(CodeType::InternalError)
                        .appendf("receiving message %u/%u failed: %s", lsz, size, errno_s);
                return;
            }

            size  = len.read<uint64_t>();

            zbuffer rxb(size+2);
            char *data = rxb.data();
            size_t trd = 0, total = 0;
            do {
                trd = MIN(8912, (size_t) (size-total));
                if (!adaptor.read(&data[total], trd, 1000)) {
                    res(CodeType::InternalError)
                            .appendf("receiving %d bytes failed: %s", trd, errno_s);
                    return;
                }
                total += trd;
            } while(total < size);

            if (!resp.ParseFromArray(data, (int)total)) {
                // parse received message failure
                res(CodeType::EncodingError)
                        .appendf("parsing received message failed");
            }
        }

        void base_client::process(Result& res, types::Request &req, Response &resp) {
            sendreq(res, req);
            if (!res.Ok()) {
                // failed to send Request
                return;
            }

            recvresp(res, resp);
            if (!res.Ok()) {
                // failed to received Response
                res() << "receiving Response from application failed";
                return;
            }

            if (resp.value_case() == Response::ValueCase::kException) {
                // application returned exception
                res(CodeType::InternalError).appendf(
                        "application returned exception: %s", resp.exception().error().c_str());
            }
        }
    }
}

