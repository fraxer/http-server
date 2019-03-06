#include "headers.h"
#include "structs.h"
#include "functions/functions.h"
#include "dkim/dkim.h"
#include "controller/controller.h"
#include "model/model.h"
#include "view/view.h"
#include "mail/mail.h"
#include "json/json.h"
#include "polling/polling.h"
// pages

#include "pages/test/controller.h"

using namespace std;

namespace server {

DH *dh2048 = nullptr;
DH *dh1024 = nullptr;

SSL_CTX* ctx;

pthread_mutex_t                         lock_objs = PTHREAD_MUTEX_INITIALIZER;

void init_OpenSSL() {

    if(!SSL_library_init()) {
        fprintf(stderr, "OpenSSL initialization failed!\n");
        exit(-1);
    }

    SSL_load_error_strings();
}

void init_dhparams(void) {

    BIO *bio;

    bio = BIO_new_file("/sda1/server/required/dh2048.pem", "r");

    if(!bio) printf("Error opening file dh2048.pem\n");

    dh2048 = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);

    if (!dh2048) printf("Error reading DH parameters from dh2048.pem\n");

    BIO_free(bio);

    bio = BIO_new_file("/sda1/server/required/dh1024.pem", "r");

    if (!bio) printf("Error opening file dh1024.pem\n");

    dh1024 = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);

    if (!dh1024) printf("Error reading DH parameters from dh1024.pem\n");

    BIO_free(bio);
}

DH *tmp_dh_callback(SSL *ssl, int is_export, int keylength) {

    DH *ret;

    if (!dh2048 || !dh1024) init_dhparams();

    switch (keylength) {

        case 1024:
            ret = dh1024;
            break;

        case 2048:
        default:
            ret = dh2048;
            break;
    }

    return ret;
}

void setup_server_ctx() {

    ctx = SSL_CTX_new(TLS_method());

    SSL_CTX_set_mode(ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);

    // if(SSL_CTX_use_certificate_file(ctx, CERTFILE, SSL_FILETYPE_PEM) != 1)
    //     printf("Error loading certificate from file\n");

    if(SSL_CTX_use_certificate_chain_file(ctx, FULLCHAINFILE) != 1)
        printf("Error loading certificate fullchain from file\n");
    
    if(SSL_CTX_use_PrivateKey_file(ctx, PRIVATEFILE, SSL_FILETYPE_PEM) != 1)
        printf("Error loading private key from file\n");

    if(!SSL_CTX_check_private_key(ctx))
        printf("SSL_CTX_check_private_key failed\n");

    SSL_CTX_set_options(ctx, SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_RENEGOTIATION);

    SSL_CTX_set_tmp_dh_callback(ctx, tmp_dh_callback);

    if (SSL_CTX_set_cipher_list(ctx, CIPHER_LIST) != 1)
        printf("Error setting cipher list (no valid ciphers)\n");
}

void enable_keepalive(int& socketfd) {

    int kenable = 1;
    setsockopt(socketfd, SOL_SOCKET, SO_KEEPALIVE, &kenable, sizeof(kenable));

    // Максимальное количество контрольных зондов
    int keepcnt = 3;
    setsockopt(socketfd, SOL_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt));

    // Время (в секундах) соединение должно оставаться бездействующим
    // до того, как TCP начнет отправлять контрольные зонды
    int keepidle = 40;
    setsockopt(socketfd, SOL_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle));

    // Время (в секундах) между отдельными зондами
    int keepintvl = 5;
    setsockopt(socketfd, SOL_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl));
}

int create_and_bind(const char* ip, short unsigned int port) {

    sockaddr_in sa;

    int socketfd;

    bzero((void*)&sa, sizeof(sa));

    sa.sin_family      = AF_INET;
    sa.sin_port        = htons(port);
    sa.sin_addr.s_addr = inet_addr(ip);

    socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(socketfd == -1){
        printf("socket error\n");
        return 0;
    }

    int enableaddr = 1;
    setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &enableaddr, sizeof(enableaddr));

    int enableport = 1;
    setsockopt(socketfd, SOL_SOCKET, SO_REUSEPORT, &enableport, sizeof(enableport));

    int enablecpu = 1;
    setsockopt(socketfd, SOL_SOCKET, SO_INCOMING_CPU, &enablecpu, sizeof(enablecpu));

    int defer_accept = 1;
    setsockopt(socketfd, SOL_SOCKET, TCP_DEFER_ACCEPT, &defer_accept, sizeof(defer_accept));

    int nodelay = 1;
    setsockopt(socketfd, SOL_SOCKET, TCP_NODELAY, &nodelay, sizeof(nodelay));

    if( bind(socketfd, (sockaddr*)&sa, sizeof(sa)) < 0 ) {
        printf("bind error\n");
        return 0;
    }

    return socketfd;
}

bool make_socket_nonblocking(int& socketfd) {

    int flags = fcntl(socketfd, F_GETFL, 0);

    if(flags == -1) {
        printf("[E] fcntl failed (F_GETFL)\n");
        return false;
    }

    flags |= O_NONBLOCK;
    // flags |= O_ASYNC;
    flags |= O_NOATIME;

    int s = fcntl(socketfd, F_SETFL, flags);

    // fcntl (socketfd, F_SETOWN, gettid());

    if(s == -1) {
        printf("[E] fcntl failed (F_SETFL)\n");
        return false;
    }

    return true;
}

