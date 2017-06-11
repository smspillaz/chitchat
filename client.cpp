#include "connection.h"
#include <map>
#include <vector>
#include <iostream>
#include <limits>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

typedef std::map<std::string, std::vector<std::string>> Rooms;
typedef std::pair<std::string, int> RoomInfo;

Rooms decode_rooms(std::string const &response) {
    Rooms rooms;

    if (response.empty()) {
        return rooms;
    }

    std::vector<std::string> entries;
    boost::split(entries, response, boost::is_any_of(";"), boost::token_compress_on);


    for (auto const &entry : entries) {
        if (entry.empty()) {
            continue;
        }

        std::vector<std::string> vec;
        boost::split(vec, entry, boost::is_any_of(":"), boost::token_compress_on);

        rooms[vec[0]] = std::vector<std::string>(vec.begin() + 1, vec.end());
    }

    return rooms;
}



std::string format_rooms(Rooms const &rooms) {
    std::stringstream ss;
    unsigned int i = 0;

    for (auto const &pair : rooms) {
        ss << "(" << i << ") " << pair.first << " " << pair.second[0] << ":" << pair.second[1] << std::endl;
    }

    return ss.str();
}

RoomInfo parse_room_info(std::vector<std::string> const &params) {
    return RoomInfo(params[0], atoi(params[1].c_str()));
}

int main(int argc, char **argv) {
    std::string client;
    std::cout << "Desired username: ";
    std::getline(std::cin, client);

    ConnectionlessSocket sock(argv[1], atoi(argv[2]), SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr *addr = NULL;
    size_t addr_len = 0;

    std::tie(addr, addr_len) = sock.addr();
    if (send_line_to_socket("list",
                            sock,
                            addr,
                            addr_len) == -1) {
        perror("client");
        exit(1);
    }

    std::string response = recv_from_until(sock, '\n', nullptr, nullptr);
    boost::trim_right(response);
    Rooms rooms(decode_rooms(response));

    if (!rooms.size()) {
        std::cout << "No rooms available on this host :(" << std::endl;
        return 0;
    }

    std::string room_name;

    while (rooms.find(room_name) == rooms.end()) {
        std::cout << format_rooms(rooms);
        std::cout << "Desired room: ";
        std::getline(std::cin, room_name);
    }

    /* Now that we have our room name, connect to its server
     * and tell it that we are going to join */
    RoomInfo info(parse_room_info(rooms[room_name]));
    ConnectSocket server_sock(info.first.c_str(), info.second, SOCK_STREAM, 0);

    /* Now that we have a connection, we can chat, until we type '/exit' */
    if (write_line_to_fd("enter:" + room_name + ":" + client, server_sock) == -1) {
        perror("client");
        exit(1);
    }

    while (true) {
        std::string cmd;
        std::getline(std::cin, cmd);
        if (cmd == "/exit") {
            write_line_to_fd("leave:" + room_name + ":" + client,
                             server_sock);
            break;
        }

        write_line_to_fd("chat:" + room_name + ":" + client + ":" + cmd,
                         server_sock);
    }

    return 0;
}