//
// Created by dc on 11/02/18.
//

#ifndef SUIL_RESULT_HPP
#define SUIL_RESULT_HPP

#include <suil/utils.h>

namespace suil {

    struct Result: OBuffer {
        enum : int {
            OK    = 0,
            ERROR = -1
        };

        Result(int code)
            : OBuffer(0),
              Code(code)
        {}

        Result()
            : Result(Result::OK)
        {}

        Result(const Result&) = delete;

        Result&operator=(const Result&) = delete;

        Result(Result&& res)
            : OBuffer((OBuffer&&)res),
              Code(res.Code)
        {}

        Result&operator=(Result&& other) {
            OBuffer::operator=(std::move(other));
            Code = other.Code;
            return *this;
        }

        inline bool Ok() const {
            return Ego.Code == Result::OK;
        }

        Result&operator()(int code) {
            if (code != Result::OK) {
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

        int Code{Result::OK};
    };
    
}
#endif //SUIL_RESULT_HPP