bool accept_connection(socket_t* socket_st, unordered_map<int, obj*>* objs, int& epollfd) {

    struct sockaddr in_addr;

    socklen_t in_len = sizeof(in_addr);

    int infd = accept(socket_st->socket, &in_addr, &in_len);

    if(infd == -1) {

        if(errno == EAGAIN || errno == EWOULDBLOCK) { // Done processing incoming connections 
            printf("[E] accept EAGAIN\n");
            return false;
        }
        else {
            printf("[E] accept failed, %d\n", errno);
            return false;
        }
    }
    // printf("%d, %d\n", infd, errno);


    struct sockaddr_in addr;

    socklen_t addr_size = sizeof(struct sockaddr_in);

    getpeername(infd, (struct sockaddr *)&addr, &addr_size);

    // cout << inet_ntoa(addr.sin_addr) << ", " << addr.sin_addr.s_addr << ", " << inet_addr("109.194.35.137") << endl;



    enable_keepalive(infd);

    if(!make_socket_nonblocking(infd)) {
        printf("[E] make_socket_nonblocking failed\n");
        return false;
    }

    obj* p_obj;

    pthread_mutex_lock(&lock_objs);
    unordered_map<int, obj*>::iterator it = objs->find(infd);
    if (it != objs->end()) {
        pthread_mutex_unlock(&lock_objs);
        p_obj = (it->second);
    } else {
        p_obj = objs->emplace(infd, new obj()).first->second;
        pthread_mutex_unlock(&lock_objs);
    }
    p_obj->port      = socket_st->port;
    p_obj->ip        = (char*)inet_ntoa(addr.sin_addr);
    p_obj->in_thread = false;
    p_obj->handler   = nullptr;
    p_obj->thread    = nullptr;
    // printf("port %d\n", p_obj->port);

    socket_st->event.data.fd  = infd;
    socket_st->event.events   = EPOLLIN;

    // if(addr.sin_addr.s_addr == inet_addr("109.194.35.137")) {
    //     socket_st->event.events   = EPOLLOUT;
    //     p_obj->status = 403;
    // }

    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, infd, &socket_st->event) == -1) {
        printf("[E] epoll_ctl failed accept, %d\n", errno);
        return false;
    }

    return true;
}

void setHandler(obj* p_obj, Controller** handler, const int socketfd, const int& epollfd) {

    if(strcmp(p_obj->path, "/test") == 0) {
        switch (p_obj->method) {
            case GET:
                *handler = new TestController(p_obj, &TestController::actionIndex);
                break;
            default:
                *handler = new Controller(p_obj, (char*(*)(void*))&Controller::methodNotAllwed);
        }
    }
}

void clearConnectionObject(obj* ob, epoll_event* ev) {
    
    ob->request_body_length  = 0;
    ob->request_body_pos     = 0;
    ob->response_head_length = 0;
    ob->response_body_pos    = 0;
    ob->response_body_length = 0;
    ob->file_offset          = 0;
    ob->redirect             = 0;
    ob->status               = 0;
    ob->headers_parsed       = false;
    ob->is_longpoll          = false;

    unordered_map<int, polling_st*>::iterator it_p;

    pthread_mutex_lock(&lock_poll);

    it_p = polling_array->find((ev->data.fd));

    pthread_mutex_unlock(&lock_poll);

    if (it_p != polling_array->end()) {
        if (it_p->second->active) {
            Polling::freePoll(it_p->second);
        }
    }

    

    // printf("clear %d\n", ev->data.fd);
    
    if(ob->fd_file > 0) {
        close(ob->fd_file);
        ob->fd_file = -1;
    }

    for (unordered_map<double, pair_chars>::iterator it = ob->headers.begin(); it != ob->headers.end(); ++it) {
        // cout << it->first << " => " << it->second.key << " => " << it->second.value << "\n\n";
        free(it->second.key);
        free(it->second.value);
    }

    ob->headers.clear();

    if(ob->uri != nullptr) {
        free(ob->uri);
        ob->uri = nullptr;
    }
    if(ob->path != nullptr) {
        free(ob->path);
        ob->path = nullptr;
    }
    if(ob->query != nullptr) {
        free(ob->query);
        ob->query = nullptr;
    }
    if(ob->ext != nullptr) {
    	bzero(ob->ext, 16);
        free(ob->ext);
        ob->ext = nullptr;
    }

    if(ob->full_path != nullptr) {
        free(ob->full_path);
        ob->full_path = nullptr;
    }

    if(ob->request_body > 0) {
        ftruncate(ob->request_body, 0);
        close(ob->request_body);
        ob->request_body = -1;
    }

    if(ob->response_body != nullptr) {
        if(!ob->cached) {
            free(ob->response_body);
        }
        ob->response_body = nullptr;
    }

    // if(ob->in_thread && ob->thread != nullptr) {
    //     // pthread_cancel(*(ob->thread));
    //     int r = pthread_cancel(*(ob->thread));
    //     printf("cancel thread: %d, %d\n", r, ESRCH);
    //     // delete ob->thread;
    //     // ob->thread = nullptr;
    // }

    if(ob->handler != nullptr) {
        // ((Controller*)ob->handler)->freePageMemory();

        delete (Controller*)ob->handler;
        ob->handler = nullptr;
    }

    

    ob->in_thread = false;
    ob->cached    = false;

    if(!ob->keep_alive && ob->ssl != nullptr && ob->port == 443) {
        (SSL_get_shutdown(ob->ssl) & SSL_RECEIVED_SHUTDOWN) ? SSL_shutdown(ob->ssl) : SSL_clear(ob->ssl);

        SSL_free(ob->ssl);
        ob->ssl = nullptr;
        ob->sslConnected = false;
    }

    ob->keep_alive = false;

    // printf("освобождаем память\n");
}

