//
// Created by dc on 14/12/17.
//

#ifndef SUIL_ABCI_HPP
#define SUIL_ABCI_HPP

#include <suil/abci/abci.pb.h>
#include <suil/net.hpp>

namespace suil {
    namespace tdmabci {

        struct Result: zbuffer {
            Result(types::CodeType code)
                : zbuffer(0),
                  Code(code)
            {}

            Result()
                : Result(types::CodeType::OK)
            {}

            Result(const Result&) = delete;
            Result&operator=(const Result&) = delete;

            Result(Result&& res)
                : zbuffer((zbuffer&&)res),
                  Code(res.Code)
            {}

            Result&operator=(Result&& other) {
                zbuffer::operator=(std::move(other));
                Code = other.Code;
                return *this;
            }

            inline bool Ok() const {
                return Ego.Code == types::CodeType::OK;
            }

            Result&operator()(types::CodeType code) {
                if (code != types::CodeType::OK) {
                    if (!Ego.empty())
                        Ego << '\n';
                    Ego << "Code_" << (int)code << ": ";
                    Ego.Code = code;
                }
                else {
                    Ego << "\n\t";
                }
                return Ego;
            }

            Result&operator()() {
                Ego << "\n\t";
                return Ego;
            }

            types::CodeType Code{types::CodeType::OK};
        };
    }
}

#endif //SUIL_ABCI_HPP
