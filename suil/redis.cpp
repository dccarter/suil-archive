//
// Created by dc on 11/12/17.
//
#include "redis.hpp"

namespace suil {
    namespace redis {

        response& transaction::execute() {
            // send MULTI command
            command multi("MULTI");

            cachedresp = client.send(multi);
            if (!cachedresp.status()) {
                // there is an error on the reply
                ierror("error sending 'MULTI' command: %s", cachedresp.error());
                return cachedresp;
            }

            client.reset();
            client.batch(commands);
            cachedresp.entries.clear();
            zcstring ret = client.commit(cachedresp);
            if (ret) {
                // sending batch commands failed
                ierror("sending batch commands failed: %s", ret());
                return  cachedresp;
            }

            // validate that all commands passed
            int id{0};
            for (auto& rp: cachedresp.entries) {
                if (!rp.special()) {
                    if (!rp.status("QUEUED")) {
                        // sending command failed
                        ierror("sending command '%s' failed: %s",
                               commands[id]()(), rp.error());
                        cachedresp.entries.clear();
                        return cachedresp;
                    }
                }
                id++;
            }

            // send MULTI command
            command exec("EXEC");
            cachedresp = client.send(exec);
            if (!cachedresp) {
                // there is an error on the reply
                ierror("error sending 'EXEC' command: %s", ret());
                return cachedresp;
            }

            return cachedresp;
        }

        response base_client::dosend(command &cmd, size_t nrps) {
            // send the command to the server
            zcstring data = cmd.prepared();
            size_t size = adaptor.send(data.cstr, data.len, config.timeout);
            if (size != data.len) {
                // sending failed somehow
                return response{reply('-', utils::catstr("sending failed: ", errno_s))};
            }
            adaptor.flush(config.timeout);

            response resp;
            std::vector<reply> stage;
            do {
                if (!recvresp(resp.buffer, stage)) {
                    // receiving data failed
                    return response{reply('-',
                                          utils::catstr("receiving response failed: ", errno_s))};
                }
            } while (--nrps > 0);

            resp.entries = std::move(stage);
            return std::move(resp);
        }

        zcstring base_client::commit(response &resp) {
            // send all the commands at once and read all the responses in one go
            command *last = batched.back();
            batched.pop_back();

            for (auto& cmd: batched) {
                zcstring data = cmd->prepared();
                size_t size = adaptor.send(data.str, data.len, config.timeout);
                if (size != data.len) {
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

        bool base_client::recvresp(buffer_t& out, std::vector<reply>& stagging) {
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
                    stagging.emplace_back(reply(prefix, std::move(tmp)));
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
                    stagging.emplace_back(reply(SUIL_REDIS_PREFIX_STRING, std::move(tmp)));
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
                    ierror("received response with unsupported type: %c", out[0]);
                    return false;
                }
            }
        }

        bool base_client::readlen(int64_t &len) {
            buffer_t out{16};
            if (!readline(out)) {
                return false;
            }
            zcstring tmp{(char *) &out[0], out.size(), false};
            tmp.str[tmp.len] = '\0';
            utils::cast(tmp, len);
            return true;
        }

        bool base_client::readline(buffer_t &out) {
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
            command cmd("INFO");
            response resp = send(cmd);
            if (!resp) {
                // couldn't receive response
                ierror("failed to receive server information");
                return false;
            }

            reply& rp = resp.entries[0];
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