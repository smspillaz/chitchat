#include <cstdio>
#include <cstdlib>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>

#include <stdexcept>
#include <string>
#include <sstream>
#include <tuple>

#include <iostream>

class Socket {
    public:

        Socket(int type, int proto) {
            sock = socket(PF_INET, type, proto);
            if (sock < 0) {
                perror("socket");
                exit(1);
            }
        }

        Socket(Socket const &) = delete;
        Socket(Socket &&) = delete;
        Socket operator=(Socket const &) = delete;
        Socket operator=(Socket &&) = delete;

        ~Socket() {
            shutdown(sock, SHUT_RDWR);
            close(sock);
        }

        operator int() const {
            return sock;
        }

    protected:
        int sock;
};

class ConnectionlessSocket : public Socket {
    public:
        ConnectionlessSocket(char const *addr_str, int port, int type, int proto) :
            Socket(type, proto)
        {
            addr_in.sin_family = AF_INET;
            addr_in.sin_port = htons(port);
            if (inet_aton(addr_str, &addr_in.sin_addr) == 0) {
                perror("socket");
                exit(1);
            }
        }

        std::tuple<struct sockaddr *, size_t> addr() const {
            struct sockaddr_in *addr_in_mut = const_cast<struct sockaddr_in *>(&addr_in);
            return std::make_tuple(reinterpret_cast<struct sockaddr *>(addr_in_mut),
                                   sizeof(struct sockaddr_in));
        }

    protected:
        struct sockaddr_in addr_in;
};

class ConnectSocket : public ConnectionlessSocket {
    public:
        ConnectSocket(char const *addr_str, int port, int type, int proto) :
            ConnectionlessSocket(addr_str, port, type, proto)
        {
            if (connect(sock,
                        reinterpret_cast<struct sockaddr *>(&addr_in),
                        sizeof(addr_in)) == -1) {
                perror("socket");
                exit(1);
            }
        }
};

class BindSocket : public Socket {
    public:
        BindSocket(int port, int type, int proto) :
            Socket(type, proto)
        {
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = htonl(INADDR_ANY);

            if (bind(sock,
                     reinterpret_cast<struct sockaddr *>(&addr),
                     sizeof(addr)) == -1) {
                perror("server");
                exit(1);
            }
        }
};

std::string format_room_name(char const *addr, char const *port, char const *title) {
    std::stringstream ss;
    ss << addr << ":" << port << ":" << title;

    return ss.str();
}


int write_string_to_fd(std::string const &str, int fd) {
    size_t len = str.size();
    return write(fd, str.c_str(), len);
}

int write_line_to_fd(std::string const &str, int fd) {
    return write_string_to_fd(str + "\n", fd);
}

int send_string_to_socket(std::string const &str,
                          int sock,
                          struct sockaddr *sa,
                          size_t sa_len) {
    size_t len = str.size();
    return sendto(sock, str.c_str(), len, 0, sa, sa_len);
}

int send_line_to_socket(std::string const &str,
                        int sock,
                        struct sockaddr *sa,
                        size_t sa_len) {
    return send_string_to_socket(str + "\n", sock, sa, sa_len);
}

std::string read_fd_until(int fd, char term) {
    std::stringstream ss;
    while (true) {
        char buf[256];
        const size_t read_bytes = read(fd, &buf, 255);
        if (read_bytes == -1) {
            throw std::runtime_error("Couldn't read from file descriptor");
        }

        buf[read_bytes] = '\0';
        ss << buf;

        if (buf[read_bytes - 1] == term) {
            break;
        }
    }

    return ss.str();
}

std::string recv_from_until(int sock, char term, struct sockaddr *addr, socklen_t *len) {
    std::stringstream ss;
    while (true) {
        char buf[256];
        const size_t read_bytes = recvfrom(sock, &buf, 255, 0, addr, len);
        if (read_bytes == -1) {
            perror("recvfrom");
            exit(1);
        }

        buf[read_bytes] = '\0';
        ss << buf;
        if (buf[read_bytes - 1] == term) {
            break;
        }
    }

    return ss.str();
}