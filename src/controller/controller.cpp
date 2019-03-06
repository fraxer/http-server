#include "controller.h"

namespace server {

Controller::Controller(obj* ob): p_obj(ob) {}

Controller::Controller(obj* ob, char*(*f)(void*)): p_obj(ob), pointerOnFunc(f) {}

Controller::Controller(obj* ob, char*(*f)(void*), const int _socketfd, const int& _epollfd): p_obj(ob), pointerOnFunc(f), socketfd(_socketfd), epollfd(_epollfd) {
    this->p_obj->in_thread = true;
}

Controller::~Controller() {
    // printf("destroy controller\n");
}

void Controller::setHandler(char*(*f)(void*)) {
    this->pointerOnFunc = f;
}

void Controller::setSocketFd(int fd) {
    this->socketfd = fd;
}

void Controller::setEpollFd(int fd) {
    this->epollfd = fd;
}

char* Controller::methodNotAllwed(Controller* c) {
    c->p_obj->status = 405;
    return c->result(new string("<html><head></head><body><h1 style=\"text-align:center;margin:20px\">" + string(http_status_text[c->p_obj->status].key) + "</h1></body></html>"));
}

void Controller::setHeader(const char* key, const char* value, size_t key_len, size_t value_len) {
    
    size_t len;

    if(key_len > 0) {
        len = key_len;
        memcpy(this->pageMemory + this->headersPos, key, key_len);
    }
    else {
        len = strlen(key);
        memcpy(this->pageMemory + this->headersPos, key, len);
    }

    this->headersPos += len;

    memcpy(this->pageMemory + this->headersPos, ": ", 2);

    this->headersPos += 2;

    if(value_len > 0) {
        len = value_len;
        memcpy(this->pageMemory + this->headersPos, value, value_len);
    }
    else {
        len = strlen(value);
        memcpy(this->pageMemory + this->headersPos, value, len);
    }

    this->headersPos += len;

    memcpy(this->pageMemory + this->headersPos, "\r\n", 2);

    this->headersPos += 2;
}

char* Controller::result(string* str) {

    this->contentLength = str->size();

    char* c = (char*)malloc(contentLength + 1);

    if(c == nullptr) {
        delete str;
        return nullptr;
    }

    memcpy(c, str->c_str(), contentLength);

    c[contentLength] = 0;

    delete str;

    return c;
}

void* Controller::resultThread(char* str) {
    this->p_obj->response_body = str;

    this->toggleThreadOnWrite();

    return nullptr;
}

void* Controller::resultThread(string* str) {
    return this->resultThread(this->result(str));
}

char* Controller::getData() {
    
    char* buffer = (char*)malloc(this->p_obj->request_body_length + 1);

    read(this->p_obj->request_body, buffer, this->p_obj->request_body_length);

    buffer[this->p_obj->request_body_length] = 0;

    return buffer;
}

char* Controller::makeCacheable(const char* handler_path) {

    this->hashHandlerPath = simpleHash(handler_path);

    this->isCacheable = true;

    this->findedInCache = false;

    pthread_mutex_lock(&lock_controller_cache);

    unordered_map<double, fl>::const_iterator it_file = cache_page.find(this->hashHandlerPath);

    // если в кеше
    if(it_file != cache_page.end()) {

        pthread_mutex_unlock(&lock_controller_cache);

        this->findedInCache = true;

        this->p_obj->response_head_length = it_file->second.head_size;

        this->p_obj->response_body_length = it_file->second.head_size + it_file->second.body_size;

        // printf("%d, %d, %s\n", this->p_obj->response_head_length, this->p_obj->response_body_length, it_file->second.head);

        // printf("\"%lf\" finded in cache\n", this->hashHandlerPath);

        return it_file->second.head;
    }

    // printf("\"%lf\" not found in cache\n", this->hashHandlerPath);

    return nullptr;
}

void Controller::setCache(const unsigned int& head_size, const unsigned int& body_size, char* head, char* body) {

    if(!this->isCacheable) return;

    // printf("\"%lf\" added into cache\n", this->hashHandlerPath);

    cache_page[this->hashHandlerPath] = {head_size, body_size, head, body};

    pthread_mutex_unlock(&lock_controller_cache);

    this->p_obj->cached = true;
}

void Controller::runHandler() {
    this->allocMemory();
    this->pointerOnFunc(this);
}

bool Controller::runPollingHandler() {
    this->allocMemory();

    this->p_obj->response_body = this->pointerOnFunc(this);

    if (this->p_obj->response_body == nullptr) {

        return true;

    } else {

        this->p_obj->response_body = this->getContentWrapper(true);

        return false;
    }
}

char* Controller::createThread(void*(*f)(void*), Controller* c) {

    if(c->p_obj->thread == nullptr) {
        c->p_obj->thread = new pthread_t();
    }

    int err_thread = pthread_create(c->p_obj->thread, NULL, *f, c);

    if (err_thread != 0) {
        printf("невозможно создать поток %d", err_thread);
        c->p_obj->in_thread = false;
        return nullptr;
    }

    int r = pthread_detach(*c->p_obj->thread);

    if (r != 0) {
        printf("error detach\n");
    }

    return nullptr;
}

void Controller::toggleThreadOnWrite() {
    epoll_event event;

    event.events  = EPOLLOUT;
    event.data.fd = this->socketfd;

    // printf("return%d, %d\n", this->epollfd, this->socketfd);

    if(epoll_ctl(this->epollfd, EPOLL_CTL_MOD, this->socketfd, &event) == -1) {
        printf("[E] epoll_ctl failed in statusThread, %d\n", errno);
    }
}

char* Controller::allocMemory() {

    char* buffer = (char*)malloc(HEADBUF);

    if(buffer == nullptr) return nullptr;

    this->pageMemory    = buffer;
    this->headersPos    = 37;
    this->isCacheable   = false;
    this->findedInCache = false;

    this->setHeader("Cache-Control",               "no-cache",                  13,  8);
    this->setHeader("Connection",                  "keep-alive",                10, 10);
    // this->setHeader("Content-Type",                "text/html; charset=UTF-8",  12, 24);
    // this->setHeader("Strict-Transport-Security",   "max-age=31536000",          25, 16);
    this->setHeader("Access-Control-Allow-Origin", "*",                         27,  1);
    this->setHeader("Access-Control-Allow-Headers", "Token",                    28,  5);
    // this->setHeader("Access-Control-Allow-Credentials", "true",                 32,  4);

    this->setAllowMethods((char*)"GET, POST");

    return buffer;
}

char* Controller::getContentWrapper(bool in_thread) {

    char* body_page = nullptr;

    if (in_thread) {
        body_page = this->pageMemory;
    } else {
        body_page = this->allocMemory();
    }

    char* content = nullptr;

    // printf("in wrapper\n");

    if (in_thread) {
        content = this->p_obj->response_body;

        // printf("in getThreadContent: %s\n", content);
    } else {

        this->contentLength = 0;

        content = this->pointerOnFunc(this);
    }

    if(content == nullptr) {
        free(body_page);
        // this->pageMemory = nullptr;
        return nullptr;
    }

    if(this->findedInCache) {
        this->p_obj->cached = true;
        free(body_page);
        // this->pageMemory = nullptr;
        return content;
    }

    if(this->p_obj->status != 200) this->p_obj->method = GET;

    if(this->p_obj->method == OPTIONS) {
        setHeader("Allow", allowMethods, 5);
        setHeader("Access-Control-Allow-Methods", allowMethods, 28);
    }

    char* memContenLength = (char*)malloc(16);

    if(memContenLength == nullptr) {
        free(content);
        free(body_page);
        // this->pageMemory = nullptr;
        return nullptr;
    }

    setHeader("Content-Length", itoa(this->contentLength, memContenLength), 14);

    pair_char_int status = http_status_text[this->p_obj->status];

    unsigned int offset = 28 - status.length; // 37 - 9 = 38

    memcpy(body_page + offset, "HTTP/1.1 ", 9);

    memcpy(body_page + offset + 9, status.key, status.length);

    memcpy(body_page + this->headersPos, "\r\n", 2);

    this->headersPos += 2;

    this->p_obj->response_head_length = this->headersPos - offset;

    this->p_obj->response_body_length = this->p_obj->response_head_length;

    memmove(body_page, body_page + offset, this->p_obj->response_head_length);

    if(this->p_obj->method != HEAD) this->p_obj->response_body_length += this->contentLength;

    char* res_page = (char*)realloc(body_page, this->p_obj->response_body_length);

    if (res_page == nullptr) {
        free(body_page);
        free(content);
        free(memContenLength);
        // this->pageMemory = nullptr;
        return nullptr;
    }

    body_page = res_page;
    this->pageMemory = res_page;

    this->setCache(this->p_obj->response_head_length, this->contentLength, body_page, body_page + this->contentLength);

    // ofstream myfile;
    // myfile.open("headers", ios::app);

    // if (myfile.is_open()) {
    //     myfile.write(body_page, this->p_obj->response_body_length);
    //     myfile.write("\n----------------\n", 18);
    //     myfile.close();
    // }

    if(content != nullptr) {
        if(this->contentLength > 0) {
            if(this->p_obj->method != HEAD) {
                memcpy(body_page + this->p_obj->response_head_length, content, this->contentLength);
            }
        }

        free(content);
    }

    free(memContenLength);

    // this->pageMemory = nullptr;

    return body_page;
}

void Controller::setAllowMethods(char* methods) {
    allowMethods = methods;
}

void Controller::parseQuery(char* str) {

    char* k = str;
    char* v = nullptr;
    bool find_value = false;

    while(*str != 0) {

        if(!find_value && *str == '=') {

            find_value = true;

            v = str + 1;

            // if (str - k == 0) {
            //     str++;
            //     continue;
            // }

            *str = 0;
        }
        else if(*str == '&') {

            find_value = false;

            *str = 0;

            if(v == nullptr) v = str;

            this->query.emplace(simpleHash(k), (pair_chars){k, v});

            k = str + 1;
        }

        str++;
    }

    if(k - str == 0) return;

    if(k - v > 0 || v == nullptr) {
        this->query.emplace(simpleHash(k), (pair_chars){k, str});
    }
    else {
        this->query.emplace(simpleHash(k), (pair_chars){k, v});
    }
}

void Controller::parseCookie(char* str) {

    char* k = str;
    char* v = nullptr;
    bool find_key   = false;
    bool find_value = false;

    while(*str != 0) {

        if(!find_key && *str != ' ') { // |  key1=value1;   key2=value2|
            k = str;

            find_key = true;
        }
        else if(!find_value && *str == '=') {

            find_value = true;

            v = str + 1;

            *str = 0;
        }
        else if(*str == ';') {

            find_key = false;

            find_value = false;

            *str = 0;

            if(v == nullptr) v = str;

            this->cookie.emplace(simpleHash(k), (pair_chars){k, v});

            k = str + 1;
        }

        str++;
    }

    if(k - str == 0) return;

    if(k - v > 0 || v == nullptr) {
        this->cookie.emplace(simpleHash(k), (pair_chars){k, str});
    }
    else {
        this->cookie.emplace(simpleHash(k), (pair_chars){k, v});
    }
}

char* Controller::getToken() {

    char* param_token = getHeader("Token", this->p_obj);

    char* _token = nullptr;

    char* buffer_cookie = nullptr;

    if(param_token != nullptr) {
        _token = param_token;
    } else {
        char* _cookie = getHeader("Cookie", this->p_obj);

        if(_cookie == nullptr) {
            return (char*)calloc(1, 1);
        }

        char* buffer_cookie = (char*)malloc(strlen(_cookie) + 1);

        strcpy(buffer_cookie, _cookie);

        this->parseCookie(buffer_cookie);

        _token = this->getCookie("token");
    }

    size_t len_token = strlen(_token);

    if(len_token == 0 || len_token > 40) {
        if(buffer_cookie != nullptr) free(buffer_cookie);
        return (char*)calloc(1, 1);
    }

    char* token = (char*)malloc(len_token + 1);

    strcpy(token, _token);

    if(buffer_cookie != nullptr) free(buffer_cookie);

    return token;
}

char* Controller::getQuery(const char* str) {

    unordered_map<float, pair_chars>::iterator it = this->query.find(simpleHash(str));

    if(it == this->query.end()) return (char*)"";

    return it->second.value;
}

char* Controller::getCookie(const char* str) {

    unordered_map<float, pair_chars>::iterator it = this->cookie.find(simpleHash(str));

    if(it == this->cookie.end()) return (char*)"";

    return it->second.value;
}

char* Controller::hexDecode(char*& str) {

    char  a, b;

    char* p_str = str;

    char* new_str = (char*)malloc(strlen(str) + 1);

    char* dst = new_str;

    while(*p_str != 0) {

        if((*p_str == '%') && (isxdigit(p_str[1]) && isxdigit(p_str[2]))) {

            a = p_str[1];
            b = p_str[2];

            if(a >= 'a') a -= 'a'-'A';

            if(a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';

            if(b >= 'a') b -= 'a'-'A';

            if(b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';

            *dst++ = 16 * a + b;

            p_str += 3;

            continue;
        }
        else {
            *dst++ = *p_str++;
        }
    }

    *dst = 0;

    free(str);

    return new_str;
}

bool Controller::parseHeaders(t_headers& headers, char* p, int& pos, const unsigned int& len) {

    char* key        = p;
    // char* key_end    = p;
    char* value      = nullptr;
    // char* value_end  = nullptr;
    bool  find_sep   = false;
    bool  find_value = false;

    // printf("start: %.15s\n", p);
    // printf("pos: %d\n", pos);
    // printf("len: %d\n", len);

    while(*p != '\0') {

        // printf("%c\n", p);

        if((unsigned int)(pos + 2) > len) return false;

        if(*p == ':') {

            if(find_sep) {
                p++;
                pos++;
                continue;
            }

            if(p[1] != ' ') {
                printf("invalid separator\n");
                return false;
            }

            if(p - key == 0) {
                printf("error: empty key 1\n");
                return false;
            }

            // key_end = p;

            *p = 0;

            p += 2;

            value = p;

            find_value = true;

            find_sep   = true;

            pos += 2;
        }
        else if(*p == '\r') {

            if(p[1] != '\n') {
                printf("invalid end of line\n");
                return false;
            }

            if(!find_sep && p - key > 0) {
                printf("error: empty key 2\n");
                return false;
            }

            if(p - value == 0) {
                printf("error: empty value\n");
                return false;
            }

            pos += 2;

            if(!find_value) break;

            // value_end = p;

            *p = 0;

            p += 2;

            find_value = false;

            find_sep   = false;

            double d = simpleHash(key);

            if(headers.find(d) == headers.end()) {

                values_st* kv = new values_st();

                kv->value = value;

                this->parseHeaderFields(value, kv->fields);

                headers.emplace(d, kv);
            }

            key = p;
        }
        else if(*p == '\n' || *p == '\t') {
            printf("error: firbidden symbol\n");
            return false;
        }
        else {
            p++;
            pos++;
        }
    }

    if(p[0] != '\r' && p[1] != '\n') {
        printf("end of headers not found\n");
        return false;
    }

    return true;
}

bool Controller::parseHeaderFields(char* str, unordered_map<double, char*>& fields) {

    // form-data; name=\"file\"; key\"value\"; ; filename=\"rutger-va\"n-de-st; eeg-cyberp\"unk-8-8.jpg\"; asd=
    // \s\s; ;; form-data; name=\"file\"; key=value\"; ; filename=\"rutger-v\"; a-s-d;\s
    // ; ;; form-data; name=\"file\"; key\"va; lue\"; ; filename=\"rutger-v\"; a-s-d

    int   pos              = 0;
    char* p                = str;
    bool  quote_opened     = false;
    bool  value_find       = false;
    char* key              = str;
    char* value            = nullptr;

    unordered_map<double, char*>::iterator it;

    while(*p != 0) {
        if(*p == ';') {       
            if(quote_opened) {
                // printf("quotes already opened\n");
                p++;
                pos++;
                continue;
            }

            *p = 0;

            if(key[0] == 0 || key == nullptr) {
                // printf("empty key\n");
                key = p + 1;
                p++;
                pos++;
                continue;
            }

            if(pos < 1 || (p[-1] != '"' && p[1] != ' ')) {
                // printf("error end of key-value\n");
                p++;
                pos++;
                continue;
            }

            double d = simpleHash(key);

            it = fields.find(d);

            // printf("fields: %s -> %s\n", key, value);

            if(it == fields.end()) {
                fields.emplace(d, value);
            }

            key          = nullptr;
            value        = nullptr;
            quote_opened = false;
            value_find   = false;

            p++;
            pos++;
        } else if(*p == ' ') {
            if(!quote_opened) {
                key = p + 1;
                // printf("new key: %s\n", key);
            }

            p++;
            pos++;
        } else if(*p == '=') {
            if(!quote_opened) {
                *p = 0;
                value_find = true;
                // printf("set key: %s\n", key);
            }

            p++;
            pos++;
        } else if(*p == '"') {
            if(value_find && !quote_opened) {
                *p = 0;
                value = p + 1;
                quote_opened     = true;
                // printf("set value: %s\n", value);
            } else if (quote_opened && (p[1] == ';' || p[1] == 0)) {
                *p = 0;
                quote_opened     = false;
            }

            p++;
            pos++;
        } else if (value_find && !quote_opened) {
            // printf("error, break. %s\n", key);
            key = p;
            value = nullptr;
            value_find = false;
            p++;
            pos++;
            // return false;
        }
        else {
            p++;
            pos++;
        }
    }


    if(key[0] == 0 || key == nullptr) {
        // printf("empty key\n");
        return true;
    }

    double d = simpleHash(key);

    it = fields.find(d);

    if(it == fields.end()) {
        fields.emplace(d, value);
    }

    // for(auto& el : fields) {
    //     printf("%s -> %s\n", el.second);
    // }

    return true;
}

int Controller::parseData(char* data, const char* boundary, const unsigned int& len, unordered_map<double, t_funvalues&>& callback) {
    ssize_t boundary_length = strlen(boundary);
    ssize_t local_boundary_length = boundary_length + 2;

    char* local_boundary = (char*)malloc(local_boundary_length + 1);

    memcpy(local_boundary, "--", 2);
    memcpy(local_boundary + 2, boundary, boundary_length);
    local_boundary[boundary_length + 2] = 0;

    // printf("local_boundary: %s\n", local_boundary);

    char* _data = data;

    unordered_map<double, t_funvalues&>::iterator it;

    while(_data[0] != 0) {

        char* u = strstr(_data, local_boundary);

        if(u == nullptr || (u - _data) > 0) {
            free(local_boundary);
            return 0;
        }

        u += local_boundary_length;

        _data = u;

        u = strstr(_data, "\r\n");

        if(u == nullptr || (u - _data) > 0) {
            free(local_boundary);
            return 0;
        }

        u += 2;

        int pos = 0;

        t_headers headers;
        t_headers::iterator h_it;

        if(!this->parseHeaders(headers, u, pos, len)) {
            // printf("parse error\n");
            for(auto& el : headers) {
                el.second->fields.clear();
                free(el.second);
            }
            free(local_boundary);
            return 0;
        }

        // for(auto& el : headers) {
        //     printf("%s >>- %s\n", el.second->key, el.second->value);
        // }

        // printf("after parse headers\n");

        bool finded = false;

        char* end = u + pos;

        while(end++) {
            // if(pos >= len) break;
            if(end[0] == '-') { // fix this
                char* s = strstr(end, local_boundary);
                if(s) {
                    end = s;
                    finded = true;
                    break;
                }
            }
            // pos++;
        }

        if(!finded) {
            for(auto& el : headers) {
                el.second->fields.clear();
                free(el.second);
            }
            free(local_boundary);
            return 0;
        }

        end[-2] = 0;

        char* start = u + pos;

        // printf("after parse body\n");

        _data = u = end;

        unsigned long int size = end - start - 2;

        end += local_boundary_length;

        finded = false;

        if((end - data + 1) <= len && end[0] == '-' && end[1] == '-') {  // check   + 1
            finded = true;
        }

        // printf("after check end file\n");

        h_it = headers.find(simpleHash("Content-Disposition"));

        if(h_it == headers.end()) {
            printf("header Content-Disposition not found\n");
            for(auto& el : headers) {
                el.second->fields.clear();
                free(el.second);
            }
            free(local_boundary);
            return 0;
        }

        char* name = h_it->second->fields[simpleHash("name")];

        it = callback.find(simpleHash(name));

        if(it != callback.end()) {
            it->second(headers, start, size);
        }

        for(auto& el : headers) {
            el.second->fields.clear();
            free(el.second);
        }

        if(finded) {
            // printf("end\n");
            free(local_boundary);
            return 1;
        }
    }

    free(local_boundary);

    return 0;
}

// void Controller::fillRights() {
//     vector<string> admin_operations;
//                    admin_operations.push_back("addNote");
//                    admin_operations.push_back("addItem");
//                    admin_operations.push_back("addComment");

//     vector<string> user_operations;
//                    user_operations.push_back("addNote");
//                    user_operations.push_back("addComment");

//     right["admin"] = admin_operations;
//     right["user"]  = user_operations;
// }

// bool Controller::hasRight(const string& who, const string& what) {
//     short size = right[who].size();
//     for(short i = 0; i < size; i++) {
//         if(right[who][i] == what)
//             return true;
//     }
//     return false;
// }


} // namespace