bool handleHandshake(obj* p_obj, epoll_event* ev, int& epollfd) {

    int r = 0;

    if(p_obj->ssl == nullptr) {

        p_obj->ssl = SSL_new(ctx);

        if(p_obj->ssl == nullptr) {

            printf("SSL_new failed\n");

            goto epoll_ssl_error;
        }

        if(!SSL_set_fd(p_obj->ssl, ev->data.fd)) {

            printf("SSL_set_fd failed\n");

            goto epoll_ssl_error;
        }

        SSL_set_accept_state(p_obj->ssl);
    }
    // else if (p_obj->sslAllocated) {

    // }

    r = SSL_do_handshake(p_obj->ssl);

    if(r == 1) {

        p_obj->sslConnected = true;

        return true;
    }

    switch(SSL_get_error(p_obj->ssl, r)) {

        case SSL_ERROR_SYSCALL:
        case SSL_ERROR_SSL:

            // printf("SSL_ERROR_SYSCALL11, errno %d\n", errno);

            epoll_ssl_error:

            if(epoll_ctl(epollfd, EPOLL_CTL_DEL, ev->data.fd, NULL) == -1) {
		        printf("[E] epoll_ctl failed del, %d\n", errno);
		    }

            clearConnectionObject(p_obj, ev);

            close(ev->data.fd);

            return false;

        case SSL_ERROR_WANT_ACCEPT:
            // printf("SSL_ERROR_WANT_ACCEPT\n");
            return false;

        case SSL_ERROR_WANT_CONNECT:
            // printf("SSL_ERROR_WANT_CONNECT\n");
            return false;

        case SSL_ERROR_ZERO_RETURN:
            // printf("SSL_ERROR_ZERO_RETURN\n");
            return false;
    }

    return false;
}


int ssl_get_error(SSL* s, int res) {

    int err = SSL_get_error(s, res);

    switch(err) {

        case SSL_ERROR_WANT_WRITE:
            return 1;

        case SSL_ERROR_WANT_READ:
            return 1;

        case SSL_ERROR_SYSCALL:
        case SSL_ERROR_SSL:
            // printf("SSL_ERROR_SYSCALL11, errno %d\n", errno);
            return 3;

        case SSL_ERROR_WANT_ACCEPT:
            // printf("SSL_ERROR_WANT_ACCEPT\n");
            return 4;

        case SSL_ERROR_WANT_CONNECT:
            // printf("SSL_ERROR_WANT_CONNECT\n");
            return 5;

        case SSL_ERROR_ZERO_RETURN:
            // printf("SSL_ERROR_ZERO_RETURN\n");
            return 6;

        default:
            // printf("SSL_ERROR %d\n", err);
            return 0;
    }
}

ssize_t c_read(obj* p_obj, const int& fd, char* buf, const size_t& count) {

    if(p_obj->port == 443) {
        return SSL_read(p_obj->ssl, buf, count);
    }
    else if(p_obj->port == 80) {
        return read(fd, buf, count);
    }

    return 0;
}

