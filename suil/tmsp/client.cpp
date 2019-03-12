//
// Created by dc on 10/12/17.
//


#include "client.h"

namespace suil::tmsp {

  Result BaseClient::echo(String&& msg) {
      types::Request req;
      types::Response resp;
      req.mutable_echo()->set_message(msg(), msg.size());
      Result res{};

      process(res, req, resp);
      if (!res.Ok()) {
          // sending echo failed
          res() << "Failed to process echo Request";
      }
      else {
          const auto &ecmsg = resp.echo().message();
          size_t msglen = MIN(ecmsg.size(), msg.size());
          if (strncmp(msg(), ecmsg.c_str(), msglen) != 0) {
              // received message does not match sent message
              res(CodeType::BaseEncodingError)
                      .appendf("Received echo message invalid: %s", ecmsg.c_str());
          }
      }

      return std::move(res);
  }

  Result BaseClient::flush() {
      types::Request  req;
      types::Response resp;
      Result res{};

      req.mutable_flush();

      process(res, req, resp);
      if (!res.Ok()) {
          // sending echo failed
          res() << "Processing flush Request failed";
      }

      return std::move(res);
  }

  Result BaseClient::info(types::ResponseInfo &out) {
      types::Request  req;
      types::Response resp;
      types::ResponseInfo& rsif = *resp.mutable_info();
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

  Result BaseClient::setOption(String &&key, String &&value) {
      types::Request   req;
      types::RequestSetOption& rqopt = *req.mutable_set_option();
      types::Response   resp;
      Result res;

      rqopt.set_key(key(), key.size());
      rqopt.set_value(value(), value.size());

      process(res, req, resp);

      if (!res.Ok()) {
          // sending echo failed
          res() << "Processing setoption Request failed";
      }
      else {
          const auto &rsopt = resp.set_option();
          res((Codes)rsopt.code())
                  << rsopt.log();
      }

      return std::move(res);
  }

  Result BaseClient::deliverTx(const uint8_t *bytes, size_t nsz, types::ResponseDeliverTx &rsdt) {
      types::Request   req;
      types::RequestDeliverTx& rqdt = *req.mutable_deliver_tx();
      types::Response   resp;
      Result res;

      rqdt.set_tx(bytes, nsz);
      process(res, req, resp);

      if (!res.Ok()) {
          res() << "Processing deliverTx Request failed";
      }
      else {
          rsdt = std::move(*resp.mutable_deliver_tx());
          res((Codes)rsdt.code())
                  << rsdt.log();
      }
      return std::move(res);
  }

  Result BaseClient::checkTx(const uint8_t *bytes, size_t nsz, types::ResponseCheckTx& rscx) {
      types::Request   req;
      types::RequestCheckTx&  rqcx = *req.mutable_check_tx();
      types::Response  resp;
      Result res;

      rqcx.set_tx(bytes, nsz);
      process(res, req, resp);

      if (!res.Ok()) {
          res() << "Processing checkTx Request failed";
      }
      else {
          rscx = std::move(*resp.mutable_check_tx());
          res((Codes) rscx.code())
                  << rscx.log();
      }

      return std::move(res);
  }

  Result BaseClient::query(RequestQuery& rq, types::ResponseQuery &rsq) {

      types::Request   req;
      types::Response  resp;
      Result res;

      req.set_allocated_query(&rq);
      process(res, req, resp);

      if (!res.Ok()) {
          res() << "Processing query Request failed";
      }
      else {
          rsq = std::move(*resp.mutable_query());
          res((Codes)rsq.code())
                  << rsq.log();
      }

      req.release_query();
      return std::move(res);
  }

  Result BaseClient::commit(types::ResponseCommit& rscm) {
      types::Request   req;
      types::Response  resp;
      Result res;

      req.mutable_commit();

      process(res, req, resp);
      if (!res.Ok()) {
          res() << "processing query Request failed";
      }
      else {
          rscm = std::move(*resp.mutable_commit());
          res((Codes) rscm.code())
                  << rscm.log();
      }

      return std::move(res);
  }

  Result BaseClient::initChain(types::RequestInitChain& ric) {
      types::Request    req;
      types::Response   resp;
      Result            res;

      req.set_allocated_init_chain(&ric);
      process(res, req, resp);

      if (!res.Ok()) {
          res() << "processing initChain Request failed";
      }

      req.release_init_chain();
      return std::move(res);
  }

  Result BaseClient::beginBlock(types::RequestBeginBlock &rqbb) {
      types::Request    req;
      types::Response   resp;
      Result            res;

      req.set_allocated_begin_block(&rqbb);

      process(res, req, resp);
      if (!res.Ok()) {
          res() << "Processing beginBlock Request failed";
      }

      req.release_begin_block();
      return std::move(res);
  }

  Result BaseClient::endBlock(types::RequestEndBlock &rqeb, types::ResponseEndBlock& rseb) {
      types::Request    req;
      types::Response   resp;
      Result            res;
      req.set_allocated_end_block(&rqeb);

      process(res, req, resp);
      if (!res.Ok()) {
          res() << "Processing endBlock Request failed";
      }

      rseb = std::move(*resp.mutable_end_block());
      req.release_end_block();
      return std::move(res);
  }

  void BaseClient::sendRequest(Result& res, types::Request &req) {
      int rqlen = req.ByteSize();
      OBuffer out((uint32_t)(rqlen+2));
      char *data = out.data();

      if (!req.SerializeToArray(data, rqlen)) {
          // serializing Request failed
          res(Codes::EncodingError)
                  .appendf("serializing Request failed");
          return;
      }


      size_t tsd = 0, total = 0, sent = 0;
      VarInt vit{(size_t) rqlen};
      uint8_t vitlen{vit.length()};

      // send size of message
      sent = adaptor.send(&vitlen, 1, 500);
      if (sent != 1) {
          // failed to size of variable size buffer
          res(Codes::InternalError)
                  .appendf("sending Request header failed: ", errno_s);
          return;
      }

      sent = adaptor.send(&vit.raw()[8-vitlen], vitlen, 500);
      if (sent != vitlen) {
          // failed to send variable size
          res(Codes::InternalError)
                  .appendf("sending Response header failed %lu/%lu: ", sent, vitlen, errno_s);
          return;
      }

      do {
          tsd = MIN(8912, rqlen-total);
          sent = adaptor.send(&data[total], tsd, 1500);
          if (sent != tsd) {
              // sending message failure
              res(Codes::InternalError)
                      .appendf("sending %lu/%lu failed: %s", sent, tsd, errno_s);
              return;
          }
          total += sent;
      } while(total < rqlen);

      // flush socket
      adaptor.flush(1500);
  }

  void BaseClient::receiveResp(Result& res, types::Response &resp) {
      uint8_t lsz{0};
      size_t  size = 1;
      VarInt  len{0};

      if (!adaptor.receive(&lsz, size)) {
          // receiving lsz failed
          res(Codes::InternalError)
                      .appendf("receiving message size failed: %s", errno_s);
          return;
      }

      if (lsz ==0 || lsz > 8) {
          // length will overflow varint buffer
          res(Codes::InternalError)
                  .appendf("received message length (%hhu) overflows varint", lsz);
          return;
      }

      size = lsz;

      if (!adaptor.receive(&len.raw()[8-size], size)) {
          // receiving lsz failed
          res(Codes::InternalError)
                  .appendf("receiving message %u/%u failed: %s", lsz, size, errno_s);
          return;
      }

      size  = len.read<uint64_t>();

      OBuffer rxb(size+2);
      char *data = rxb.data();
      size_t trd = 0, total = 0;
      do {
          trd = MIN(8912, (size_t) (size-total));
          if (!adaptor.read(&data[total], trd, 1000)) {
              res(Codes::InternalError)
                      .appendf("receiving %d bytes failed: %s", trd, errno_s);
              return;
          }
          total += trd;
      } while(total < size);

      if (!resp.ParseFromArray(data, (int)total)) {
          // parse received message failure
          res(Codes::EncodingError)
                  .appendf("parsing received message failed");
      }
  }

  void BaseClient::process(Result& res, types::Request &req, types::Response &resp) {
      sendRequest(res, req);
      if (!res.Ok()) {
          // failed to send Request
          return;
      }

      receiveResp(res, resp);
      if (!res.Ok()) {
          // failed to received Response
          res() << "receiving Response from application failed";
          return;
      }

      if (resp.value_case() == Response::ValueCase::kException) {
          // application returned exception
          res(Codes::InternalError).appendf(
                  "application returned exception: %s", resp.exception().error().c_str());
      }
  }
}