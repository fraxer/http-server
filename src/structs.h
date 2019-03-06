#ifndef __STRUCTS__
#define __STRUCTS__

using namespace std;

namespace server {

#define BUFSIZ32 16384

#define EPOLL_MAX_EVENTS 16

#define WORKERS 2

#define HEADBUF 2048

#define MAX_SIZE_FOR_CACHE 0 // 262144

#define DB_CONNECTION_TTL 3600 // seconds

#define MAX_UPLOAD_SIZE 110485760

#define CIPHER_LIST "TLS_AES_256_GCM_SHA384 TLS_CHACHA20_POLY1305_SHA256 TLS_AES_128_GCM_SHA256 TLS_AES_128_CCM_8_SHA256 TLS_AES_128_CCM_SHA256 ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256"

#define FULLCHAINFILE "/etc/letsencrypt/live/galleryhd.ru/fullchain.pem"

#define PRIVATEFILE "/etc/letsencrypt/live/galleryhd.ru/privkey.pem"

#define gettid() syscall(SYS_gettid)

#define PQ_MAX_CONNECTIONS 20  // In PostgreSQL, the one connection of 100 is provided to the superuser, 
                               // so the application must use on 1 less connection.

struct socket_t {
    short unsigned int port;
    int                socket;
    epoll_event        event;
};

struct pair_chars {
    char* key;
    char* value;
};

struct pair_char_int {
    char*  key;
    size_t length;
};

enum http_version {
    HTTP_1_0,
    HTTP_1_1
};

enum http_method {
    GET,
    POST,
    PUT,
    PATCH,
    HEAD,
    DELETE,
    OPTIONS
};

struct fl {
    unsigned int head_size;
    unsigned int body_size;
    char* head;
    char* body;
};

struct head {
    http_method method;
};

struct obj {

    atomic<bool>                      active;

    bool                              in_thread;

    bool                              is_longpoll;

    bool                              keep_alive;

    bool                              headers_parsed;

    bool                              cached;

    bool                              sslConnected;

    unsigned short                    port;

    short                             status;

    short                             redirect;

    http_method                       method;

    http_version                      version;

    unsigned int                      request_body_pos;

    unsigned int                      request_body_length; // from header 'content-length'

    unsigned int                      response_head_length;

    unsigned int                      response_body_pos;

    unsigned int                      response_body_length;

    unsigned int                      file_offset;

    int                               fd_file;

    int                               request_body;

    pthread_t*                        thread;

    SSL*                              ssl;

    chrono::system_clock::time_point  start_request_read;

    char*                             response_body;

    char*                             uri;

    char*                             path;

    char*                             query;

    char*                             ext;

    char*                             full_path;

    char*                             ip;

    void*                             handler;

    unordered_map<double, pair_chars>  headers;
};

struct rewrite_st {
    bool         stop;

    const char*  dest;
    const char*  location_error;
    const char*  rule_error;

    pcre*        location;
    pcre*        rule;

    short        redirect;
    int          location_erroffset;
    int          rule_erroffset;
};

struct conn_st {
    bool             acquired;
    PGconn*          conn;
    chrono::system_clock::time_point ttl;
};

struct mx_domain {
    int    ttl;
    char*  domain;
    vector<struct in_addr> ip_list;
};

struct mail_st {
    char* email;
    char* subject;
    char* body;
};

struct file_st {
    char*  name;
    char*  type;
    char*  body;
    unsigned long int size;
};

struct values_st {
    char* value;
    unordered_map<double, char*> fields;
};

struct polling_st {
    pthread_mutex_t* mutex;
    bool             active;
    int              epoll_fd;
    int              user_id;
    string*          uri;
    vector<string*>* data;
    obj*             p_obj;
};

union UN {
    unsigned long int value;
    struct {
        unsigned long int b0 : 1;
        unsigned long int b1 : 1;
        unsigned long int b2 : 1;
        unsigned long int b3 : 1;
        unsigned long int b4 : 1;
        unsigned long int b5 : 1;
        unsigned long int b6 : 1;
        unsigned long int b7 : 1;
    };
};

union unionPow {
    double d;
    int x[2];
};

typedef unordered_map<double, values_st*> t_headers;

typedef function<void(t_headers&, char*, unsigned long int&)> t_funvalues;

extern unordered_map<int, conn_st*> db_connection;

extern unordered_map<double, char*> mime_types;

extern vector<rewrite_st> rewrite;

extern size_t rewrite_size;

extern unordered_map<short, pair_char_int> http_status_text;

extern list<mail_st*> list_mails;

extern unordered_map<double, fl> cache_file;

extern unordered_map<double, fl> cache_page;

extern unordered_map<int, polling_st*>* polling_array;

extern char* base_path;

extern char* tmp_path;

extern char* password_salt;

extern unsigned int base_path_length;

extern pthread_mutex_t lock_controller_cache;

extern pthread_mutex_t lock_file;

extern pthread_mutex_t lock_mail;

extern pthread_cond_t  cond_mail;

extern pthread_mutex_t lock_list_conn;

extern pthread_cond_t  cond_list_conn;

extern pthread_mutex_t lock_poll;

extern pthread_cond_t  cond_poll;

extern conn_st* list_connections[PQ_MAX_CONNECTIONS];

void sigpipeHandler(int);

void init_OpenSSL();

void setup_server_ctx();

void* thread_func(void*);

void* thread_mail(void*);

void* thread_polling(void*);

} // namespace

#endif