void handleDataRead(obj* p_obj, epoll_event* ev, int& epollfd, char* buf) {

    // printf("read head %d\n", ev->data.fd);

    int readed = 0;

    while((readed = c_read(p_obj, ev->data.fd, buf, BUFSIZ32))) {

        // printf("readed %d, errno %d, fd %d\n", readed, errno, ev->data.fd);

        if(readed > 0) {

            // char _num[16];

            // string str_fd = itoa(ev->data.fd, &_num[0]);

            // str_fd += ".txt";

            // ofstream myfile;
            // myfile.open(str_fd.c_str(), ios::app);

            // if (myfile.is_open()) {
            //     // myfile.write(p_obj->ip, strlen(p_obj->ip));
            //     // myfile.write("\n", 1);
            //     myfile.write(buf, readed);
            //     // myfile.write("\n----------------\n", 18);
            //     myfile.close();
            // }

            if(!p_obj->headers_parsed) {

                char* p = buf;

                int pos = 0;

                p_obj->start_request_read = chrono::system_clock::now();

                p_obj->request_body_pos = 0;

                if((p = setMethod(p_obj, p, pos)) == nullptr) {
                    return;
                }

                if((p = setURI(p_obj, p, pos)) == nullptr) {
                    goto epoll_headers_error;
                }

                if((p = setHttpVersion(p_obj, p, pos)) == nullptr) goto epoll_headers_error;

                if(!setHeaders(p_obj, p, pos)) goto epoll_headers_error;

                p_obj->headers_parsed = true;

                // printf("pos: %d\n", pos);

                // printf("заголовки прочитаны %d, thread: %d, fd: %d\n", p_obj->request_body_length, gettid(), ev->data.fd);

                if(p_obj->request_body_length == 0) {
                    ev->events = EPOLLOUT;
                }
                else {

                    char path[32];
                    char num[32];

                    strcpy(path, tmp_path);
                    strcat(path, itoa(ev->data.fd, &num[0]));

                    // printf("%s\n", path);

                    p_obj->request_body = open(path, O_CREAT | O_NOATIME | O_RDWR, S_IRUSR | S_IWUSR);

                    // printf("fd %d, %d\n", p_obj->request_body, errno);

                    if(p_obj->request_body == -1) {
                        printf("Ошибка создания временного файла 1, %d\n", errno);
                        goto epoll_headers_error;
                    }

                    p_obj->request_body_pos = readed - pos;

                    if(p_obj->request_body_pos > 0) {

                        int wr = write(p_obj->request_body, buf + pos, readed - pos);

                        if(wr < readed - pos) {
                            printf("Ошибка записи во временный файл 2, %d\n", errno);
                            goto epoll_headers_error;
                        }
                    }
                }
            }
            else if(p_obj->request_body_length > 0 && (p_obj->method == POST || p_obj->method == PATCH || p_obj->method == PUT)) {

                // printf("считывание файла %d/%d, file_fd: %d, thread: %d, fd: %d\n", p_obj->request_body_pos + readed, p_obj->request_body_length, p_obj->request_body, gettid(), ev->data.fd);

                if(chrono::duration_cast<chrono::duration<int>>(chrono::system_clock::now() - p_obj->start_request_read).count() > 5) {
                    printf("время принятия тела запроса истекло\n");
                    goto epoll_headers_error;
                }

                p_obj->start_request_read = chrono::system_clock::now();

                if(p_obj->request_body_pos + readed > MAX_UPLOAD_SIZE) {
                    printf("превышен максимальный размер загружаемого файла\n");
                    goto epoll_headers_error;
                }

                int wr = write(p_obj->request_body, buf, readed);

                if(wr < readed) {
                    printf("Ошибка записи во временный файл 3, %d, %d/%d\n", errno, wr, readed);
                    goto epoll_headers_error;
                }

                p_obj->request_body_pos += readed;

                if(p_obj->request_body_pos < p_obj->request_body_length)
                    ev->events = EPOLLIN;
                else {
                    ev->events = EPOLLOUT;
                }
            }
            else {
                goto epoll_headers_error;
            }

            if(epoll_ctl(epollfd, EPOLL_CTL_MOD, ev->data.fd, ev) == -1) {
                printf("[E] epoll_ctl failed in read, %d\n", errno);
            }
        }
        else if(readed < 0) {

            if(errno == 0) goto epoll_headers_error;

            if(p_obj->request_body_length > 0 && p_obj->request_body_pos >= p_obj->request_body_length) {

                ev->events = EPOLLOUT;

                if(epoll_ctl(epollfd, EPOLL_CTL_MOD, ev->data.fd, ev) == -1) {
                    printf("[E] epoll_ctl failed in read done, %d\n", errno);
                }
            }

            return;
        }
    }

    if(readed == 0) {

        epoll_headers_error:

        // printf("%d соединение разорвано клиентом %d, %d\n", gettid(),  ev->data.fd, errno);

        p_obj->keep_alive = false;

        if(epoll_ctl(epollfd, EPOLL_CTL_DEL, ev->data.fd, NULL) == -1) {
	        printf("[E] epoll_ctl failed del, %d\n", errno);
	    }

        clearConnectionObject(p_obj, ev);

        close(ev->data.fd);
    }
}

