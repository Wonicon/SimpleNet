all: client/app_client.o server/app_server.o client/stcp_client.o server/stcp_server.o common/seg.o
	gcc -g -pthread -I common server/app_server.o common/seg.o server/stcp_server.o -o server/lab5_server
	gcc -g -pthread -I common client/app_client.o common/seg.o client/stcp_client.o -o client/lab5_client

server: server/app_server.o server/stcp_server.o common/seg.o
	gcc -g -pthread -I common server/app_server.o common/seg.o server/stcp_server.o -o server/lab5_server

client: client/app_client.o client/stcp_client.o common/seg.o
	gcc -g -pthread -I common client/app_client.o common/seg.o client/stcp_client.o -o client/lab5_client

client/app_client.o: client/app_client.c
	gcc -pthread -I common -g -c client/app_client.c -o client/app_client.o
server/app_server.o: server/app_server.c
	gcc -pthread -I common -g -c server/app_server.c -o server/app_server.o

common/seg.o: common/seg.c common/seg.h
	gcc -g -c common/seg.c -o common/seg.o
client/stcp_client.o: client/stcp_client.c client/stcp_client.h
	gcc -pthread -I common -g -c client/stcp_client.c -o client/stcp_client.o
server/stcp_server.o: server/stcp_server.c server/stcp_server.h
	gcc -pthread -I common -g -c server/stcp_server.c -o server/stcp_server.o

clean:
	rm -rf client/*.o
	rm -rf server/*.o
	rm -rf common/*.o
	rm -rf client/lab5_client
	rm -rf server/lab5_server

