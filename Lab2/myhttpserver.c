#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"
#include "wq.h"

#define BLOCK 1024
#define LIBHTTP_REQUEST_MAX_SIZE 16384

// Struct used in proxy mode.
struct two_fds {
  int client_fd;
  int server_fd;
};

void* client_to_server(void *args);
void* server_to_client(void *args);
/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue;
int num_threads = 5;
int server_port;
char server_files_directory[256];
char *server_proxy_hostname;
char *ipaddr;
int server_proxy_port;
int FileOrProxyMode=0;//0--file|1--proxy
int debugger;
// bool isDirectory(const char* path);

/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 */

void write_file(char* direction,char *file_path, int fd,int status) {
  //get the data size.
  // fprintf("write_file:%s\n",file_path);
  FILE* fp = fopen(file_path, "rb+");
  if(fp){

  }else{
    fprintf("can't open :%s\n",file_path);
  }
  fseek(fp, 0, SEEK_END);
  int size = ftell(fp);
  rewind(fp);
  
  char *read_buffer = malloc(size+1);
  if (!read_buffer) {
    fprintf(stderr, "%s\n", "Malloc Failed");
  }
  int bytes_read = fread(read_buffer, 1, size, fp);
  printf("[write_file]: bytes_read: %d\n", bytes_read);
  read_buffer[bytes_read] = '\0';
  // convert the int to char*.
  char sz_string[20];
  snprintf(sz_string, sizeof(sz_string), "%d", bytes_read);

  // start to write response.
  http_start_response(fd, status);
  http_send_header(fd, "Content-Type", http_get_mime_type(file_path));
  http_send_header(fd, "Content-Length", sz_string);
  http_end_headers(fd);
  http_send_data(fd, read_buffer, bytes_read);

  fclose(fp);
  //free(read_buffer);
}

void handle_files_request(int fd) {

  /*
   * TODO: Your solution for Task 1 goes here! Feel free to delete/modify *
   * any existing code.
   */
  printf("in handlers\n");
  struct http_request *request = http_request_parse(fd);
  printf("method:%s\n",request->method);
  printf("path:%s\n",request->path);
  printf("parse request succeed.\n");
  if (!request) {
    fprintf(stderr, "%s\n", "Request parse Failed");
    return;
  }
  if(strcmp(request->method, "GET") == 0)
  {

    char *file_path = malloc(strlen(server_files_directory) + strlen(request->path)+15);
    strncpy(file_path, server_files_directory, strlen(server_files_directory)); // don't need the last '/'.
    file_path[strlen(server_files_directory)] = '\0';
    // printf("%s %d\n", file_path, strlen(file_path));
    strcat(file_path, request->path);
    printf("Request path: %s\n", request->path);
    printf("file_path: %s\n",file_path);
    int status=200;
    if(strcmp(request->path, "/404.html") == 0) status=404;
    else if(strcmp(request->path, "/501.html") == 0) status=501;

    if(strcmp(request->path, "/api/check") == 0)
    {
      file_path[strlen(server_files_directory)] = '\0';
      strcat(file_path, "/data.txt");
      write_file(server_files_directory,file_path,fd,status);
      return;
    }else if(strcmp(request->path, "/api/check") == 0)
    {
      file_path[strlen(server_files_directory)] = '\0';
      strcat(file_path, "/data.txt");
      write_file(server_files_directory,file_path,fd,status);
      return;
    }
    struct stat buf;
    int FileOrFolder; 
    int result;
    result = stat(file_path, &buf);
    fprintf("statusofstat:%d\n",result);
    if(result==-1)
    {
      file_path[strlen(server_files_directory)] = '\0';
      strcat(file_path, "/404.html");
      write_file(server_files_directory,file_path,fd,404);
    }else if(S_IFDIR & buf.st_mode)
    {
      char *index_path = malloc(50);
      strcpy(index_path, file_path);
      strcat(index_path, "/index.html");
      write_file(server_files_directory,index_path,fd,status);
    }
    else if(S_IFREG & buf.st_mode&&access(file_path, F_OK) == 0)
    {
       write_file(server_files_directory,file_path,fd,status);
    }
  }else if(strcmp(request->method, "POST") == 0)
  {
    http_start_response(fd, 200);
    http_send_header(fd, "Content-Type", request->contenttype);
    http_send_header(fd, "Content-Length", request->contentlength);
    http_end_headers(fd);
    http_send_string(fd, request->content);

  }else{
        http_start_response(fd, 404);
        http_send_header(fd, "Content-Type", "text/html");
        http_end_headers(fd);
        http_send_string(fd,
        "<center>"
        "<h1>404 Not Found.</h1>"
        "<hr>"
        "<p>Error.</p>"
        "</center>");
  }
  // free(file_path);
}