void handleDataWrite(obj* p_obj, epoll_event* ev, int& epollfd) {

    int sended = 0;

    int r = 0;

    // printf("fd %d, %d\n", ev->data.fd, p_obj->port);

    // наполняем тело
    if(p_obj->response_body_length == 0) {

        // printf("is head wr\n");

        if(p_obj->request_body > 0) lseek(p_obj->request_body, 0, SEEK_SET);

        p_obj->response_body_pos = 0;

        if(p_obj->status >= 400) {
            if(standardResponse(p_obj) == nullptr) goto epoll_write_error;

            goto epoll_write;
        }

        short status_code = 200;

        if(p_obj->redirect != 0) {

            if(p_obj->redirect == 301) {
                status_code = 301;
            }
            else if(p_obj->redirect == 302) {
                status_code = 302;
            }

            p_obj->response_body = (char*)malloc(HEADBUF);

            if(p_obj->response_body == nullptr) goto epoll_write_error;

            unsigned int headers_pos = 37;

            setHeader(p_obj->response_body, headers_pos, "Cache-Control",     "no-cache",    13,   8);
            setHeader(p_obj->response_body, headers_pos, "Connection",        "keep-alive",  10,  10);
            setHeader(p_obj->response_body, headers_pos, "Content-Length",    "0",           14,   1);

            char buffer[HEADBUF];

            strcpy(&buffer[0],  "https://galleryhd.ru");
            strcat(&buffer[19], p_obj->uri);

            setHeader(p_obj->response_body, headers_pos, "Location", &buffer[0], 8);

            pair_char_int status = http_status_text[status_code];

            writeHeadersInHead(p_obj, status, headers_pos, 0);

            goto epoll_write;
        }

        // printf("bool %d\n", p_obj->in_thread);

        if (p_obj->in_thread) {

            p_obj->response_body = ((Controller*)p_obj->handler)->getContentWrapper(true);

            delete (Controller*)p_obj->handler;

            p_obj->handler = nullptr;

            p_obj->in_thread = false;

            if(p_obj->response_body == nullptr) {
                p_obj->status = 500;
                if(standardResponse(p_obj) == nullptr) goto epoll_write_error;
            }

            goto epoll_write;
        }

        Controller* handler = nullptr;

        setHandler(p_obj, &handler, ev->data.fd, epollfd);

        // если обычный запрос
        if(handler != nullptr) {

            p_obj->status = status_code;

            if (p_obj->in_thread) {
                p_obj->handler = handler;

                ev->events = EPOLLONESHOT;
                if(epoll_ctl(epollfd, EPOLL_CTL_MOD, ev->data.fd, ev) == -1) {
                    printf("[E] epoll_ctl failed in write oneshot, %d\n", errno);
                }

                handler->runHandler();
                
                return;

            }
            // else if (p_obj->is_longpoll) {

            //     bool b = handler->runPollingHandler();

            //     delete handler;

            //     if (b) {

            //         ev->events = EPOLLONESHOT;
            //         if(epoll_ctl(epollfd, EPOLL_CTL_MOD, ev->data.fd, ev) == -1) {
            //             printf("[E] epoll_ctl failed in write oneshot, %d\n", errno);
            //         }

            //         return;

            //     } else {

            //         if(p_obj->response_body == nullptr) {
            //             p_obj->status = 500;
            //             if(standardResponse(p_obj) == nullptr) goto epoll_write_error;
            //         }
            //     }

            // }
            else {

                p_obj->response_body = handler->getContentWrapper();

                delete handler;

                if(p_obj->response_body == nullptr) {
                    p_obj->status = 500;
                    if(standardResponse(p_obj) == nullptr) goto epoll_write_error;
                }
            }
        }
        // если это файл 
        else if(handler == nullptr) {

            unordered_map<double, char*>::const_iterator it = mime_types.find(simpleHash(p_obj->ext));
            
            if(it == mime_types.end()) {
                it = mime_types.find(simpleHash("txt"));
            }

            double hash_filename = simpleHash(p_obj->path);

            pthread_mutex_lock(&lock_file);

            unordered_map<double, fl>::const_iterator it_file = cache_file.find(hash_filename);

            // если не в кеше
            if(it_file == cache_file.end()) {

                unsigned int path_length = strlen(p_obj->path) + 1; // + \0

                if(p_obj->full_path == nullptr)
                   p_obj->full_path = (char*)malloc(base_path_length + path_length + 1);

                if(p_obj->full_path == nullptr) goto epoll_write_error;

                memcpy(p_obj->full_path, base_path, base_path_length);
                memcpy(p_obj->full_path + base_path_length, p_obj->path, path_length);

                unsigned int file_size = 0;

                unsigned int file_part_size = 0;

                p_obj->fd_file = open(p_obj->full_path, O_RDONLY);

                if(p_obj->fd_file == -1) {

                    pthread_mutex_unlock(&lock_file);

                    p_obj->status = 404;

                    if(standardResponse(p_obj) == nullptr) goto epoll_write_error;

                    goto epoll_write;
                }
                else {
                    file_size = (unsigned int)lseek(p_obj->fd_file, 0, SEEK_END);

                    lseek(p_obj->fd_file, 0, SEEK_SET);

                    file_part_size = file_size;

                    if(file_size > MAX_SIZE_FOR_CACHE) {
                        // printf("mutex unlock (file_size > MAX_SIZE_FOR_CACHE) %d\n", gettid());
                        pthread_mutex_unlock(&lock_file);
                    }
                }

                // наполняем голову
                char* content_length = (char*)malloc(32);

                if(content_length == nullptr) goto epoll_write_error;

                content_length = itoa(file_size, content_length);

                unsigned int headers_pos = 37;

                p_obj->response_body = (char*)malloc(BUFSIZ32);

                if(p_obj->response_body == nullptr) {
                    free(content_length);
                    goto epoll_write_error;
                }

                unordered_map<double, pair_chars>::const_iterator it_h = p_obj->headers.find(simpleHash("range"));

                if(it_h != p_obj->headers.end()) {

                    status_code = 206;

                    short res = processingHeaderRange(it_h->second.value, file_size, file_part_size, content_length, p_obj, headers_pos);

                    if(res == -1) {
                        free(content_length);

                        p_obj->status = 500;

                        if(standardResponse(p_obj) == nullptr) goto epoll_write_error;

                        goto epoll_write;
                    }
                    else if(res == -2) {
                        free(content_length);

                        p_obj->status = 416;

                        if(standardResponse(p_obj) == nullptr) goto epoll_write_error;

                        goto epoll_write;
                    }
                }
                else {
                    setHeader(p_obj->response_body, headers_pos, "Content-Length", content_length,              14);
                    setHeader(p_obj->response_body, headers_pos, "Cache-Control",  "public, max-age=31536000",  13,   24);
                }

                free(content_length);

                struct stat stat_obj;

                stat(p_obj->full_path, &stat_obj);

                char* last_modified = (char*)malloc(30);

                if(last_modified == nullptr) {
                    goto epoll_write_error;
                }

                strftime(last_modified, 30, "%a, %d %b %Y %H:%M:%S GMT", localtime(&stat_obj.st_mtime));

                setHeader(p_obj->response_body, headers_pos, "Last-Modified", last_modified,                    13);
                setHeader(p_obj->response_body, headers_pos, "Connection",    "keep-alive",                     10,   10);
                setHeader(p_obj->response_body, headers_pos, "Expires",       "Sun, 30 Apr 2019 12:28:42 GMT",   7,   29);
                setHeader(p_obj->response_body, headers_pos, "Content-Type",  it->second,                       12);

                free(last_modified);

                pair_char_int status = http_status_text[status_code];

                writeHeadersInHead(p_obj, status, headers_pos, file_part_size);

                if(file_size <= MAX_SIZE_FOR_CACHE) {
                    char* res = (char*)realloc(p_obj->response_body, p_obj->response_body_length);

                    if (res == nullptr) {
                        free(p_obj->response_body);
                        p_obj->response_body = nullptr;
                    } else {
                        p_obj->response_body = res;
                    }
                }

                if(p_obj->response_body == nullptr) goto epoll_write_error;

                // наполняем тело
                

                if(p_obj->fd_file == -1) {
                    pthread_mutex_unlock(&lock_file);

                    p_obj->status = 404;

                    if(standardResponse(p_obj) == nullptr) goto epoll_write_error;
                }
                else {

                    if(file_size <= MAX_SIZE_FOR_CACHE && status_code == 200) { // video should not be cached

                        read(p_obj->fd_file, p_obj->response_body + p_obj->response_head_length, file_size);

                        cache_file[hash_filename] = {p_obj->response_head_length, file_size, p_obj->response_body, p_obj->response_body + p_obj->response_head_length};

                        pthread_mutex_unlock(&lock_file);

                        p_obj->cached = true;

                        close(p_obj->fd_file);

                        p_obj->fd_file = -1;
                    }
                    else {

                        if(p_obj->file_offset > 0) lseek(p_obj->fd_file, p_obj->file_offset, SEEK_SET);

                        r = read(p_obj->fd_file, p_obj->response_body + p_obj->response_head_length, BUFSIZ32 - p_obj->response_head_length);

                        r += p_obj->response_head_length;
                    }
                }
            }
            else {
                pthread_mutex_unlock(&lock_file);

                p_obj->response_body = it_file->second.head;

                p_obj->response_body_length = it_file->second.head_size + it_file->second.body_size;

                p_obj->cached = true;
            }

        }
        else {
            goto epoll_write_error;
        }
    }

    epoll_write:

    if(p_obj->fd_file > 0) {

        if(p_obj->response_body_pos > 0) {

            if(p_obj->response_body_pos > p_obj->response_head_length) {

                lseek(p_obj->fd_file, p_obj->file_offset + (p_obj->response_body_pos - p_obj->response_head_length), SEEK_SET);

                r = read(p_obj->fd_file, p_obj->response_body, BUFSIZ32);

                // printf("отступ файла %d, %d\n", p_obj->file_offset + (p_obj->response_body_pos - p_obj->response_head_length), ev->data.fd);
                // printf("считано байт с файла %d, %d\n", r, ev->data.fd);

                if(!r) goto toggle_epollin;
            }

        }

        if(p_obj->port == 443) {
            sended = SSL_write(p_obj->ssl, p_obj->response_body, r);

            if (sended == -1) {
                int err = ssl_get_error(p_obj->ssl, sended);

                if (err == 1) return;
            }
        }
        else
            sended = write(ev->data.fd, p_obj->response_body, r);

        // char _num[16];

        // string str_fd = itoa(ev->data.fd, &_num[0]);

        // str_fd += ".txt";

        // ofstream myfile;
        // myfile.open(str_fd.c_str(), ios::app);

        // if (myfile.is_open()) {
        //     myfile.write(p_obj->response_body, r);
        //     myfile.close();
        // }
    }
    else {
        if(p_obj->port == 443) {
            sended = SSL_write(p_obj->ssl, p_obj->response_body + p_obj->response_body_pos, p_obj->response_body_length - p_obj->response_body_pos);

            if (sended == -1) {
                int err = ssl_get_error(p_obj->ssl, sended);

                if (err == 1) return;
            }
        }
        else
            sended = write(ev->data.fd, p_obj->response_body + p_obj->response_body_pos, p_obj->response_body_length - p_obj->response_body_pos);

        // char _num[16];

        // string str_fd = itoa(ev->data.fd, &_num[0]);

        // str_fd += ".txt";

        // ofstream myfile;
        // myfile.open(str_fd.c_str(), ios::app);

        // if (myfile.is_open()) {
        //     myfile.write(p_obj->response_body + p_obj->response_body_pos, p_obj->response_body_length - p_obj->response_body_pos);
        //     myfile.close();
        // }
    }

    // printf("sended %d, %d\n", sended, errno);

    if(sended > 0) {

        p_obj->response_body_pos += sended;

        if(p_obj->response_body_pos >= p_obj->response_body_length) {

            toggle_epollin:

            // printf("toggle_epollin\n");

            ev->events = EPOLLIN;
            if(epoll_ctl(epollfd, EPOLL_CTL_MOD, ev->data.fd, ev) == -1) {
                printf("[E] epoll_ctl failed in write done, %d\n", errno);
            }

            // char _num[16];

            // string str_fd = itoa(ev->data.fd, &_num[0]);

            // str_fd += ".txt";

            // ofstream myfile;
            // myfile.open(str_fd.c_str(), ios::app);

            // if (myfile.is_open()) {
            //     myfile.write("\n----------------\n", 18);
            //     myfile.close();
            // }

            if(!p_obj->keep_alive) {
                if(epoll_ctl(epollfd, EPOLL_CTL_DEL, ev->data.fd, NULL) == -1) {
                    printf("[E] epoll_ctl failed del, %d\n", errno);
                }

                clearConnectionObject(p_obj, ev);

                close(ev->data.fd);
            } else {
                clearConnectionObject(p_obj, ev);
            }
        }
    }
    else goto epoll_write_error;

    return;

    epoll_write_error:

    // printf("epoll_write_error\n");

    p_obj->keep_alive = false;

    if(epoll_ctl(epollfd, EPOLL_CTL_DEL, ev->data.fd, NULL) == -1) {
        printf("[E] epoll_ctl failed del, %d\n", errno);
    }

    clearConnectionObject(p_obj, ev);

    close(ev->data.fd);
}

