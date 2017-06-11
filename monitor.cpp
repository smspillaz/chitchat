#include "connection.h"
#include <algorithm>
#include <map>
#include <vector>
#include <iostream>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

typedef std::map<std::string, std::vector<std::string>> Rooms;

enum class OpCode : unsigned int {
    List,
    Add,
    Remove
};

typedef std::pair<OpCode, std::vector<std::string>> Op;

Op decode_to_op(std::string const &payload) {
    std::vector<std::string> vec;
    boost::split(vec, payload, boost::is_any_of(":"), boost::token_compress_on);
    boost::trim_right(vec[0]);

    if (vec[0] == "list") {
        return Op(OpCode::List, std::vector<std::string>());
    } else if (vec[0] == "add") {
        return Op(OpCode::Add, std::vector<std::string>(vec.begin() + 1, vec.end()));
    } else if (vec[0] == "remove") {
        return Op(OpCode::Remove, std::vector<std::string>(vec.begin() + 1, vec.end()));
    }

    throw std::runtime_error("Don't know how to handle opcode " + vec[0]);
}

std::string encode_rooms(Rooms const &rooms) {
    std::stringstream ss;
    for (auto const &pair : rooms) {
        ss << pair.first << ":";
        for (auto const &entry : pair.second) {
            ss << entry << ":";
        }

        ss << ";";
    }

    return ss.str();
}

int main(int argc, char **argv) {
    BindSocket sock(atoi(argv[1]), SOCK_DGRAM, IPPROTO_UDP);
    Rooms rooms;

    while (true) {
        struct sockaddr_in src;
        socklen_t addr_len = sizeof(struct sockaddr_in);
        std::cout << "[monitor] Waiting to receive a command" << std::endl;
        std::string request = recv_from_until(sock,
                                              '\n',
                                              reinterpret_cast<struct sockaddr *>(&src),
                                              &addr_len);
        boost::trim_right(request);

        Op op(decode_to_op(request));
        switch (op.first) {
            case OpCode::List: {
                std::cout << "[monitor] Received list request" << std::endl;
                std::string encoded(encode_rooms(rooms));
                if (send_line_to_socket(encoded,
                                        sock,
                                        reinterpret_cast<struct sockaddr *>(&src),
                                        addr_len) == -1) {
                    perror("monitor");
                    exit(1);
                }
                break;
            }
            case OpCode::Add:
                std::cout << "[monitor] Received add request: " << op.second[0] << std::endl;
                rooms[op.second[0]] = std::vector<std::string>(op.second.begin() + 1,
                                                               op.second.end());
                break;
            case OpCode::Remove: {
                std::cout << "[monitor] Received remove request: " << op.second[0] << std::endl;
                Rooms::iterator it = rooms.find(op.second[0]);
                if (it != rooms.end()) {
                    rooms.erase(it);
                }
                break;
            }
        }
    }

    return 0;
}