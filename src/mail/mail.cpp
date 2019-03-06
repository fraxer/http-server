#include "mail.h"

using namespace std;

namespace server {

Mail::Mail() {

    private_key = (char*)"-----BEGIN RSA PRIVATE KEY-----\nMIIEpAIBAAKCAQEAiRtS9k+I1E/wz3Uxn1LJNcZAkBQeiaHizplbGTdrGh08+ZNK\nrRUv6dhNqT/OH1J8HrbVKe6Ta//snLJkjzWfPO9iqjz1nKcVpmLoIwoFnT668tCV\nVRmNOjNjn3Stz7SB4iTZEabX6CqE4oG4yjMF1kqCFO/btrNCyB4ZzYq99aGKzDSD\n90xNuAg1adfBVhT1wMbr2VuyXWdFXoTjEKp268EGoAjifkoTPyp2mgnG4rXqrVz4\nL7NFf9o9Bj/l35QbzvJAcwdW5vMFvYjUFzirmEf761vky6qUHMTdRpvw2qWeOj9f\nT+hEUOP9Yo6JfQt8YVtV7hBTTJoDEL2T/As1ywIDAQABAoIBAA7Y56YGvOGI+qHs\npgAD3gg1vN4dX6U147LNxqC3cKC2U9kPC0ItDjA7nUVbxH192DUXRYQx8sL6Ruw9\nIUik3OajYcLfsRVSvrRy94J73lnF5kbPMayyhKmt0Pb+kqfwezQ7G0qfD7hzikph\nE9rJpbMsBGQyCVACn5O+NrsccC++MwtCHzl7GickRRxdI5368UcCI1j9XiRwcBrX\ngRjjtt4pozo97tbYblT6MQS6L3SAuUFckh+cwTZC+8E84CK+iRn9GcCLovw/oyKo\nyFBjGSy5xQX+lJ6jOBamF+yaEyOX4Qtrj79zgM66spZX9usDxq9GBC4eEzPv5e0w\nE4cGJNECgYEAwH0/J4pMcvp2AB0mg99GX/zbp6AoIenjsqGBhgcxfv40yY0F5ma6\nCEOThk+zNTa4+HdEdmG0PgrA3ML3pjXDDpnCY0MeWb/qSjUuf5TyLYgPgBkUCFsm\nrKfQ0RZuiGoHPghu8KBIBmlO8NweyvRvjcBBJoOmS+MDrucqmX25XBMCgYEAtlgm\nlNHqeisIelFWKPibwDksN/RZ3hWQCMpfrDxpkxy/TUZV8knfbhe+HM4+nGV2MQJE\nY395B5CmmH2NGIoV4bj8OAc1FKzfj59xDmEuzyibZqfpL+a6Rh0KUbWItuhW7leF\n8EEzqDL9W0SWAbxYxxfTG5uI0pEwrxiDHgU0BmkCgYA9syfeNb9hj5TpXm6tEJGP\nTQ7fgT+79yusY4aP/phH+5XSESkV/FGfOoH8KGnqIbtSsXA5fgH3bRz65mfZWBxI\n/tJHXQlKfXDNDpT0AjtKivVk+yTntMgFydjuXIFbqpjdsnhVGxtbKsBKBlS8e3OS\nfVCk1sgkRQU2OBT4lEspDwKBgQCFcrARvCspa7MRqdMvuvppzK3S6Y6XnRpDhLBO\nIgx18NUEndQqtOvC67dj54Ek/pBoP6uDUfhmk/OIqGIJso1fG/3il0u+rOIUf3DD\nQFZ8n9BTadGcD/UFeR1jPUMn8ZQlbIKGmYFPuLl5ARHzhT9HveTvUH3q9P03N+5F\nmzM++QKBgQCEdr+4qbF5cDc/e8nZHoBExGl8oFZBmo/WU43mqalYnsf8Xdkw5aCD\nTp/ajYm0LooYhRMavBDG0Ds/cHlMjv3lFt21pxDWg8383/BxkYmjzivi2chGgvE7\nZ53tK9Ie+ez5nEwTqXcGV6WX2bfNkQeWw33UPNODriAsi4qnVaJNxQ==\n-----END RSA PRIVATE KEY-----";
    domain      = (char*)"galleryhd.ru";
    selector    = (char*)"galleryhd";

	headers_size = 5;

    socket = -1;

	headers = (stringpair**)malloc(sizeof(stringpair*) * headers_size);

	for(int i = 0; i < headers_size; ++i) {
        headers[i] = (stringpair*)malloc(sizeof(stringpair));
    }

    ddata = nullptr;
    hostname = (char*)malloc(128);
    from = (char*)malloc(128);
    to = (char*)malloc(128);
    date = (char*)malloc(128);
    message_id = (char*)malloc(128);

    len_ddata      = 0;
    len_from       = 0;
	len_to         = 0;
	len_subject    = 0;
	len_date       = 0;
	len_message_id = 0;

};

Mail::~Mail() {

    if(socket > -1) {
        ::send(socket, "QUIT\r\n", 6, MSG_DONTWAIT);
    }

    for (int i = 0; i < headers_size; ++i)
    	free(headers[i]);

    free(headers);
    free(from);
    free(to);
    free(date);
    free(message_id);
    free(hostname);

    if(ddata != nullptr) {
        free(ddata);
        ddata = nullptr;
    }

    if(socket > -1) {
        close(socket);
    }
};

bool Mail::makeSocketNonblocking(int& socketfd) {

    int flags = fcntl(socketfd, F_GETFL, 0);

    if(flags == -1) {
        printf("[E] fcntl failed (F_GETFL)\n");
        return false;
    }

    flags |= O_NONBLOCK;

    int s = fcntl(socketfd, F_SETFL, flags);

    if(s == -1) {
        printf("[E] fcntl failed (F_SETFL)\n");
        return false;
    }

    return true;
}

void Mail::parseMxRecord(unsigned char *buffer, size_t r, ns_sect s, int idx, ns_msg *m, map<int, mx_domain*>& mx_records) {

    ns_rr rr;

    if(ns_parserr(m, s, idx, &rr) == -1) {
        std::cerr << "error " << errno << ": " << strerror(errno) << "\n";
        return;
    }

    const size_t size = NS_MAXDNAME;

    unsigned char name[size];

    const u_char* data = ns_rr_rdata(rr);

    if(ns_rr_type(rr) != ns_t_mx) return;

    int pref = ns_get16 (data);

    ns_name_unpack(buffer, buffer + r, data + sizeof(u_int16_t), name, size);

    char name2[size];

    ns_name_ntop(name, name2, size);

    struct hostent* he;

    if((he = gethostbyname(name2)) == NULL) {
        // get the host info
        herror("gethostbyname");
        return;
    }
 
    struct in_addr** addr_list = (struct in_addr **)he->h_addr_list;

    vector<struct in_addr> ip_list;

    for(int i = 0; addr_list[i] != NULL; i++) {
        ip_list.push_back(*addr_list[i]);
    }

    // std::cout << pref << " " << name2;

    mx_domain* obj = new mx_domain();

    *obj = {(int)ns_rr_ttl(rr), name2, ip_list};

    mx_records[pref] = obj;
};

int Mail::getMXServers(const char* host, map<int, mx_domain*>& mx_records) {

    union {
        HEADER hdr;              /* defined in resolv.h */
        u_char buf[NS_PACKETSZ]; /* defined in arpa/nameser.h */
    } buffer;

    int bufferLen = res_query (host, ns_c_in, ns_t_mx, (u_char*)&buffer, sizeof(buffer));

    if(bufferLen == -1) {
        // std::cerr << "1 error " << h_errno << ": " << hstrerror (h_errno) << "\n";
        return 0;
    }

    if(buffer.hdr.rcode != NOERROR) {

        // std::cerr << "Error: ";

        // switch (buffer.hdr.rcode) {
        //     case FORMERR:
        //         std::cerr << "Format error";
        //         break;
        //     case SERVFAIL:
        //         std::cerr << "Server failure";
        //         break;
        //     case NXDOMAIN:
        //         std::cerr << "Name error";
        //         break;
        //     case NOTIMP:
        //         std::cerr << "Not implemented";
        //         break;
        //     case REFUSED:
        //         std::cerr << "Refused";
        //         break;
        //     default:
        //         std::cerr << "Unknown error";
        // }

        return 0;
    }

    int answers = ntohs(buffer.hdr.ancount);

    ns_msg m;

    if(ns_initparse(buffer.buf, bufferLen, &m) == -1) {
        // std::cerr << "2 error " << errno << ": " << strerror(errno) << "\n";
        return 0;
    }

    ns_rr rr;

    if(ns_parserr(&m, ns_s_qd, 0, &rr) == -1) {
        // std::cerr << "3 error " << errno << ": " << strerror(errno) << "\n";
        return 0;
    }

    for(int i = 0; i < answers; ++i) {
        parseMxRecord(buffer.buf, bufferLen, ns_s_an, i, &m, mx_records);
    }

    return 1;
};

bool Mail::isRealService(const char* email) {
    const char* host = strchr(email, '@');

    if(host == nullptr) {
        return false;
    }

    ++host;

    int socket = ::socket(AF_INET, SOCK_STREAM, 0);

    if(socket == -1) {
        return false;
    }

    union {
        HEADER hdr;              /* defined in resolv.h */
        u_char buf[NS_PACKETSZ]; /* defined in arpa/nameser.h */
    } buffer;

    int bufferLen = res_query (host, ns_c_in, ns_t_mx, (u_char*)&buffer, sizeof(buffer));

    if(bufferLen == -1) {
        close(socket);
        return false;
    }

    if(buffer.hdr.rcode != NOERROR) {
        close(socket);
        return false;
    }

    int answers = ntohs(buffer.hdr.ancount);

    ns_msg m;

    if(ns_initparse(buffer.buf, bufferLen, &m) == -1) {
        close(socket);
        return false;
    }

    ns_rr rr;

    if(ns_parserr(&m, ns_s_qd, 0, &rr) == -1) {
        close(socket);
        return false;
    }

    bool flag = false;

    for(int i = 0; i < answers; ++i) {

        ns_rr rr;

        if(ns_parserr(&m, ns_s_an, i, &rr) == -1) {
            continue;
        }

        const size_t size = NS_MAXDNAME;

        unsigned char name[size];

        const u_char* data = ns_rr_rdata(rr);

        if(ns_rr_type(rr) != ns_t_mx) continue;

        // int pref = ns_get16(data);

        ns_name_unpack(buffer.buf, buffer.buf + bufferLen, data + sizeof(u_int16_t), name, size);

        char name2[size];

        ns_name_ntop(name, name2, size);

        struct hostent* he;

        if((he = gethostbyname(name2)) == NULL) {
            continue;
        }
        else {
            flag = true;
            break;
        }

    }

    close(socket);

    return flag;
};

int Mail::createConnection(char* domain, u_short port) {

    char* p = strchr(domain, '@');

    if(p == nullptr) {
        // printf("not detect domain from email: %s\n", domain);
        return 0;
    }

    ++p;

    socket = ::socket(AF_INET, SOCK_STREAM, 0);

    if(socket == -1) {
        // printf("Error in socket\n");
        return 0;
    }

    // cout << "DOMAIN: " << p << endl;

    map<int, mx_domain*> mx_records;

    if(!getMXServers(p, mx_records)) {
        return 0;
    }

    if(mx_records.size() == 0) {
        // printf("not found servers\n");
        return 0;
    }

    map<int, mx_domain*>::iterator it = mx_records.begin();

    // cout << it->second->ip_list[0].s_addr << endl;

    struct in_addr ip_addr = it->second->ip_list[0];

    sockaddr_in sockaddr;

    bzero((void*)&sockaddr,sizeof(sockaddr));

    sockaddr.sin_family      = AF_INET;
    sockaddr.sin_port        = htons(port);
    sockaddr.sin_addr.s_addr = ip_addr.s_addr;

    if(connect(socket, (struct sockaddr*) &sockaddr, sizeof(sockaddr)) == -1) {
        // printf("Error in connect\n");
        return 0;
    }

    for(auto &x : mx_records) {
        delete x.second;
    }

    mx_records.clear();

    makeSocketNonblocking(socket);

    sendCommand((char*)"EHLO galleryhd.ru\r\n");

    getMessage();

    this->connected = true;

    return 1;
};

bool Mail::isConnected() {
    return this->connected;
};

void Mail::closeConnection() {

    this->connected = false;

    if(socket > -1) {
        close(socket);
    }
};

void Mail::getMessage() {

	char status[256];

    ssize_t r;
    int i = 0;

    // printf("get new message\n");

    while((r = recv(socket, status, sizeof(status), MSG_DONTWAIT))) {

        if(i++ > 100) {
            break;
        }

        if(r > 0) {
            // status[r] = 0;
            // printf("status: %s\n", status);
            break;
        }

        if(r < 0) {
            usleep(10000);
            continue;
        }
    }
};

void Mail::sendCommand(char* command) {

    ::send(socket, command, strlen(command), MSG_DONTWAIT);

    // printf("%s", command);

    getMessage();
};

void Mail::sendFrom() {

    char text[128];

	memcpy(text, "MAIL FROM:", 10);
    memcpy(text + 10, from, len_from);
    memcpy(text + 10 + len_from, "\r\n\0", 3);

    sendCommand(text);
};

void Mail::sendTo() {

	char text[128];

	memcpy(text, "RCPT TO:", 8);
    memcpy(text + 8, to, len_to);
    memcpy(text + 8 + len_to, "\r\n\0", 3);

    sendCommand(text);
};

void Mail::sendData() {

    ::send(socket, ddata, len_ddata, MSG_DONTWAIT);

    // printf("%s\n", ddata);

    getMessage();
};

void Mail::setFrom(const char* value) {

    this->len_from = strlen(value);

    memcpy(this->from, "<", 1);
    memcpy(this->from + 1, value, this->len_from);
    memcpy(this->from + 1 + this->len_from, ">\0", 2);

    this->len_from += 2;

    headers[0]->key = (char*)"From";
    headers[0]->value = this->from;
};

void Mail::setTo(const char* value) {

    this->len_to = strlen(value);

	memcpy(this->to, "<", 1);
    memcpy(this->to + 1, value, this->len_to);
    memcpy(this->to + 1 + this->len_to, ">\0", 2);

    this->len_to += 2;

    headers[1]->key = (char*)"To";
    headers[1]->value = this->to;

    strcpy(hostname, value);
};

void Mail::setSubject(const char* value) {

	this->len_subject = strlen(value);

	headers[2]->key   = (char*)"Subject";
	headers[2]->value = (char*)value;
};

void Mail::setData(const char* value) {

	char* base64_data = base64_encode((const unsigned char*)value, strlen(value));

	size_t len_data = strlen(base64_data);

	if (!len_data) return;

	int num_sep = (len_data / 76) * 2;

	char* data = (char*)malloc(len_data + num_sep + 1);

	size_t pos_value     = 0;
	size_t cur_pos_value = 0;
	size_t pos_data      = 0;

    short counter = 0;

    while(cur_pos_value < len_data) {

        if(counter == 76) {

        	memcpy(data + pos_data, base64_data + pos_value, cur_pos_value - pos_value);

        	pos_data += cur_pos_value - pos_value;

        	data[pos_data]     = '\r';
        	data[pos_data + 1] = '\n';
        	
        	pos_data += 2;

            pos_value = cur_pos_value;

            counter = 0;
        }

        counter++;

        cur_pos_value++;
    }

    memcpy(data + pos_data, base64_data + pos_value, cur_pos_value - pos_value);

    len_data = pos_data + cur_pos_value - pos_value;

	data[len_data] = 0;

	free(base64_data);

	int pos = 0;

    char date[64]; // Sun, 13 May 2018 21:53:00 +0600
    char mid[64];  // <20180513215300@galleryhd.ru>

    time_t rawtime;

    time(&rawtime);

    struct tm * timeinfo = localtime(&rawtime);

    strftime(date, 80, "%a, %d %b %Y %T +0600",       timeinfo);
    strftime(mid,  80, "<%Y%m%d%H%M%S@galleryhd.ru>", timeinfo);

    headers[3]->key = (char*)"Date";
    headers[3]->value = date;

    headers[4]->key = (char*)"Message-Id";
    headers[4]->value = mid;

    len_date       = strlen(date);
    len_message_id = strlen(mid);


    ddata = (char*)realloc(ddata, len_data + 1024);

	memcpy(ddata + pos, headers[0]->key, 4);                 pos += 4;
    memcpy(ddata + pos, ": ", 2);                            pos += 2;
    memcpy(ddata + pos, headers[0]->value, len_from);        pos += len_from;
    memcpy(ddata + pos, "\r\n", 2);                          pos += 2;

	memcpy(ddata + pos, headers[1]->key, 2);                 pos += 2;
    memcpy(ddata + pos, ": ", 2);                            pos += 2;
    memcpy(ddata + pos, headers[1]->value, len_to);          pos += len_to;
    memcpy(ddata + pos, "\r\n", 2);                          pos += 2;

	memcpy(ddata + pos, headers[2]->key, 7);                 pos += 7;
    memcpy(ddata + pos, ": ", 2);                            pos += 2;
    memcpy(ddata + pos, headers[2]->value, len_subject);     pos += len_subject;
    memcpy(ddata + pos, "\r\n", 2);                          pos += 2;

	memcpy(ddata + pos, headers[3]->key, 4);                 pos += 4;
    memcpy(ddata + pos, ": ", 2);                            pos += 2;
    memcpy(ddata + pos, headers[3]->value, len_date);        pos += len_date;
    memcpy(ddata + pos, "\r\n", 2);                          pos += 2;

	memcpy(ddata + pos, headers[4]->key, 10);                pos += 10;
    memcpy(ddata + pos, ": ", 2);                            pos += 2;
    memcpy(ddata + pos, headers[4]->value, len_message_id);  pos += len_message_id;
    memcpy(ddata + pos, "\r\n", 2);                          pos += 2;


	char* dkim = dkim_create(headers, headers_size, data, private_key, domain, selector, 0);

	size_t len_dkim = strlen(dkim);

    

    memcpy(ddata + pos, "DKIM-Signature: ", 16);                            pos += 16;
    memcpy(ddata + pos, dkim, len_dkim);                                    pos += len_dkim;
    memcpy(ddata + pos, "\r\n", 2);                                         pos += 2;
    memcpy(ddata + pos, "MIME-Version: 1.0\r\n", 19);                       pos += 19;
    memcpy(ddata + pos, "Content-Transfer-Encoding: base64\r\n", 35);       pos += 35;
    memcpy(ddata + pos, "Content-Type: text/html; charset=utf-8\r\n", 40);  pos += 40;
    memcpy(ddata + pos, "\r\n", 2);                                         pos += 2;
    memcpy(ddata + pos, data, len_data);                                    pos += len_data;
    memcpy(ddata + pos, "\r\n.\r\n", 5);                                    pos += 5;

    len_ddata = pos;

    free(dkim);
    free(data);
};

void Mail::send() {

    // if(!createConnection(hostname, 25)) {
    //     printf("connection not established\n");
    //     return;
    // }

    sendFrom();
    sendTo();
    sendCommand((char*)"DATA\r\n");
    sendData();

	len_ddata      = 0;
	len_from       = 0;
	len_to         = 0;
	len_subject    = 0;
	len_date       = 0;
	len_message_id = 0;

    // close(socket);
    
};

} // namespace