void sigpipeHandler(int signum) {
    // printf("Received signal %d\n", signum);
}

void addListener(int& epollfd, vector<socket_t*>& sockets, const char* ip, short unsigned int port) {

    socket_t* socket_st         = new socket_t();
              socket_st->socket = create_and_bind(ip, port);
              socket_st->port   = port;

    if(socket_st->socket == -1) {
        printf("can't create socket on port %d\n", port);
        exit(0);
    }

    if(!make_socket_nonblocking(socket_st->socket)) {
        printf("can't make socket nonblocking on port %d\n", port);
        exit(0);
    }

    if(listen(socket_st->socket, SOMAXCONN) == -1) {
        printf("listen socket failed on port %d\n", port);
        exit(0);
    }

    socket_st->event.data.fd  = socket_st->socket;
    socket_st->event.events   = EPOLLIN | EPOLLEXCLUSIVE;

    if(epoll_ctl(epollfd, EPOLL_CTL_ADD, socket_st->socket, &socket_st->event) == -1) {
        printf("[E] epoll_ctl failed in addListener, %d\n", errno);
        if(epollfd == -1) {
            printf("epoll_ctl failed\n");
            exit(0);
        }
    }

    sockets.push_back(socket_st);
}

bool is_current_socket(vector<socket_t*>& sockets, epoll_event* ev, unordered_map<int, obj*>* objs, int& epollfd) {

    size_t s = sockets.size();

    // printf("start current_socket\n");

    for (size_t i = 0; i < s; i++) {

        // printf("is socket %d, %d\n", sockets[i]->socket, ev->data.fd);

        if(sockets[i]->socket == ev->data.fd) {

            // printf("find %d, %d\n", sockets[i]->socket, ev->data.fd);

            return accept_connection(sockets[i], objs, epollfd);
        }
    }
    return false;
}

