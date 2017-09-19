#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFF_SIZE 128

void usage(int argc, char *argv[])
{
	if(argc != 2)
	{
		fprintf(stderr, "usage: %s <PORT>.\n", argv[0]);
		exit(1);
	}
}

/*sfd：传输文件的socket
* filename：要传输的文件
*/
int send_file(int sfd, char *filename)
{
	assert(filename);
	
	const char *start = "start\r\n";

	int fd = open(filename, O_RDONLY);
	if(fd == -1)
	{
		fprintf(stderr, "open() %s error.\n", filename);
		return -1;
	}

	char *buf = malloc(BUFF_SIZE);
	int nwrite, nsum = 0, nread;

	uint32_t total_size;
	/*获得待发送文件的大小*/
	struct stat f_stat;
	bzero(&f_stat, sizeof(f_stat));
	int err = stat(filename, &f_stat);
	if(err == -1)
	{
		perror("stat() error");
		return -1;
	}
	total_size = f_stat.st_size;
	printf("total file size:%ubytes.\n", total_size);
	
	/*发送文件传送标志位*/
	nwrite = write(sfd, start, strlen(start));
	nsum += nwrite;

	/*发送文件大小*/
	nwrite = write(sfd, &total_size, sizeof(total_size));
	nsum += nwrite;

	/*发送文件内容*/
	while(1)
	{
		bzero(buf, BUFF_SIZE);
		nread = read(fd, buf, BUFF_SIZE-1);
		if(nread == 0)
		{
			break;
		}

		nwrite = write(sfd, buf, nread);
		nsum += nwrite;
	}

	printf("send all %dbytes,%s size:%dbytes\n",
			nsum, 
			filename,
			(int)(nsum-strlen(start)-4));

	close(fd);
	return nsum;
}


uint32_t parse_size(unsigned char *buf)
{
	int i = 0;
	uint32_t temp = 0;
	printf("parse:");
	for(i=0; i<4; i++)
	{
		printf("0x%.2x ", *(buf+i));
	}
	printf("\n");

	for(i=0; i<4; i++)
	{
		temp |= *(buf+i) << (i*8);
	}
	printf("parse over:%.8x\n", temp);
	return temp;
}

int recv_file(int sfd, char *filename)
{
	//assert(filename);

	int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if(fd == -1)
	{
		fprintf(stderr, "open() %s error.\n", filename);
		return -1;
	}

	const char *start = "start\r\n";

	int nsum = 0, nread, nwrite, n;
	char *buf = malloc(BUFF_SIZE);
	char *temp = NULL;
	/*判断文件的开始标志位start*/
	while(1)
	{
		bzero(buf, BUFF_SIZE);
		nread = read(sfd, buf, BUFF_SIZE-1);
		temp = strstr(buf, start);
		if(temp != NULL)
			break;
	}
	
	uint32_t total_size = parse_size(temp + strlen(start));
	printf("start receiver file %ubytes...\n", total_size);

	uint32_t process = nread-strlen(start)-4;
	write(fd, buf+strlen(start)+4, process);

	/*开始接收文件内容*/
	while((total_size-process)>0)
	{
		if((total_size-process) > (BUFF_SIZE-1))
			nread = BUFF_SIZE-1;
		else
			nread = total_size-process;

		bzero(buf, BUFF_SIZE);
		n = read(sfd, buf, nread);	
		write(fd, buf, n); //把接收到的文件内容写到文件中

		process += n;
	}
	printf("receive all %ubytes.\n", process);

	close(fd);
}
int main(int argc, char *argv[])
{
	
	usage(argc, argv);
	int sfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sfd == -1)
	{
		perror("socket() error");
		exit(1);
	}

	int on = 1;
	setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(on));

	struct sockaddr_in srv;
	int srv_len = sizeof srv;
	bzero(&srv, srv_len);

	/*设置本地IP地址和端口参数*/
	srv.sin_family = AF_INET;
	srv.sin_port = htons(atoi(argv[1]));
	srv.sin_addr.s_addr = htonl(INADDR_ANY);

	/*绑定本地IP地址和端口*/
	int err = bind(sfd, (struct sockaddr*)&srv, srv_len);
	if(err == -1)
	{
		fprintf(stderr, "bind() PORT:%s error.\n", argv[1]);
		exit(1);
	}

	/*设置套接字为监听状态*/
	err = listen(sfd, 3);
	if(err == -1)
	{
		perror("listen() error");
		exit(1);
	}

	struct sockaddr_in cli;
	socklen_t cli_len;
	bzero(&cli, sizeof(cli));
	
	/*接收客户端的连接请求*/
	printf("watting someboby connect.\n");
	int cfd = accept(sfd, (struct sockaddr*)&cli, &cli_len);
	//int cfd = accept(sfd,NULL, NULL);
	if(cfd == -1)
	{
		perror("accept() error");
		exit(1);
	}

	printf("%s connect success\n", inet_ntoa(cli.sin_addr));

	char buf[10];
	printf("Waitting to receive file from client...\n");
	recv_file(cfd, "bin/wav/seriver_receive.wav");

	printf("Enter any key to send bin/xml/server.xml.\n");
	scanf("%s", buf);
	send_file(cfd, "bin/xml/server.xml");

	close(cfd);
	close(sfd);
		
	return 0;
}
