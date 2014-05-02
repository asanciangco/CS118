/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <sys/stat.h>


void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void dostuff(int); /* function prototype */
void parseRequest(int);
char* readFileBytes(const char *);

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     struct sigaction sa;          // for signal SIGCHLD

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,5);
     
     clilen = sizeof(cli_addr);
     
     /****** Kill Zombie Processes ******/
     sa.sa_handler = sigchld_handler; // reap all dead processes
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &sa, NULL) == -1) {
         perror("sigaction");
         exit(1);
     }
     /*********************************/
     
     while (1) {
         newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		 
         if (newsockfd < 0) 
             error("ERROR on accept");
         
         pid = fork(); //create a new process
         if (pid < 0)
             error("ERROR on fork");
         
         if (pid == 0)  { // fork() returns a value of 0 to the child process
             close(sockfd);
             //dostuff(newsockfd);
			 parseRequest(newsockfd);
             exit(0);
         }
         else //returns the process ID of the child process to the parent
             close(newsockfd); // parent doesn't need this 
     } /* end of while */
     return 0; /* we never get here */
}

/******** DOSTUFF() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void dostuff (int sock)
{
   int n;
   char buffer[256];
      
   bzero(buffer,256);
   n = read(sock,buffer,255);
   if (n < 0) error("ERROR reading from socket");
   printf("Here is the message: %s\n",buffer);
   n = write(sock,"I got your message",18);
   if (n < 0) error("ERROR writing to socket");
}

void parseRequest(int sock)
{
	int n;
	int buf_size = 256;
	char buf[buf_size];
	char req[buf_size];
	char *content_type = "text/html"; 
	
	bzero(buf, buf_size);
	bzero(req, buf_size);
	n = read(sock, buf, buf_size - 1);
	
	if (n < 0)
		error("ERROR reading from socket");
	
	// print request to server
	printf("Here is the message: %s\n",buf);
	
	int spaces = 0;
	int b_index = 0;
	int r_index = 0;
	while (b_index < buf_size)
	{
		if (buf[b_index] == ' ')
			spaces++;
		if (spaces == 1)
		{
			if (buf[b_index] != '/' && buf[b_index] != ' ')
			{
				req[r_index] = buf[b_index];
				r_index++; // careful!
			}
		} else if (spaces == 2)
			break;
		b_index++;
	}
	// req should now have the filename of length r_index (NOT r_index+1!!!)
	req[r_index] = '\0';
	
	//printf("%s\n", req);
	
	char *file;
	struct stat st;
	int size;
	
	// get file size
	stat(req, &st);
	size = st.st_size;

	file = readFileBytes(req);
	
	//// Regular stuff
	char *headerTop = "HTTP/1.1 200 OK\nConnection: close\n";
	int headerTopLength = strlen(headerTop);
	
	//// Date Header
	char date[100];
	int dateLength;
	
	time_t now = time(0);
	struct tm tm = *gmtime(&now);
	strftime(date, sizeof date, "Date: %a, %d %b %Y %H:%M:%S %Z\n", &tm);
	dateLength = strlen(date);
	//printf("%s", date, dateLength);
	
	
	//// Server name header
	char *serverName = "Server: SanciangcoLung/1.0\n";
	int serverLength = strlen(serverName);
	//printf("%s", serverName);
	
	//// File modification date
	struct stat attrib;
    stat(req, &attrib);
    char modDate[100];
    strftime(modDate, sizeof modDate, "Last-Modified: %a, %d %m %y %H:%M:%S %Z\n", gmtime(&(attrib.st_ctime)));
    //printf("%s", modDate);
	int fileModLen = strlen(modDate);
	
	//// Content length
	char contentLength[64];
	sprintf(contentLength, "Content-Length: %i\n", size);
	//printf("%s", contentLength);
	int conLenLen = strlen(contentLength);
	
	//// Content type
	char *type;
	if (req[r_index - 1] == 'l')
			type = "text/html";
	else if (req[r_index - 1] == 'g')
			type = "image/jpeg";
	char contentType[32];
	sprintf(contentType, "Content-Type: %s\n\n", type); // 2 new lines since it's the end of the header
	int typeLen = strlen(contentType);
	
	int headerLength = headerTopLength + dateLength + serverLength + fileModLen + conLenLen + typeLen;
	
	//// Generate full header
	char *header;
	header = malloc(sizeof(char) * headerLength + size);
	sprintf(header, "%s%s%s%s%s%s", headerTop, date, serverName, modDate, contentLength, contentType);
	printf("%s", header);
	
	strcat(header, file);
	
	
	if (file == NULL)
	{
		error("ERROR file error");
		return;
	}
	n = write(sock, header, headerLength + size);
	if (n < 0)
		error("ERROR writing to socket");
	
}

char* readFileBytes(const char *name)
{
    FILE *fl = fopen(name, "r");
    fseek(fl, 0, SEEK_END);
    long len = ftell(fl);
    char *ret = malloc(len);
    fseek(fl, 0, SEEK_SET);
    fread(ret, 1, len, fl);
    fclose(fl);
    return ret;
}