void* thread_func(void* arg) {

    int epollfd = epoll_create1(0);

    if(epollfd == -1) {
        printf("epoll_create1 failed\n");
        return nullptr;
    }

    epoll_event events[EPOLL_MAX_EVENTS];

    vector<socket_t*>    sockets;

    addListener(epollfd, sockets, "192.168.1.10", 443);
    addListener(epollfd, sockets, "192.168.1.10", 80);

    unordered_map<int, obj*>* objs = (unordered_map<int, obj*>*)arg;

    obj*   p_obj;

    char*  buf = (char*)malloc(BUFSIZ32);

    conn_st* db_conn = new conn_st();

    db_conn->conn = nullptr;
    db_conn->ttl  = chrono::system_clock::now() - chrono::hours(DB_CONNECTION_TTL + 1);

    pthread_mutex_lock(&lock_objs);
    db_connection.emplace(gettid(), db_conn);
    pthread_mutex_unlock(&lock_objs);

    int n;

    epoll_event* ev;

    unordered_map<int, obj*>::iterator it;

    while(1) {

        n = epoll_wait(epollfd, events, EPOLL_MAX_EVENTS, -1);

        while(--n >= 0) {

            ev = &events[n];
            int fd = ev->data.fd;

            pthread_mutex_lock(&lock_objs);
            it = objs->find(fd);
            if (it != objs->end()) {
                pthread_mutex_unlock(&lock_objs);
                p_obj = (it->second);
            } else {
                p_obj = objs->emplace(fd, new obj()).first->second;
                p_obj->port = 80;
                pthread_mutex_unlock(&lock_objs);
            }

            if(p_obj->active.load(memory_order_acquire)) {
                // printf("cell already used\n");
                continue;
            }

            p_obj->active.store(true, memory_order_release);

            if(ev->events & (EPOLLERR | EPOLLHUP) ) {

                // printf("EPOLLERR %d, errno %d\n", fd, errno);

                if(epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL) == -1) {
			        printf("[E] epoll_ctl failed del, %d\n", errno);
			    }

                p_obj->keep_alive = false;

                clearConnectionObject(p_obj, ev);

                close(fd);
            }
            else if(is_current_socket(sockets, ev, objs, epollfd)) {
            }
            else if(ev->events & EPOLLIN) {

                if(p_obj->port == 80 || p_obj->sslConnected) {
                    handleDataRead(p_obj, ev, epollfd, buf);
                    // printf("read\n");
                }
                else {
                    handleHandshake(p_obj, ev, epollfd);
                    // printf("handshake read\n");
                }
            }
            else if(ev->events & EPOLLOUT) {
                if(p_obj->port == 80 || p_obj->sslConnected) {
                    handleDataWrite(p_obj, ev, epollfd);
                    // printf("write\n");
                }
                else {
                    handleHandshake(p_obj, ev, epollfd);
                    // printf("handshake write\n");
                }
            }
            p_obj->active.store(false, memory_order_release);
        }
    }

    free(buf);

    for (size_t i = 0; i < sockets.size(); i++) {
        close(sockets[i]->socket);
        delete sockets[i];
    }

    close(epollfd);

    pthread_exit(NULL);
}

