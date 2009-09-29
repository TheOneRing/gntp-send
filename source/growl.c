#ifdef _WIN32
#include <windows.h>
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#include <arpa/inet.h>
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "md5.h"
#include "tcp.h"


static const char hex_table[] = "0123456789ABCDEF";
static char* string_to_hex_alloc(const char* str, int len) {
	int n, l;
	char* tmp = (char*)malloc(len * 2 + 1);
	memset(tmp, 0, len * 2 + 1);
    for (l = 0, n = 0; l < len; l++) {
        tmp[n++] = hex_table[(str[l] & 0xF0) >> 4];
        tmp[n++] = hex_table[str[l] & 0x0F];
    }
    return tmp;
}

char* gen_salt_alloc(int count) {
	char* salt = (char*)malloc(count + 1);
	int n;
	for (n = 0; n < count; n++) salt[n] = (((int)rand()) % 255) + 1;
	salt[n] = 0;
	return salt;
}

char* gen_password_hash_alloc(const char* password, const char* salt) {
	md5_context md5ctx;
	char md5tmp[20];
	char* md5digest;

	memset(md5tmp, 0, sizeof(md5tmp));
	md5_starts(&md5ctx);
	md5_update(&md5ctx, (uint8_t*)password, strlen(password));
	md5_update(&md5ctx, (uint8_t*)salt, strlen(salt));
	md5_finish(&md5ctx, (uint8_t*)md5tmp);

	md5_starts(&md5ctx);
	md5_update(&md5ctx, (uint8_t*)md5tmp, 16);
	md5_finish(&md5ctx, (uint8_t*)md5tmp);
	md5digest = string_to_hex_alloc(md5tmp, 16);

	return md5digest;
}

EXPORT
int growl( const char *const server,const char *const appname,const char *const notify,const char *const title, const char *const message ,
                                const char *const icon , const char *const password , const char *url )
{		
	int sock = -1;
	char* salt;
        char* salthash;
        char* keyhash;
		char* authheader = NULL;

	if (password) {
		srand(time(NULL));
		salt = gen_salt_alloc(8);
		keyhash = gen_password_hash_alloc(password, salt);
		salthash = string_to_hex_alloc(salt, 8);
		free(salt);
		authheader = (char*)malloc(strlen(keyhash) + strlen(salthash) + 7);
		sprintf(authheader, " MD5:%s.%s", keyhash, salthash);
		free(salthash);
	}

	sock = growl_tcp_open(server);
	if (sock == -1) goto leave;
    
	growl_tcp_write(sock, "GNTP/1.0 REGISTER NONE %s", authheader);
	growl_tcp_write(sock, "Application-Name: %s ", appname);
	growl_tcp_write(sock, "Notifications-Count: 1" );
	growl_tcp_write(sock, "" );
	growl_tcp_write(sock, "Notification-Name: %s", notify);
	growl_tcp_write(sock, "Notification-Display-Name: %s", notify);
	growl_tcp_write(sock, "Notification-Enabled: True" );
	growl_tcp_write(sock, "" );
	while (1) {
		char* line = growl_tcp_read(sock);
		int len = strlen(line);
		/* fprintf(stderr, "%s\n", line); */
		if (strncmp(line, "GNTP/1.0 -ERROR", 15) == 0) {
			fprintf(stderr, "failed to register notification\n");
			free(line);
			goto leave;
		}
		free(line);
		if (len == 0) break;
	}
	growl_tcp_close(sock);

	sock = growl_tcp_open(server);
	if (sock == -1) goto leave;

	growl_tcp_write(sock, "GNTP/1.0 NOTIFY NONE %s", authheader);
	growl_tcp_write(sock, "Application-Name: %s", appname);
	growl_tcp_write(sock, "Notification-Name: %s", notify);
	growl_tcp_write(sock, "Notification-Title: %s", title);
	growl_tcp_write(sock, "Notification-Text: %s", message);
	if (icon) growl_tcp_write(sock, "Notification-Icon: %s", icon);
	if (url) growl_tcp_write(sock, "Notification-Callback-Target: %s", url  );

	growl_tcp_write(sock, "");
	while (1) {
		char* line = growl_tcp_read(sock);
		int len = strlen(line);
		/* fprintf(stderr, "%s\n", line); */
		if (strncmp(line, "GNTP/1.0 -ERROR", 15) == 0) {
			fprintf(stderr, "failed to post notification\n");
			free(line);
			goto leave;
		}
		free(line);
		if (len == 0) break;
	}
	growl_tcp_close(sock);
	sock = 0;

leave:
	if (authheader) free(authheader);

	return (sock == 0) ? 0 : -1;
}

void growl_append_md5( unsigned char *const data , const int data_length , const char *const password )
{
	md5_context md5ctx;
	char md5tmp[20];

	memset(md5tmp, 0, sizeof(md5tmp));
	md5_starts(&md5ctx);
	md5_update(&md5ctx, (uint8_t*)data, data_length );
	md5_update(&md5ctx, (uint8_t*)password, strlen(password));
	md5_finish(&md5ctx, (uint8_t*)md5tmp);

	memcpy( data + data_length , md5tmp , 16 );
}

