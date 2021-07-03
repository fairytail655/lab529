#ifndef _MY_HTTP_H
#define _MY_HTTP_H
 
#define MY_HTTP_DEFAULT_PORT 80
 
char * http_get(const char *url, int timeout_s);
char * http_post(const char *url,const char * post_str, int timeout_s);
 
#endif