void* thread_mail(void* arg) {

    Mail* mail = new Mail();

    list<mail_st*> local_list_mails;

    map<double, list<mail_st*>> sort_list;

    map<double, bool> email_is_set;

    while(1) {

        pthread_mutex_lock(&lock_mail);

        if (list_mails.size() == 0) {
            pthread_cond_wait(&cond_mail, &lock_mail);
        }

        local_list_mails = move(list_mails);

        pthread_mutex_unlock(&lock_mail);

        while(!local_list_mails.empty()) {

            auto& elem = local_list_mails.front();
            
            char* p = strchr(elem->email, '@');

            if(p == nullptr) {
                free(elem->email);
                free(elem->subject);
                free(elem->body);
                delete elem;
                local_list_mails.pop_front();
                continue;
            }

            p++;

            if(!email_is_set[simpleHash(p)]) {
                if(Mail::isRealService(elem->email)) {
                    email_is_set[simpleHash(p)] = true;
                } else {
                    free(elem->email);
                    free(elem->subject);
                    free(elem->body);
                    delete elem;
                    local_list_mails.pop_front();
                    continue;
                }
            }

            sort_list[simpleHash(p)].push_back(move(elem));

            local_list_mails.pop_front();
        }

        for(auto& elem : sort_list) {

            while (!elem.second.empty()) {

                auto& ml = elem.second.front();

                if(!mail->isConnected()) {
                    if(!mail->createConnection(ml->email, 25)) {
                        // printf("connection NOT established\n");

                        free(ml->email);
                        free(ml->subject);
                        free(ml->body);

                        delete ml;

                        elem.second.pop_front();

                        continue;
                    }
                }

                // printf("connection established\n");

                mail->setFrom("root@galleryhd.ru");
                mail->setTo(ml->email);
                mail->setSubject(ml->subject);
                mail->setData(ml->body);
                mail->send();

                // printf("mail sended\n");

                free(ml->email);
                free(ml->subject);
                free(ml->body);

                delete ml;

                elem.second.pop_front();

                // printf("connection closed\n");
            }

            mail->closeConnection();
        }
    }
}

void* thread_polling(void* arg) {

    unordered_map<int, polling_st*>::iterator it_p;

    obj* p_obj;

    while(1) {

        pthread_mutex_lock(&lock_poll);

        pthread_cond_wait(&cond_poll, &lock_poll);

        pthread_mutex_unlock(&lock_poll);

        for(it_p = polling_array->begin(); it_p != polling_array->end(); ++it_p) {

            if (!it_p->second->active) continue;

            p_obj = it_p->second->p_obj;

            p_obj->active.store(true, memory_order_release);

            Polling::makeResult(it_p->second);

            Polling::joinConnection(it_p->second->epoll_fd, it_p->first);

            Polling::freePoll(it_p->second);

            p_obj->active.store(false, memory_order_release);
        }
    }
}


} // namespace