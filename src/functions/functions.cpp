#include "functions.h"

using namespace std;
namespace server {

vector<rewrite_st>                      rewrite;

unordered_map<double, char*>            mime_types;

size_t                                  rewrite_size;

list<mail_st*>                          list_mails;

unordered_map<int, conn_st*>            db_connection;

unordered_map<double, fl>               cache_file;

unordered_map<double, fl>               cache_page;

pthread_mutex_t                         lock_controller_cache = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t                         lock_file = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t                         lock_mail = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t                          cond_mail = PTHREAD_COND_INITIALIZER;

pthread_mutex_t                         lock_list_conn = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t                          cond_list_conn = PTHREAD_COND_INITIALIZER;

pthread_mutex_t                         lock_poll = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t                          cond_poll = PTHREAD_COND_INITIALIZER;

conn_st*                                list_connections[PQ_MAX_CONNECTIONS];

unordered_map<int, polling_st*>*        polling_array = new unordered_map<int, polling_st*>();

unordered_map<short, pair_char_int> http_status_text = {
    { 100, {(char*)"100 Continue\r\n",                        14} },
    { 101, {(char*)"101 Switching Protocols\r\n",             25} },
    { 102, {(char*)"102 Processing\r\n",                      16} },
    { 103, {(char*)"103 Early Hints\r\n",                     17} },

    { 200, {(char*)"200 OK\r\n",                              8 } },
    { 201, {(char*)"201 Created\r\n",                         13} },
    { 202, {(char*)"202 Accepted\r\n",                        14} },
    { 203, {(char*)"203 Non-Authoritative Information\r\n",   35} },
    { 204, {(char*)"204 No Content\r\n",                      16} },
    { 205, {(char*)"205 Reset Content\r\n",                   19} },
    { 206, {(char*)"206 Partial Content\r\n",                 21} },
    { 207, {(char*)"207 Multi-Status\r\n",                    18} },
    { 208, {(char*)"208 Already Reported\r\n",                22} },
    { 226, {(char*)"226 IM Used\r\n",                         13} },

    { 300, {(char*)"300 Multiple Choices\r\n",                22} },
    { 301, {(char*)"301 Moved Permanently\r\n",               23} },
    { 302, {(char*)"302 Moved Temporarily\r\n",               23} },
    { 303, {(char*)"303 See Other\r\n",                       15} },
    { 304, {(char*)"304 Not Modified\r\n",                    18} },
    { 305, {(char*)"305 Use Proxy\r\n",                       15} },
    { 306, {(char*)"306 Switch Proxy\r\n",                    18} },
    { 307, {(char*)"307 Internal Redirect\r\n",               23} },
    { 308, {(char*)"308 Permanent Redirect\r\n",              24} },

    { 400, {(char*)"400 Bad Request\r\n",                     17} },
    { 401, {(char*)"401 Unauthorized\r\n",                    18} },
    { 402, {(char*)"402 Payment Required\r\n",                22} },
    { 403, {(char*)"403 Forbidden\r\n",                       15} },
    { 404, {(char*)"404 Not Found\r\n",                       15} },
    { 405, {(char*)"405 Method Not Allowed\r\n",              24} },
    { 406, {(char*)"406 Not Acceptable\r\n",                  20} },
    { 407, {(char*)"407 Proxy Authentication Required\r\n",   35} },
    { 408, {(char*)"408 Request Timeout\r\n",                 21} },
    { 409, {(char*)"409 Conflict\r\n",                        14} },
    { 410, {(char*)"410 Gone\r\n",                            10} },
    { 411, {(char*)"411 Length Required\r\n",                 21} },
    { 412, {(char*)"412 Precondition Failed\r\n",             25} },
    { 413, {(char*)"413 Payload Too Large\r\n",               23} },
    { 414, {(char*)"414 URI Too Long\r\n",                    18} },
    { 415, {(char*)"415 Unsupported Media Type\r\n",          28} },
    { 416, {(char*)"416 Range Not Satisfiable\r\n",           27} },
    { 417, {(char*)"417 Expectation Failed\r\n",              24} },
    { 418, {(char*)"418 I'm a teapot\r\n",                    18} },
    { 421, {(char*)"421 Misdirected Request\r\n",             25} },
    { 422, {(char*)"422 Unprocessable Entity\r\n",            26} },
    { 423, {(char*)"423 Locked\r\n",                          12} },
    { 424, {(char*)"424 Failed Dependency\r\n",               23} },
    { 426, {(char*)"426 Upgrade Required\r\n",                22} },
    { 428, {(char*)"428 Precondition Required\r\n",           27} },
    { 429, {(char*)"429 Too Many Requests\r\n",               23} },
    { 431, {(char*)"431 Request Header Fields Too Large\r\n", 37} },
    { 451, {(char*)"451 Unavailable For Legal Reasons\r\n",   35} },

    { 500, {(char*)"500 Internal Server Error\r\n",           27} },
    { 501, {(char*)"501 Not Implemented\r\n",                 21} },
    { 502, {(char*)"502 Bad Gateway\r\n",                     17} },
    { 503, {(char*)"503 Service Unavailable\r\n",             25} },
    { 504, {(char*)"504 Gateway Timeout\r\n",                 21} },
    { 505, {(char*)"505 HTTP Version Not Supported\r\n",      32} },
    { 506, {(char*)"506 Variant Also Negotiates\r\n",         29} },
    { 507, {(char*)"507 Insufficient Storage\r\n",            26} },
    { 508, {(char*)"508 Loop Detected\r\n",                   19} },
    { 510, {(char*)"510 Not Extended\r\n",                    18} },
    { 511, {(char*)"511 Network Authentication Required\r\n", 37} }
};

const char gDigitsLut[200] = {
    '0','0','0','1','0','2','0','3','0','4','0','5','0','6','0','7','0','8','0','9',
    '1','0','1','1','1','2','1','3','1','4','1','5','1','6','1','7','1','8','1','9',
    '2','0','2','1','2','2','2','3','2','4','2','5','2','6','2','7','2','8','2','9',
    '3','0','3','1','3','2','3','3','3','4','3','5','3','6','3','7','3','8','3','9',
    '4','0','4','1','4','2','4','3','4','4','4','5','4','6','4','7','4','8','4','9',
    '5','0','5','1','5','2','5','3','5','4','5','5','5','6','5','7','5','8','5','9',
    '6','0','6','1','6','2','6','3','6','4','6','5','6','6','6','7','6','8','6','9',
    '7','0','7','1','7','2','7','3','7','4','7','5','7','6','7','7','7','8','7','9',
    '8','0','8','1','8','2','8','3','8','4','8','5','8','6','8','7','8','8','8','9',
    '9','0','9','1','9','2','9','3','9','4','9','5','9','6','9','7','9','8','9','9'
};

char* base_path = (char*)"/sda1/vue-apps/frontend-vue.js/dist";

char* tmp_path = (char*)"/sda1/server/tmp/";

char* password_salt = (char*)"Iwnrus&*w3r239u()w2br1tf68ay4wurnWqof0csHYsH5";

unsigned int base_path_length = strlen(base_path);

size_t mb_strlen(const char *str) {

    const char *char_ptr;

    UN symbol;

    size_t len = 0;

    for(char_ptr = str; *char_ptr != 0;) {
        if(*char_ptr == '\0') return len;

        symbol.value = *char_ptr;

        if(symbol.b7 ==0) {
            char_ptr++;
        }
        else if(symbol.b7 == 1 && symbol.b6 == 1 && symbol.b5 == 0) {
            char_ptr += 2;
        }
        else if(symbol.b7 == 1 && symbol.b6 == 1 && symbol.b5 == 1 && symbol.b4 == 0) {
            char_ptr += 3;
        }
        else if(symbol.b7 == 1 && symbol.b6 == 1 && symbol.b5 == 1 && symbol.b4 == 1 && symbol.b3 == 0) {
            char_ptr +=4;
        }

        len++;
    }

    return len;
}

char* itoa(int64_t value, char* buffer) {
    char* b = buffer;
    
    uint64_t u = static_cast<uint64_t>(value);
    if (value < 0) {
        *buffer++ = '-';
        u = ~u + 1;
    }
    
    char temp[20];
    char* p = temp;
    
    while (u >= 100) {
        const unsigned i = static_cast<unsigned>(u % 100) << 1;
        u /= 100;
        *p++ = gDigitsLut[i + 1];
        *p++ = gDigitsLut[i];
    }

    if (u < 10)
        *p++ = char(u) + '0';
    else {
        const unsigned i = static_cast<unsigned>(u) << 1;
        *p++ = gDigitsLut[i + 1];
        *p++ = gDigitsLut[i];
    }

    do {
        *buffer++ = *--p;
    } while (p != temp);

    *buffer = '\0';

    return b;
}

double fastPow(double a, double b) {

    unionPow u = { a };

    u.x[1] = (int)(b * (u.x[1] - 1072632447) + 1072632447);
    u.x[0] = 0;

    return u.d;
}

double simpleHash(const char* s) {

    double key = 0;

    ssize_t len = strlen(s);

    for (short i = 0; i < len; i++) {
        key += s[i] * fastPow(26, len - (i + 1));
    }

    return key;
}

char* toLowerCase(char* str) {

    unsigned int c_pos = 0;

    while(str[c_pos]) {

        str[c_pos] = tolower(str[c_pos]);

        c_pos++;
    }

    return str;
}

void loadMimeTypes() {

    const char *filename = "/sda1/server/required/mime.types";

    int fd = open(filename, O_RDONLY);

    if(fd == -1) {
        printf("mime types file not found. exit.\n");
        exit(0);
    }

    char* buf = (char*)malloc(4096);

    char* c_buf = nullptr;

    char* row = nullptr;

    char* c_row = nullptr;

    int bytes_read = 0;

    bool ismime = true;

    bool start_mime = true;

    bool start_mime_val = true;

    bool endmime = false;

    int k = 0;

    // char* s_m = nullptr;

    int s_p = 0;

    // char* s_m_v = nullptr;

    int s_v_p = 0;

    unordered_map<int, int> arr;

    while( (bytes_read = read(fd, buf, 4096)) > 0 ) {

        c_buf = buf;

        row = (char*)realloc(row, k + bytes_read);

        c_row = row + k;

        // s_m = row + s_p;

        // s_m_v = row + s_v_p;

        int i = 0;

        while(i < bytes_read) {

            switch(*c_buf) {

                case '\r': case '\n': case '\t': case ' ':

                    if(endmime) {

                        ismime  = false;
                        endmime = false;

                        *c_row++ = 0;

                        if(!start_mime_val) {
                            // printf("%d: %s\n", s_v_p, s_m_v);
                            arr[s_v_p] = s_p;

                            start_mime_val = true;
                        }

                        c_buf++;

                        i++;

                        k++;

                        continue;
                    }

                    if(ismime) {

                        c_buf++;

                        i++;

                        continue;
                    }

                    c_buf++;

                    i++;

                    continue;

                case ';':

                    ismime         = true;
                    start_mime     = true;
                    endmime        = false;
                    start_mime_val = true;

                    c_buf++;

                    *c_row++ = 0;

                    // printf("%d: %s\n", s_v_p, s_m_v);
                    arr[s_v_p] = s_p;

                    i++;

                    k++;

                    continue;


                default:

                    if(ismime) {

                        if(start_mime) {
                            start_mime = false;
                            // s_m = c_row;
                            s_p = k;
                        }

                        endmime = true;
                    }

                    if(!endmime) {
                        endmime = true;

                        if(start_mime_val) {
                            start_mime_val = false;
                            // s_m_v = c_row;
                            s_v_p = k;
                        }
                    }

                    *c_row = *c_buf++;

                    c_row++;

                    k++;
            }

            i++;
        }
    }

    arr[s_v_p] = s_p;

    row = (char*)realloc(row, k + 1);

    row[k] = 0;

    free(buf);

    close(fd);

    for (unordered_map<int, int>::iterator it = arr.begin(); it != arr.end(); ++it) {
        mime_types[simpleHash(row + it->first)] = row + it->second;
    }

    // for (unordered_map<double, char*>::iterator it = mime_types.begin(); it != mime_types.end(); ++it) {
    //     cout << it->first << " => " << it->second << "\n";
    // }

    // printf("%s\n", mime_types[simpleHash((char*)"jpeg")]);
}

char* setMethod(obj* ob, char* p, int& pos) {
    if(p[0] == 'G' && p[1] == 'E' && p[2] == 'T' && p[3] == ' ') {
        ob->method = GET;
        pos += 4;
        p += 4;
    }
    else if(p[0] == 'P' && p[1] == 'O' && p[2] == 'S' && p[3] == 'T' && p[4] == ' ') {
        ob->method = POST;
        pos += 5;
        p += 5;
    }
    else if(p[0] == 'P' && p[1] == 'U' && p[2] == 'T' && p[3] == ' ') {
        ob->method = PUT;
        pos += 4;
        p += 4;
    }
    else if(p[0] == 'P' && p[1] == 'A' && p[2] == 'T' && p[3] == 'C' && p[4] == 'H' && p[5] == ' ') {
        ob->method = PATCH;
        pos += 6;
        p += 6;
    }
    else if(p[0] == 'D' && p[1] == 'E' && p[2] == 'L' && p[3] == 'E' && p[4] == 'T' && p[5] == 'E' && p[6] == ' ') {
        ob->method = DELETE;
        pos += 7;
        p += 7;
    }
    else if(p[0] == 'H' && p[1] == 'E' && p[2] == 'A' && p[3] == 'D' && p[4] == ' ') {
        ob->method = HEAD;
        pos += 5;
        p += 5;
    }
    else if(p[0] == 'O' && p[1] == 'P' && p[2] == 'T' && p[3] == 'I' && p[4] == 'O' && p[5] == 'N' && p[6] == 'S' && p[7] == ' ') {
        ob->method = OPTIONS;
        pos += 8;
        p += 8;
    }
    else {
        return nullptr;
    }

    return p;
}

void setRedirect(const char* location, const char* rule, const char* dest, bool stop, short int redirect) {

    rewrite_st  obj;
                obj.location = pcre_compile(location,   0, &obj.location_error, &obj.location_erroffset, NULL);
                obj.rule     = pcre_compile(rule,       0, &obj.rule_error,     &obj.rule_erroffset,     NULL);
                obj.dest     = dest;
                obj.stop     = stop;
                obj.redirect = redirect;

    rewrite.push_back(obj);
}

char* redirects(char* input, obj* p_obj, char*& src, const int& pos) {

    int pp = pos;

    while(*src != '\0' && *src != ' ' && pp < HEADBUF) {
        src++;
        pp++;
    }

    if(pp == HEADBUF) return nullptr;

    bool   uri_changed    = false;
    bool   global_changed = false;
    bool   stop           = false;
    short  iteration      = 0;
    short  uri_copy_len   = 0;
    size_t i              = 0;
    char*  res;
    char*  uri_copy;
    char*  pcre_num;
    char*  redirect_result;
    int    location_ovector[30];
    int    rule_ovector[30];

    if(!(uri_copy = (char*)malloc(HEADBUF))) {
        printf("memory for uri_copy not allocated\n");
        return nullptr;
    }

    if(!(pcre_num = (char*)malloc(10))) {
        printf("memory for pcre_num not allocated\n");
        free(uri_copy);
        return nullptr;
    }

    if(!(redirect_result = (char*)malloc(HEADBUF))) {
        printf("memory for redirect_result not allocated\n");
        free(uri_copy);
        free(pcre_num);
        return nullptr;
    }

    uri_copy_len = src - input;

    memcpy(uri_copy, input, uri_copy_len);

    uri_copy[uri_copy_len] = 0;

    for (; iteration < 10; iteration++) {

        uri_changed = false;

        for (i = 0; i < rewrite_size; i++) {

            res = redirect_result;

            rewrite_st* rwr = &rewrite[i];

            if(pcre_exec(rwr->location, NULL, uri_copy, uri_copy_len, 0, 0, location_ovector, 30) > 0) {

                int rc = pcre_exec(rwr->rule, NULL, uri_copy, uri_copy_len, 0, 0, rule_ovector, 30);

                if(rc <= 0) continue;

                char* dest    = (char*)rwr->dest;
                char* start_d = dest;
                bool  find    = false;

                while(*dest) {

                    // printf("%c -> %d\n", *dest, *dest);

                    if(*dest == '$') {
                        find    = true;
                        start_d = ++dest;

                        // printf("%s\n", start_d);
                    }
                    else if(find && (*dest < 48 || *dest > 57)) {

                        find  = false;

                        if(dest - start_d > 0) {
                            // printf("1. %d -> %.*s\n", dest - start_d, dest - start_d, start_d);

                            memcpy(pcre_num, start_d, dest - start_d);

                            pcre_num[dest - start_d] = 0;

                            int n = atoi(pcre_num);

                            if(n > rc) {
                                printf("group not exist 1\n");
                                continue;
                            }

                            n *= 2;

                            // printf("%d\n", n);

                            memmove(res, uri_copy + rule_ovector[n], rule_ovector[n+1] - rule_ovector[n]);
                            res += rule_ovector[n+1] - rule_ovector[n];
                        }

                        *res++ = *dest++;
                    }
                    else {

                        if(!find) *res++ = *dest;

                        dest++;
                    }
                }

                if(find) {

                    if(dest - start_d > 0) {
                        // printf("2. %d -> %.*s\n", dest - start_d, dest - start_d, start_d);

                        memcpy(pcre_num, start_d, dest - start_d);
                            
                        pcre_num[dest - start_d] = 0;

                        // printf("pcre_num: %s\n", pcre_num);

                        int n = atoi(pcre_num);

                        if(n > rc) {
                            printf("group not exist 2\n");
                            continue;
                        }

                        n *= 2;

                        // printf("%d\n", n);

                        // printf("%s\n", uri_copy + rule_ovector[n]);
                        // printf("%d\n", rule_ovector[n+1] - rule_ovector[n]);


                        memmove(res, uri_copy + rule_ovector[n], rule_ovector[n+1] - rule_ovector[n]);

                        res += rule_ovector[n+1] - rule_ovector[n];
                    }
                }

                *res = 0;

                uri_copy_len = res - redirect_result;

                // printf("%s, %d\n", redirect_result, uri_copy_len);

                uri_changed    = true;
                global_changed = true;

                if(rwr->stop) {
                    stop = true;
                    p_obj->redirect = rwr->redirect;
                    break;
                }
                else {
                    memcpy(uri_copy, redirect_result, uri_copy_len);

                    // printf("%s, %d\n", uri_copy, uri_copy_len);
                }
            }
        }

        if(!uri_changed || stop) break;
    }

    free(uri_copy);
    free(pcre_num);

    if(iteration == 10) {
        printf("infinity loop\n");
    }

    if(global_changed) {

        size_t l = uri_copy_len;

        redirect_result[l] = ' ';
        redirect_result[++l] = 0;

        return redirect_result;
    }

    free(redirect_result);
    return nullptr;
}

char* setURI(obj* ob, char* p, int& pos) {

    if(ob->uri == nullptr) {
        if(!(ob->uri = (char*)malloc(HEADBUF))) {
            printf("memory for uri not allocated\n");
            return nullptr;
        }
    }

    if(ob->path == nullptr) {
        if(!(ob->path = (char*)malloc(HEADBUF))) {
            printf("memory for path not allocated\n");
            return nullptr;
        }
    }

    if(ob->query == nullptr) {
        if(!(ob->query = (char*)malloc(HEADBUF))) {
            printf("memory for query not allocated\n");
            return nullptr;
        }
    }

    if(ob->ext == nullptr) {
        if(!(ob->ext = (char*)malloc(16))) {
            printf("memory for ext not allocated\n");
            return nullptr;
        }
    }

    // printf("POS:%d\n", pos);

    char* src           = p;
    char* internal_p    = p;
    int   pos_before    = pos;
    int   pos_internal  = 0;
    bool  find_redirect = false;
    char* new_uri       = redirects(p, ob, src, pos);

    if(new_uri != nullptr) {
        // printf("find redirect: %s\n", new_uri);
        // printf("uri: %s\n", p);

        if (new_uri[strlen(new_uri) - 1] == '=') {
            printf("%s!\n", new_uri);
        }

        pos_internal  = src - p + 1; // add escape symbol
        find_redirect = true;
        p = new_uri;
    }

    int arr[HEADBUF];
    int arr_pos = 0;
    int k       = 0;
    int range   = 0;
    
    src         = p;
    char* st    = ob->uri;

    char* dst   = ob->uri;
    char* path  = ob->path;
    char* ext   = nullptr;
    char* query = nullptr;

    ob->query[0] = 0;
    ob->ext[0]   = 0;

    bool find_query = false;

    if(*src != '/') {

        if( src[0] == '.' ) {

            if(src[1] == '/' && src[2] != ' ' && src[2] != '\0') {
                src += 1;
                pos += 1;
            }
            else if(src[1] == '/' && (src[2] == ' ' || src[2] == '\0')) {

                src += 2;
                pos += 2;

                *dst++ = '/';
                *path++ = '/';
                arr[arr_pos++] = k++;
            }
            else if( src[1] == ' ' || src[1] == '\0' || src[1] == '?' || src[1] == '#' ) {

                src += 1;
                pos += 1;

                *dst++ = '/';
                *path++ = '/';
                arr[arr_pos++] = k++;
            }
            else if( src[1] == '.' && src[2] == '/' && src[3] != ' ' && src[3] != '\0' ) {
    
                src += 2;
                pos += 2;
            }
            else if( src[1] == '.' && src[2] == '/' && (src[3] == ' ' || src[3] == '\0') ) {
    
                src += 3;
                pos += 3;

                *dst++ = '/';
                *path++ = '/';
                arr[arr_pos++] = k++;
            }
            else if( src[1] == '.' && (src[2] == ' ' || src[2] == '?' || src[2] == '#' || src[2] == '\0') ) {

                src += 2;
                pos += 2;

                *dst++ = '/';
                *path++ = '/';
                arr[arr_pos++] = k++;
            }
            else {
                // src += 1;
                // pos += 1;

                *dst++ = '/';
                *path++ = '/';
                arr[arr_pos++] = k++;
            }
        }
        else {
            *dst++  = '/';
            *path++ = '/';
            arr[arr_pos++] = k++;
        }
    }

    // parse path
    while(*src != ' ' && *src != '\0' && pos < HEADBUF) {

        if((int)(*src) < 33 || (int)(*src) > 126) {
            src++;
            pos++;
            continue;
        }

        if(!find_query && (*src == '/')) {

            ext = nullptr;

            if( src[1] == '/' ) {

                src++;
                pos++;
                continue;
            }

            if( src[1] == '.' && (src[2] == '/' || src[2] == '?' || src[2] == '#' || src[2] == '\0' || src[2] == ' ') ) {

                src += 2;
                pos += 2;
                continue;
            }


            if( src[1] == '.' && src[2] == '.' && (src[3] == '/' || src[3] == '?' || src[3] == '#' || src[3] == '\0' || src[3] == ' ') ) {

                path -= dst - st;

                dst = st;

                arr_pos--;

                if(arr_pos > 0) {

                    range = arr[arr_pos] - arr[arr_pos - 1];

                    st -= range;

                    k -= range;

                }
                else {
                    arr_pos = 0;

                    k = 0;
                }

                src += 3;

                pos += 3;

                continue;
            }

            
            arr[arr_pos] = k;

            arr_pos++;

            st = dst;

            *path++ = *src;

            *dst++ = *src++;

            pos++;
        }
        else if(!find_query && *src == '?') {

            if(k == 0) {

                *dst++ = '/';

                *path++ = '/';
            }

            *dst++ = *src++;

            src[-1] = 0;

            find_query = true;

            query = dst;

            pos++;
        }
        else if(*src == '#') {
            break;
        }
        else if(!find_query && *src == '.') {

            *path++ = *src;

            if(*(path - 2) != '/') {
                ext = path;
            }

            *dst++ = *src++;

            pos++;
        }
        else {

            if(!find_query) *path++ = *src;

            *dst++ = *src++;

            pos++;
        }

        k += 1;
    }

    // printf("pos: %d\n", pos);

    if(pos >= HEADBUF) {
        printf("end of string 1\n");
        if (new_uri != nullptr) free(new_uri);
        return nullptr;
    }

    while(pos < HEADBUF) {

        if(*src == ' ') {
            src++;
            pos++;
            break;
        }
        else if(*src == '\0') {
            printf("end of string 2, %s!\n", ob->uri);
            if (new_uri != nullptr) free(new_uri);
            return nullptr;
        }

        src++;
        pos++;
    }

    if (new_uri != nullptr) free(new_uri);

    if(k == 0) {

        *dst++ = '/';

        *path++ = '/';
    }

    if(k > 2 && *(dst-1) == '/' && !find_query) {
        *dst-- = 0;
    }

    if(path - ob->path > 2 && ob->path[0] == '/' && *(path-1) == '/') {
        *path-- = 0;
    }

    *dst++ = 0;

    *path++ = 0;

    if(query != nullptr) {

        size_t l = dst - query;

        memmove(ob->query, dst - l, l);
    }
    
    if(ext != nullptr) {

        size_t l = path - ext;

        if(l > 16) {

            memmove(ob->ext, ext, 15);

            ob->ext[15] = 0;
        }
        else {
            memmove(ob->ext, path - l, l);
        }
    }

    if(find_redirect) {

        pos = pos_before + pos_internal;

        src = internal_p + pos_internal;

        // printf("POS2:%d\n", pos);
    }

    // printf("P:%s\n", src);

    // printf("%d, URI:%s\n", gettid(), ob->uri);

    // printf("%d, PATH:%s\n", gettid(), ob->path);

    // printf("%d, QUERY:%s\n", gettid(), ob->query);

    // printf("%d, EXT:%s\n\n\n", gettid(), ob->ext);

    return src;
}

char* setHttpVersion(obj* ob, char* p, int& pos) {

    if(p[0] == 'H' && p[1] == 'T' && p[2] == 'T' && p[3] == 'P' && p[4] == '/' && p[5] == '1' && p[6] == '.') {
        if(p[7] == '0') {
            ob->version = HTTP_1_0;
        }
        else if(p[7] == '1') {
            ob->version = HTTP_1_1;
        }
        else {
            printf("error http version\n");
            return nullptr;
        }
    }
    else {
        printf("error http version\n");
        return nullptr;
    }

    if(p[8] != '\r' || p[9] != '\n') {
        printf("error parse first line\n");
        return nullptr;
    }

    pos += 10;

    return p + 10;
}

void setHeader(char* buf, unsigned int& headersPos, const char* key, const char* value, size_t key_len, size_t value_len) {
    
    size_t len;

    if(key_len > 0) {
        len = key_len;
        memcpy(buf + headersPos, key, key_len);
    }
    else {
        len = strlen(key);
        memcpy(buf + headersPos, key, len);
    }

    headersPos += len;

    memcpy(buf + headersPos, ": ", 2);

    headersPos += 2;

    if(value_len > 0) {
        len = value_len;
        memcpy(buf + headersPos, value, value_len);
    }
    else {
        len = strlen(value);
        memcpy(buf + headersPos, value, len);
    }

    headersPos += len;

    memcpy(buf + headersPos, "\r\n", 2);

    headersPos += 2;
};

char* standardResponse(obj* p_obj) {

    p_obj->response_body = (char*)malloc(HEADBUF);

    if(p_obj->response_body == nullptr) return nullptr;

    char* content_len = (char*)malloc(32);

    if(content_len == nullptr) {
        free(p_obj->response_body);
        return p_obj->response_body = nullptr;
    }

    unsigned int headers_pos = 37;

    const char* str1 = "<html><head></head><body><h1 style=\"text-align:center;margin:20px\">";
    const char* str2 = "</h1></body></html>";

    pair_char_int status = http_status_text[p_obj->status];

    setHeader(p_obj->response_body, headers_pos, "Cache-Control",                "no-cache",                                           13,   8);
    setHeader(p_obj->response_body, headers_pos, "Connection",                   "keep-alive",                                         10,  10);
    setHeader(p_obj->response_body, headers_pos, "Strict-Transport-Security",    "max-age=31536000",                                   25,  16);
    setHeader(p_obj->response_body, headers_pos, "Content-Type",                 "text/html; charset=UTF-8",                           12,  24);
    setHeader(p_obj->response_body, headers_pos, "Content-Length",               itoa(67 + status.length - 2 + 19, content_len),   14);

    free(content_len);

    unsigned int body_length = 67 + status.length - 2 + 19;

    writeHeadersInHead(p_obj, status, headers_pos, body_length);

    memmove(p_obj->response_body + p_obj->response_head_length,                           str1,        67);
    memmove(p_obj->response_body + p_obj->response_head_length + 67,                      status.key,  status.length - 2);
    memmove(p_obj->response_body + p_obj->response_head_length + 67 + status.length - 2,  str2,        19);

    return p_obj->response_body;
}

char* standardResponseAsync(obj* p_obj) {

    p_obj->response_body = (char*)malloc(HEADBUF);

    if(p_obj->response_body == nullptr) return nullptr;

    char* content_len = (char*)malloc(32);

    if(content_len == nullptr) {
        free(p_obj->response_body);
        return p_obj->response_body = nullptr;
    }

    unsigned int headers_pos = 37;

    const char* str1 = "{\"success\":\"false\",\"message\":\"";
    const char* str2 = "\"}";

    pair_char_int status = http_status_text[p_obj->status];

    setHeader(p_obj->response_body, headers_pos, "Cache-Control",                "no-cache",                                           13,   8);
    setHeader(p_obj->response_body, headers_pos, "Connection",                   "keep-alive",                                         10,  10);
    setHeader(p_obj->response_body, headers_pos, "Strict-Transport-Security",    "max-age=31536000",                                   25,  16);
    setHeader(p_obj->response_body, headers_pos, "Content-Type",                 "application/json; charset=UTF-8",                    12, 31);
    setHeader(p_obj->response_body, headers_pos, "Content-Length",               itoa(30 + status.length - 2 + 2, content_len),        14);

    free(content_len);

    unsigned int body_length = 30 + status.length - 2 + 2;

    writeHeadersInHead(p_obj, status, headers_pos, body_length);

    memmove(p_obj->response_body + p_obj->response_head_length,                           str1,        30);
    memmove(p_obj->response_body + p_obj->response_head_length + 30,                      status.key,  status.length - 2);
    memmove(p_obj->response_body + p_obj->response_head_length + 30 + status.length - 2,  str2,        2);

    return p_obj->response_body;
}

void writeHeadersInHead(obj* p_obj, const pair_char_int& status, unsigned int& headers_pos, unsigned int body_length) {

    unsigned int offset = 28 - status.length; // 37 - 9 = 28

    memcpy(p_obj->response_body + offset, "HTTP/1.1 ", 9);

    memcpy(p_obj->response_body + offset + 9, status.key, status.length);

    memcpy(p_obj->response_body + headers_pos, "\r\n", 2);

    headers_pos += 2;

    p_obj->response_head_length = headers_pos - offset;

    p_obj->response_body_length = p_obj->response_head_length + body_length;

    memmove(p_obj->response_body, p_obj->response_body + offset, p_obj->response_head_length);
}

bool setHeaders(obj* ob, char* p, int& pos) {

    char* key        = p;
    char* key_end    = p;
    char* value      = nullptr;
    char* value_end  = nullptr;
    bool  find_sep   = false;
    bool  find_value = false;
    bool  find_host  = false;

    while(*p != '\0') {

        if(pos >= HEADBUF) return false;

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

            key_end = p;

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

            value_end = p;

            *p = 0;

            p += 2;

            find_value = false;

            find_sep   = false;

            key = toLowerCase(key);

            if(key[0] == 'c' && key[7] == '-' && key[13] == 'h') {
                ob->request_body_length = atoi(value);
                // printf("длина контента получена: %d\n", atoi(value));
            }
            else if(key[0] == 'c' && key[1] == 'o' && key[2] == 'n' && key[3] == 'n') {
                if(strcmp(toLowerCase(value), "keep-alive") == 0) {
                    ob->keep_alive = true;
                    // printf("connection is keep-alive\n");
                }
                else {
                    ob->keep_alive = false;
                    // printf("connection is not keep-alive\n");
                }
            }
            else if(key[0] == 'h' && key[1] == 'o' && key[2] == 's' && key[3] == 't') {

                char* v = toLowerCase(value);

                
                if(
                    strcmp(v, "www.galleryhd.ru") == 0
                ) {
                    ob->redirect = 301;
                }
                // else if(strcmp(v, "galleryhd.ru") != 0) {
                //     ob->status = 404;
                // }

                find_host = true;
            }

            short key_len   = key_end - key + 1; // with \0
            short value_len = value_end - value + 1; // with \0

            // printf("key `%s` = %d = %d\n", key, strlen(key), key_end - key);
            // printf("value `%s` = %d = %d\n", value, strlen(value), value_end - value);

            char* k = (char*)malloc(key_len);
            char* v = (char*)malloc(value_len);

            memmove(k, key, key_len);
            memmove(v, value, value_len);

            // printf("%s -> %s\n", key, value);

            ob->headers[simpleHash(key)] = {k, v};

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

    // printf("pos %d\n", pos);

    if(p[0] != '\r' && p[1] != '\n') {
        printf("end of headers not found\n");
        return false;
    }

    if(!find_host) {
        ob->status = 400;
    }

    return true;
}

char* getHeader(const char* key, obj* ob) {

    if(strlen(key) > 63) {
        printf("header key is greater than 63 bytes\n");
        return nullptr;
    }

    unsigned int c_pos = 0;

    char* str = (char*)malloc(64);

    if(str == nullptr) return nullptr;

    while(key[c_pos]) {
        str[c_pos] = tolower(key[c_pos]);
        c_pos++;
    }

    str[c_pos] = 0;

    unordered_map<double, pair_chars>::iterator it = ob->headers.find(simpleHash(str));

    free(str);

    if(it == ob->headers.end()) {
        return nullptr;
    }

    return it->second.value;
}

short processingHeaderRange(char* range, unsigned int& file_size, unsigned int& file_part_size, char* content_length, obj* p_obj, unsigned int& headers_pos) {

    unsigned int first_number  = 0;
    unsigned int second_number = 0;

    bool find_dash         = false;
    bool find_first_number = false;
    bool error             = true;
    
    if(range[0] == 'b' && range[1] == 'y' && range[2] == 't' && range[3] == 'e' && range[4] == 's' && range[5] == '=') {

        char* r = range + 6;

        char* r_s = r;

        while(*r != '\0') {

            if(*r >= 48 && *r <= 57) {
                r++;
            }
            else if(*r == 45){ // -

                if(find_dash) {
                    error = true;
                    break;
                }

                error = false;

                find_dash = true;

                *r = 0;

                if(!find_first_number && r - r_s > 0) {

                    first_number = strtoull(r_s, NULL, 10);

                    find_first_number = true;
                }

                *r = 45;

                r_s = ++r;
            }
            else if(*r == 44){ // ,
                error = true;
                break;
            }
            else {
                error = true;
                break;
            }
        }

        if(r - r_s > 0) {
            second_number = strtoull(r_s, NULL, 10);
        }
        else {
            second_number = file_size - 1;
        }

        if(!find_first_number) {

            if(second_number >= file_size) {
                second_number = file_size;
            }

            first_number = file_size - second_number;

            second_number = file_size - 1;
        }

        if(second_number >= file_size) {
            second_number = file_size - 1;
        }
    }
    else {
        error = true;
    }

    if(first_number >= file_size || first_number > second_number) error = true;

    if(!error) {

        char* mem_content_range = (char*)malloc(32);

        if(mem_content_range == nullptr) {
            return -1;
        }

        char* string_content_range = (char*)malloc(64);

        if(string_content_range == nullptr) {
            free(mem_content_range);
            return -1;
        }

        strcpy(string_content_range, "bytes ");

        strcat(string_content_range, itoa(first_number, mem_content_range));

        strcat(string_content_range, "-");

        file_part_size = second_number - first_number + 1;

        strcat(string_content_range, itoa(second_number, mem_content_range));

        free(mem_content_range);

        strcat(string_content_range, "/");

        strcat(string_content_range, content_length);

        setHeader(p_obj->response_body, headers_pos, "Content-Range", string_content_range, 13);

        free(string_content_range);

        p_obj->file_offset = first_number;

        setHeader(p_obj->response_body, headers_pos, "Content-Length", itoa(file_part_size, content_length), 14);
        setHeader(p_obj->response_body, headers_pos, "Cache-Control",  "no-cache",                           13, 8);
    }
    else {
        return -2;
    }

    return 1;
}

bool mkdirTree(const char* bs_path, const char* path) {

    size_t path_len    = strlen(path);
    size_t bs_path_len = strlen(bs_path);

    if (path_len == 0) {
        return false;
    }

    struct stat stat_obj;

    char* local_path = (char*)malloc(bs_path_len + path_len + 1);
    char* p_path = (char*)path;
    char* p_local_path = local_path + bs_path_len;

    strcpy(local_path, bs_path);

    if (local_path[bs_path_len - 1] != '/') {
        *p_local_path++ = '/';
    }
    
    if (*p_path == '/') {
        p_path++;
    }

    while (*p_path) {

        *p_local_path++ = *p_path;

        if (*p_path == '/') {
            p_path++;
            break;
        }

        p_path++;
    }

    *p_local_path = 0;

    // printf("%s\n", local_path);

    if(stat(local_path, &stat_obj) == -1) {
        int mkd = mkdir(local_path, S_IRWXU);

        if(mkd == -1) {
            printf("%s, %s\n", local_path, strerror(errno));
            free(local_path);
            return true;
        }
    }

    if (*p_path != 0) {
        mkdirTree(local_path, p_path);
    }

    free(local_path);

    return true;
}

} // namespace