int growl_udp_register( const char *const server , const char *const appname , const char *const notify , const char *const password  )
{
	int register_header_length = 26+strlen(appname)+strlen(notify);
	unsigned char *data = malloc(register_header_length);
	int pointer = 0;
	int rc = 0;

	uint8_t GROWL_PROTOCOL_VERSION  = 1;
	uint8_t GROWL_TYPE_REGISTRATION = 0;

	uint16_t appname_length = ntohs(strlen(appname));
	uint16_t notify_length = ntohs(strlen(notify));
	uint8_t notifications_count = 1;
	uint8_t default_notifications_count = 1;
	
	memset( data , 0 ,  register_header_length );


	pointer = 0;
	memcpy( data + pointer , &GROWL_PROTOCOL_VERSION , 1 );	
	pointer++;
	memcpy( data + pointer , &GROWL_TYPE_REGISTRATION , 1 );
	pointer++;
	memcpy( data + pointer , &appname_length , 2 );	
	pointer += 2;
	memcpy( data + pointer , &notifications_count , 1 );	
	pointer++;
	memcpy( data + pointer, &default_notifications_count , 1 );	
	pointer++;
	sprintf( data + pointer , "%s" , appname );
	pointer += strlen(appname);

	memcpy( data + pointer, &notify_length , 2 );		
	pointer +=2;
	sprintf( data + pointer , "%s" , notify );
	pointer += strlen(notify);
	 
	memcpy( data , &notifications_count , 1 );
	pointer++;


	growl_append_md5( data , pointer , password );
	pointer += 16;

	rc = growl_tcp_datagram( server , data , pointer );
	free(data);
	return rc;
}


int growl_udp_notify( const char *const server,const char *const appname,const char *const notify,const char *const title, const char *const message ,
                                const char *const password )
{
	int notify_header_length = 28 + strlen(appname)+strlen(notify)+strlen(message)+strlen(title);
	unsigned char *data = malloc(notify_header_length);
	int pointer = 0;
	int rc = 0;

	uint8_t GROWL_PROTOCOL_VERSION  = 1;
	uint8_t GROWL_TYPE_NOTIFICATION = 1;

	uint16_t flags = ntohs(0);
	uint16_t appname_length = ntohs(strlen(appname));
	uint16_t notify_length = ntohs(strlen(notify));
	uint16_t title_length = ntohs(strlen(title));
	uint16_t message_length = ntohs(strlen(message));


	memset( data , 0 ,  notify_header_length );
	
	pointer = 0;
	memcpy( data + pointer , &GROWL_PROTOCOL_VERSION , 1 );	
	pointer++;
	memcpy( data + pointer , &GROWL_TYPE_NOTIFICATION , 1 );
	pointer++;
	memcpy( data + pointer , &flags , 2 );
	pointer += 2;
	memcpy( data + pointer , &notify_length , 2 );	
	pointer += 2;
	memcpy( data + pointer , &title_length , 2 );	
	pointer += 2;
	memcpy( data + pointer , &message_length , 2 );	
	pointer += 2;
	memcpy( data + pointer , &appname_length , 2 );	
	pointer += 2;
	sprintf( data + pointer , notify );
	pointer += strlen(notify);
	sprintf( data + pointer , title );
	pointer += strlen(title);
	sprintf( data + pointer , message );
	pointer += strlen(message);
	sprintf( data + pointer , appname );
	pointer += strlen(appname);


	growl_append_md5( data , pointer , password );
	pointer += 16;


	rc = growl_tcp_datagram( server , data , pointer );
	free(data);
	return rc;
}

EXPORT
int growl_udp( const char *const server,const char *const appname,const char *const notify,const char *const title, const char *const message ,
                                const char *const icon , const char *const password , const char *url )
{
	int rc = growl_udp_register(  server ,  appname ,  notify , password  );
	printf( "rc = %d\n" , rc );
	if( rc == 0 )
	{
		rc = growl_udp_notify( server, appname, notify, title,  message , password );
	}
	return rc;
}


#ifdef _WIN32
EXPORT
void GrowlNotify(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow) {
	char* server = "127.0.0.1:23053";
	char* password = NULL;
	char* appname = "gntp-send";
	char* notify = "gntp-send notify";
	char* title = NULL;
	char* message = NULL;
	char* icon = NULL;
	char* url = NULL;
	char* first = strdup(lpszCmdLine);
	char* ptr = first;
	int rc;
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) return;
	#define SKIP(x)	while (*x && *x != ' ') x++; if (*x == ' ') *x++ = 0;
	server = ptr;  SKIP(ptr);
	appname = ptr; SKIP(ptr);
	notify = ptr;  SKIP(ptr);
	title = ptr;   SKIP(ptr);
	message = ptr; SKIP(ptr);
	icon = ptr;    SKIP(ptr);
	url = ptr;     SKIP(ptr);
	rc = growl(server,appname,notify,title,message,icon,password,url);
	WSACleanup();
	free(ptr);
}
#endif