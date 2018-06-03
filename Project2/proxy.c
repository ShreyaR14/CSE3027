#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>  // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h> // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h> // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <fcntl.h>      //O_WRONLY
#include <unistd.h>     //write(), close()
#include <strings.h>
#include <netdb.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "LRUCache.h"

#define BUFF_SIZE 1024
#define READ_BUFF_SIZE 16384

LRU_LinkedList cache;

void error(char *msg)
{
  perror(msg);
  exit(0);
}

void log_history(char *message)
{
  int log_file;
  if ((log_file = open("proxy.log", O_CREAT | O_RDWR | O_APPEND, 0644)) < 0)
    error("Failed to open Log File");

  printf("LOG:: %s", message);

  if (write(log_file, message, strlen(message)) < 0)
    error("Failed to write log");
  close(log_file);
}

int main(int argc, char *argv[])
{
  char buffer[BUFF_SIZE];
  char log_message[BUFF_SIZE];

  int client_fd, proxy_fd, host_fd;
  int port;
  struct sockaddr_in client_addr, proxy_addr, host_addr;

  init_LRU_LinkedList(&cache);

  if (argc < 2)
    error("Port is not provided");

  port = atoi(argv[1]);

  if ((proxy_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    error("Failed to open proxy socket");

  memset(&proxy_addr, 0, sizeof(proxy_addr));
  proxy_addr.sin_family = AF_INET;
  proxy_addr.sin_addr.s_addr = INADDR_ANY;
  proxy_addr.sin_port = htons(port);

  int opt = 1;
  setsockopt(proxy_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, (int)sizeof(opt));

  if (bind(proxy_fd, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0)
    error("Failed to bind");

  listen(proxy_fd, 5); // 5 is count of Backlog queue

  printf("-------- Start to proxy --------\n\n");

  while (1)
  {
    socklen_t client_len = sizeof(client_addr);
    client_fd = accept(proxy_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0)
      error("Failed to accept");

    time_t t;
    t = time(NULL);
    struct tm *tm = localtime(&t);
    char *timestamp = asctime(tm);
    timestamp[strlen(timestamp) - 1] = '\0';
    printf("Time: %s\n", timestamp);

    memset(buffer, 0, BUFF_SIZE);
    if (read(client_fd, buffer, BUFF_SIZE) < 0)
    {
      close(client_fd);
      error("Failed to read");
    }

    printf("Origin Request message\n%s", buffer);

    char *req_method = strtok(buffer, " ");
    char *path = strtok(NULL, " ");
    char *url = (char *)malloc(strlen(path));
    strcpy(url, path);
    strtok(path, "//");
    char *temp_host = strtok(NULL, "//");
    char *host_path = (char *)malloc(strlen(temp_host));
    strcpy(host_path, temp_host);
    printf("Request Method: %s\n", req_method);
    printf("URL: %s\n", url);
    printf("Host: %s\n\n", host_path);

    struct hostent *host = gethostbyname(host_path);
    if (host == NULL)
      error("Failed to get host");

    memset(&host_addr, 0, sizeof(host_addr));
    host_addr.sin_family = AF_INET;
    host_addr.sin_addr.s_addr = *((unsigned long *)host->h_addr_list[0]);
    host_addr.sin_port = htons(80);
    char *ip = inet_ntoa(host_addr.sin_addr);
    // if (strcmp(req_method, "GET") == 0)
    // {
    Node *cachedNode = search_node(&cache, url);
    // When the page is cached
    if (cachedNode != NULL)
    {
      printf("\n-------- HIT in Cache --------\n");
      printf("Cached URL: %s\n", cachedNode->url);
      if (write(client_fd, cachedNode->object, cachedNode->data_size) < 0)
        error("Failed to write on the Cached");
      print_cache(&cache);
      memset(log_message, 0, BUFF_SIZE);
      sprintf(log_message, "%s %s %s %d\n", timestamp, ip, url, cachedNode->data_size);
      log_history(log_message);
    }
    else
    {
      printf("Can't find node in Cache\n\n");
      if ((host_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        error("Failed to open host socket");

      if (connect(host_fd, (struct sockaddr *)&host_addr, sizeof(host_addr)) < 0)
        error("Failed to connect with host");

      char req_header[BUFF_SIZE];
      sprintf(req_header, "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", url, host_path);
      printf("Request Header\n%s", req_header);

      if (write(host_fd, req_header, BUFF_SIZE) < 0)
        error("Failed to write on socket");

      memset(buffer, 0, BUFF_SIZE);

      int data_size = 0;
      int n = 0;
      char *object = (char *)malloc(MAX_OBJECT_SIZE);
      char read_buff[READ_BUFF_SIZE];

      memset(object, 0, MAX_OBJECT_SIZE);
      memset(read_buff, 0, READ_BUFF_SIZE);

      while ((n = read(host_fd, read_buff, READ_BUFF_SIZE)) > 0)
      {
        data_size += write(client_fd, read_buff, n);
        if (data_size < MAX_OBJECT_SIZE)
          strcat(object, read_buff);
        memset(read_buff, 0, READ_BUFF_SIZE);
      }

      if (data_size <= MAX_OBJECT_SIZE)
      {
        printf("** Cache new page **\n");
        Node *node = create_node(url, object);
        add_node(&cache, node);
        print_node(node);
      }

      print_cache(&cache);
      close(host_fd);

      memset(log_message, 0, BUFF_SIZE);
      sprintf(log_message, "%s %s %s %d\n", timestamp, ip, url, data_size);

      log_history(log_message);
    }
  }
  close(client_fd);
  close(proxy_fd);
  return 0;
}