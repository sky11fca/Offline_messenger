FROM archlinux:latest

RUN pacman -Syu base-devel git --noconfirm 

WORKDIR /root/serv

COPY server.c /root/serv/

RUN gcc server.c -o server 

EXPOSE 9090 

CMD ["./server"]