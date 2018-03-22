//
// Created by dc on 11/12/17.
//
#include "redis.hpp"

namespace suil {
    namespace redis {

        transaction::transaction(base_client &client)
            : client(client)
        {}

        Response& transaction::exec() {
            // send MULTI command
            if (!in_multi) {
                iwarn("EXEC not support outside of MULTI transaction");
                return cachedresp;
            }

            // send EXEC command
            cachedresp = client.send("EXEC");
            if (!cachedresp) {
                // there is an error on the reply
                ierror("error sending 'EXEC' command: %s",cachedresp.error());
            }

            in_multi = false;
            return cachedresp;
        }

        bool transaction::discard()
        {
            if (!in_multi) {
                iwarn("DISCARD not supported outside MULTI");
                return false;
            }

            // send EXEC command
            cachedresp = client.send("DISCARD");
            if (!cachedresp) {
                // there is an error on the reply
                ierror("error sending 'DISCARD' command: %s", cachedresp.error());
            }

            in_multi = false;
            return cachedresp;
        }

        bool transaction::multi()
        {
            if (in_multi) {
                iwarn("MULTI not supported within a MULTI block");
                return false;
            }

            // send EXEC command
            cachedresp = client.send("MULTI");
            if (!cachedresp) {
                // there is an error on the reply
                ierror("error sending 'MULTI' command: %s", cachedresp.error());
            }

            in_multi = true;
            return cachedresp;
        }

        transaction::~transaction() {
            if (in_multi)
                discard();
        }

        Response base_client::dosend(Commmand &cmd, size_t nrps) {
            // send the command to the server
            zcstring data = cmd.prepared();
            size_t size = adaptor.send(data.data(), data.size(), config.timeout);
            if (size != data.size()) {
                // sending failed somehow
                return Response{Reply('-', utils::catstr("sending failed: ", errno_s))};
            }
            adaptor.flush(config.timeout);

            Response resp;
            std::vector<Reply> stage;
            do {
                if (!recvresp(resp.buffer, stage)) {
                    // receiving data failed
                    return Response{Reply('-',
                                          utils::catstr("receiving Response failed: ", errno_s))};
                }
            } while (--nrps > 0);

            resp.entries = std::move(stage);
            return std::move(resp);
        }

        zcstring base_client::commit(Response &resp) {
            // send all the commands at once and read all the responses in one go
            Commmand *last = batched.back();
            batched.pop_back();

            for (auto& cmd: batched) {
                zcstring data = cmd->prepared();
                size_t size = adaptor.send(data.data(), data.size(), config.timeout);
                if (size != data.size()) {
                    // sending command failure
                    return utils::catstr("sending '", (*cmd)(), "' failed: ", errno_s);
                }
            }

            resp =  dosend(*last, batched.size()+1);
            if (!resp) {
                // return error message
                return resp.error();
            }
            return zcstring{nullptr};
        }

        bool base_client::recvresp(zbuffer& out, std::vector<Reply>& stagging) {
            size_t rxd{1}, offset{out.size()};
            char prefix;
            if (!adaptor.read(&prefix, rxd, config.timeout)) {
                return false;
            }

            switch (prefix) {
                case SUIL_REDIS_PREFIX_ERROR:
                case SUIL_REDIS_PREFIX_INTEGER:
                case SUIL_REDIS_PREFIX_VALUE: {
                    if (!readline(out)) {
                        return false;
                    }
                    zcstring tmp{(char *)&out[offset], out.size()-offset, false};
                    out << '\0';
                    stagging.emplace_back(Reply(prefix, std::move(tmp)));
                    return true;
                }
                case SUIL_REDIS_PREFIX_STRING: {
                    int64_t len{0};
                    if (!readlen(len)) {
                        return false;
                    }

                    zcstring  tmp = zcstring{nullptr};
                    if (len >= 0) {
                        size_t size = (size_t) len + 2;
                        out.reserve((size_t) size + 2);
                        if (!adaptor.read(&out[offset], size, config.timeout)) {
                            idebug("receiving string '%lu' failed: %s", errno_s);
                            return false;
                        }
                        // only interested in actual string
                        if (len) {
                            out.seek(len);
                            tmp = zcstring{(char *) &out[offset], (size_t) len, false};
                            // terminate string
                            out << '\0';
                        }
                    }
                    stagging.emplace_back(Reply(SUIL_REDIS_PREFIX_STRING, std::move(tmp)));
                    return true;
                }

                case SUIL_REDIS_PREFIX_ARRAY: {
                    int64_t len{0};
                    if (!readlen(len)) {
                        return false;
                    }

                    if (len == 0) {
                        // remove crlf
                        if (!readline(out)) {
                            return false;
                        }
                    }
                    else if (len > 0) {
                        int i = 0;
                        for (i; i < len; i++) {
                            if (!recvresp(out, stagging)) {
                                return false;
                            }
                        }
                    }

                    return true;
                }

                default: {
                    ierror("received Response with unsupported type: %c", out[0]);
                    return false;
                }
            }
        }

        bool base_client::readlen(int64_t &len) {
            zbuffer out{16};
            if (!readline(out)) {
                return false;
            }
            zcstring tmp{(char *) &out[0], out.size(), false};
            tmp.data()[tmp.size()] = '\0';
            utils::cast(tmp, len);
            return true;
        }

        bool base_client::readline(zbuffer &out) {
            out.reserve(255);
            size_t cap{out.capacity()};
            do {
                if (!adaptor.receiveuntil(&out[out.size()], cap, "\r", 1, config.timeout)) {
                    if (errno == ENOBUFS) {
                        // reserve buffers
                        out.seek(cap);
                        out.reserve(512);
                        continue;
                    }
                    return false;
                }

                out.seek(cap-1);
                uint8_t  tmp[2];
                cap = sizeof(tmp);
                if (!adaptor.receiveuntil(tmp, cap, "\n", 1, config.timeout)) {
                    // this should be impossible
                    return false;
                }
                break;
            } while (true);

            return true;
        }

        bool base_client::info(server_info& out) {
            Commmand cmd("INFO");
            Response resp = send(cmd);
            if (!resp) {
                // couldn't receive Response
                ierror("failed to receive server information");
                return false;
            }

            Reply& rp = resp.entries[0];
            auto parts = utils::strsplit(rp.data, "\r");
            for (auto& part: parts) {
                if (*part == '\n') part++;
                if (part[0] == '#' || strlen(part) == 0) continue;

                char *k = part;
                char *v = strchr(part, ':');
                v[0] = '\0';
                zcstring key{k, (size_t) (v-k)-1, false};
                v++;
                zcstring val{v, strlen(v), false};

                if (key.compare("redis_version") == 0) {
                    out.version =  std::move(val);
                }
                else {
                    out.params.emplace(std::make_pair(
                            std::move(key), std::move(val)));
                }
            }

            return true;
        }
    }
}