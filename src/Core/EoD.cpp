#include "Core/EoD.hpp"

EoD::EoD() {

}

EoD::~EoD() {

}

void EoD::start() {
    eod_fd = socket(AF_INET, SOCK_STREAM, 0);

    if(eod_fd == 0) {
        perror("eod socket");
    }

    sockaddr_in eod_addr{};
    eod_addr.sin_family = AF_INET;
    eod_addr.sin_addr.s_addr = INADDR_ANY;
    eod_addr.sin_port = htons(eod_port);

    if(bind(eod_fd, (sockaddr*)&eod_addr, sizeof(eod_addr)) < 0) {
        perror("eod bind");
    }

    epoll_fd = epoll_create1(0);

    epoll_event event{};
    event.events = EPOLLIN | EPOLLOUT;
    event.data.fd = eod_fd;

    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, eod_fd, &event) < 0){
        perror("epoll ctl");
    }

    epoll_event events[max_event];

    while(true) {
        int n = epoll_wait(epoll_fd, events, max_event, -1);

        for(int i = 0; i < n; ++i) {
            if(events[i].data.fd == eod_fd) {
                
            }
        }
    }
}