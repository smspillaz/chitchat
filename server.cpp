#include "connection.h"
#include <sys/poll.h>
#include <vector>
#include <string>
#include <map>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

enum class OpCode : unsigned int {
    Enter,
    Leave,
    Chat
};

typedef std::pair<OpCode, std::vector<std::string>> Op;
typedef std::map<std::string, int> Room;
typedef std::map<std::string, Room> Rooms;

Op decode_op(std::string const &input) {
    std::vector<std::string> vec;
    boost::split(vec, input, boost::is_any_of(":"), boost::token_compress_on);
    std::string &op = vec[0];
    boost::trim_right(op);
    std::vector<std::string> params(vec.begin() + 1, vec.end());

    if (op == "enter") {
        return Op(OpCode::Enter, params);
    } else if (op == "leave") {
        return Op(OpCode::Leave, params);
    } else if (op == "chat") {
        return Op(OpCode::Chat, params); 
    }

    throw std::runtime_error("Don't know how to handle op " + op);
}

void broadcast_msg(Room const &room, std::string const &msg) {
    for (auto const &pair : room) {
        /* We can just write directly here */
        write_line_to_fd(msg, pair.second);
    }
}

void broadcast_enter(Room const &room, std::string const &client) {
    std::stringstream ss;
    ss << "enter:" << client << "\n";
    broadcast_msg(room, ss.str());
}

void broadcast_leave(Room const &room, std::string const &client) {
    std::stringstream ss;
    ss << "leave:" << client << "\n";
    broadcast_msg(room, ss.str());
}

void broadcast_chat(Room const &room,
                    std::string const &client,
                    std::string const &msg) {
    std::stringstream ss;
    ss << "chat:" << client << ":" << msg << "\n";
    broadcast_msg(room, ss.str());
}

struct pollfd read_pollfd(int fd) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN | POLLERR | POLLHUP;

    return pfd;
}

void update_room_state(int fd, Rooms &rooms) {
    std::string input = read_fd_until(fd, '\n');
    boost::trim_right(input);
    Op op(decode_op(input));

    switch(op.first) {
        case OpCode::Enter:
            std::cout << "[server] Client (" << op.second[1] << ") entered room: " << op.second[0] << std::endl;
            if (rooms.find(op.second[0]) == rooms.end()) {
                rooms[op.second[0]] = Room();
            }

            rooms[op.second[0]][op.second[1]] = fd;
            broadcast_enter(rooms[op.second[0]], op.second[1]);
            break;
        case OpCode::Leave:
            std::cout << "[server] Client (" << op.second[1] << ") left room: " << op.second[0] << std::endl;
            if (rooms.find(op.second[0]) != rooms.end()) {
                broadcast_leave(rooms[op.second[0]], op.second[1]);
                rooms[op.second[0]].erase(op.second[0]);
            }
            break;
        case OpCode::Chat:
            std::cout << "[server] Client (" << op.second[1] << ") chat in room: " << op.second[0] << " " << op.second[2] << std::endl;
            if (rooms.find(op.second[0]) != rooms.end()) {
                broadcast_chat(rooms[op.second[0]], op.second[1], op.second[2]);
            }
            break;
    }
}

int main(int argc, char **argv) {
    ConnectionlessSocket sock(argv[1], atoi(argv[2]), SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr *addr = NULL;
    size_t addr_len = 0;

    std::tie(addr, addr_len) = sock.addr();
    if (send_line_to_socket("add:" + format_room_name(argv[4], "127.0.0.1", argv[3]),
                            sock,
                            addr,
                            addr_len) == -1) {
        perror("server");
        exit(1);
    }

    /* Listen on the given port for connections */
    BindSocket bound(atoi(argv[3]), SOCK_STREAM, 0);
    listen(bound, 5);

    Rooms rooms;

    while (true) {
        struct sockaddr_in src;
        socklen_t src_len;

        std::vector<struct pollfd> watchfds;
        watchfds.push_back(read_pollfd(bound));
        for (auto const &room_pair : rooms) {
            for (auto const &client_pair : room_pair.second) {
                watchfds.push_back(read_pollfd(client_pair.second));
            }
        }

        if (poll(&watchfds[0], watchfds.size(), -1) == -1) {
            perror("server (poll)");
            exit(1);
        }

        /* If the first fd fired, it was because we can accept a new connection
         * so do that now */
        if (watchfds[0].revents & POLLIN) {
            /* We basically expect the client to update the state immediately
             * or we lose them. Not great design... */
            int fd = accept(watchfds[0].fd, reinterpret_cast<struct sockaddr *>(&src), &src_len);
            update_room_state(fd, rooms);
        }

        /* Now with all the other watched fds, check to see if we've got some
         * events to process */
        std::vector<struct pollfd> chat_fds(watchfds.begin() + 1, watchfds.end());
        for (auto const &fd : chat_fds) {
            if (fd.revents & POLLHUP) {
                continue;
            }

            if (fd.revents & POLLIN) {
                update_room_state(fd.fd, rooms);
            }
        }
    }

    send_line_to_socket("remove:" + std::string(argv[4]), sock, addr, addr_len);
    return 0;
}