/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {//这里的fd是client和httpserver的连接的fd

  /*
  * The code below does a DNS lookup of server_proxy_hostname and 
  * opens a connection to it. Please do not modify.
  */

  struct sockaddr_in target_address;
  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port = htons(server_proxy_port);

  struct hostent *target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

  int client_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (client_socket_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    exit(errno);
  }

  if (target_dns_entry == NULL) {
    fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
    exit(ENXIO);
  }

  char *dns_address = target_dns_entry->h_addr_list[0];

  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  int connection_status = connect(client_socket_fd, (struct sockaddr*) &target_address,
      sizeof(target_address));

  if (connection_status < 0) {//如果与proxy建立连接失败，返回错误信息504给client
    /* Dummy request parsing, just to be compliant. */
    http_request_parse(fd);

    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);
    http_send_string(fd, "<center><h1>502 Bad Gateway</h1><hr></center>");
    return;

  }

  /* 
  * TODO: Your solution for task 3 belongs here! 
  */
  struct two_fds fds;
  fds.client_fd = fd;//和client连接的fd
  fds.server_fd = client_socket_fd;//和proxy连接的fd
  /* create two threads. 
   * Thread 1: send data from client to server.
   * Thread 2: send data from server to client.
   */ 
  pthread_t thread1, thread2;
  pthread_create(&thread1, NULL, &client_to_server, (void *)&fds);
  pthread_create(&thread2, NULL, &server_to_client, (void *)&fds);
  pthread_join(thread1, NULL);
  pthread_join(thread2, NULL);
}

void* client_to_server(void *args) {
  struct two_fds *fds = (struct two_fds*) args;
  char buffer[1024];
  int bytes_read;
  //将client写入的东西交给proxy处理，所以从client中read然后write给了server
  while ((bytes_read = read(fds->client_fd, buffer, sizeof(buffer))) > 0) {
    int bytes_write = write(fds->server_fd, buffer, bytes_read);
    // if write failed, close the socket.
    if (bytes_write < 0) {
      close(fds->client_fd);
    }
  }
  return NULL;
}

void* server_to_client(void *args) {
  struct two_fds *fds = (struct two_fds*) args;
  char buffer[1024];
  int bytes_read;
  //将server写的东西交给client，所以从proxy中read然后write给了client
  while ((bytes_read = read(fds->server_fd, buffer, sizeof(buffer))) > 0) {
    int bytes_write = write(fds->client_fd, buffer, bytes_read);
    // if write failed, clise the socket.
    if (bytes_write < 0) {
      close(fds->server_fd);
    }
  }
  return NULL;
}

void* thread_routine(void* args) {
  void (*request_handler)(int) = (void(*)(int))args;
  printf("[%lu] Thread start.\n", pthread_self());
  while (1) {
    int child_socket_fd = wq_pop(&work_queue);
    printf("[%lu] Get the socket_fd. Start to handle it.\n", pthread_self());
    request_handler(child_socket_fd);
    close(child_socket_fd);
    printf("[%lu] Finish to handle the request.\n", pthread_self());
  }
  return NULL;
}


void init_thread_pool(int num_threads, void (*request_handler)(int)) {
  /*
   * TODO: Part of your solution for Task 2 goes here!
   */
  wq_init(&work_queue);
  printf("[init_thread_pool] num_threads: %d\n", num_threads);
  for (int i = 0; i < num_threads; i++) {
    pthread_t thread;
    pthread_create(&thread, NULL, &thread_routine, request_handler);
  }
}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  //绑定端口号等工作
  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = inet_addr(ipaddr);
  server_address.sin_port = htons(server_port);
  printf("connect to %s\n",ipaddr);
  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }
  //监听连接
  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }
  printf("Listening on port %d...\n", server_port);
  //初始化线程池
  init_thread_pool(num_threads, request_handler);

  while (1) {
    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }
    time_t current_time = time(NULL);
    char *c_time_string = ctime(&current_time);
    printf("\n[%s]: Accepted connection from %s on port %d\n",
        c_time_string, 
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);
    wq_push(&work_queue, client_socket_number);
  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files www_directory/ --port 8000 [--num-threads 5]\n"
  "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);//当捕捉到Ctrl-C时退出

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;//request_handler为要求的处理方式

  int i;
  for (i = 1; i < argc; i++) {
     if (strcmp("--proxy", argv[i]) == 0) {//若为代理模式
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];//要求代理服务器名称?
      FileOrProxyMode=1;

      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }
      char *colon_pointer = strchr(proxy_target, ':');//截取代理服务器端口号
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;//否则默认设置为80
        server_proxy_port = 80;
      }

    } else if (strcmp("--port", argv[i]) == 0) {//设置本地的端口
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--threads", argv[i]) == 0) {//设置线程数量
      char *num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--ip", argv[i]) == 0) {
      ipaddr=argv[++i];
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }
  if(!FileOrProxyMode)
  {
      request_handler = handle_files_request;//为文件传输模式
      getcwd(server_files_directory,256);//设置文件目录
      if (!server_files_directory) {//若目录不合法
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
  }
  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
