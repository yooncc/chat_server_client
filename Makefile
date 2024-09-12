all : chat_server chat_client

chat_server_arm : chat_server_arm.o
	aarch64-linux-gnu-gcc-12 -o chat_server_arm chat_server_arm.o

chat_server_arm.o : chat_server.c
	aarch64-linux-gnu-gcc-12 -c chat_server.c
	
chat_server : chat_server.o
	gcc -o chat_server chat_server.o

chat_server.o : chat_server.c
	gcc -c chat_server.c

chat_client : chat_client.o
	gcc -o chat_client chat_client.o

chat_client.o : chat_client.c
	gcc -c chat_client.c

clean :
	rm -f chat_client.o chat_client chat_server.o chat_server
