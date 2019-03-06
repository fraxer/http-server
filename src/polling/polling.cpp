#include "polling.h"

using namespace std;

namespace server {

void Polling::create(obj* p_obj, int socket_fd, int epoll_fd, int user_id, char* uri) {

    unordered_map<int, polling_st*>::iterator it;

    string string_uri = uri;

    pthread_mutex_lock(&lock_poll);

    for (it = polling_array->begin(); it != polling_array->end(); ++it) {

        if (it->second->user_id == user_id && *it->second->uri == string_uri) {
            break;
        }
    }

    if (it != polling_array->end()) {

        pthread_mutex_lock(it->second->mutex);

        it->second->active   = true;
        it->second->epoll_fd = epoll_fd;
        it->second->user_id  = user_id;
        it->second->uri->assign(uri);
        it->second->p_obj = p_obj;

        pthread_mutex_unlock(it->second->mutex);

    } else {

        pthread_mutex_t* mutex = new pthread_mutex_t();

        pthread_mutex_init(mutex, nullptr);

        polling_st* polling = new polling_st({
            mutex,
            true,
            epoll_fd,
            user_id,
            new string(uri),
            new vector<string*>(),
            p_obj
        });

        polling_array->emplace(socket_fd, polling);
    }

    pthread_mutex_unlock(&lock_poll);
}

void Polling::addInQueue(int epoll_fd, int user_id, char* uri, vector<string*>* data) {

    unordered_map<int, polling_st*>::iterator it;

    string string_uri = uri;

    pthread_mutex_lock(&lock_poll);

    for (it = polling_array->begin(); it != polling_array->end(); ++it) {

        if (it->second->user_id != user_id || *it->second->uri != string_uri) continue;

        pthread_mutex_lock(it->second->mutex);

        ssize_t data_size = data->size();

        for (int i = 0; i < data_size; i++) {
            it->second->data->push_back(new string(*data->at(i)));
        }

        it->second->active   = true;
        it->second->epoll_fd = epoll_fd;
        it->second->user_id  = user_id;
        it->second->uri->assign(uri);

        pthread_mutex_unlock(it->second->mutex);

        pthread_cond_signal(&cond_poll);

        break;
    }

    pthread_mutex_unlock(&lock_poll);
}

void Polling::joinConnection(int epoll_fd, int socket_fd) {

    epoll_event event;

    event.events  = EPOLLOUT;
    event.data.fd = socket_fd;

    if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, socket_fd, &event) == -1) {
        printf("[E] epoll_ctl failed in statusThread, %d\n", errno);
    }
}

void Polling::freePoll(polling_st* st) {

    pthread_mutex_lock(st->mutex);

    st->active = false;
    st->epoll_fd = 0;
    st->user_id = 0;
    st->uri->erase();
    st->p_obj = nullptr;

    ssize_t data_size = st->data->size();

    for (int i = 0; i < data_size; i++) {
        // st->data->at(i)->erase();
        delete st->data->at(i);
    }

    st->data->clear();

    pthread_mutex_unlock(st->mutex);
}

void Polling::makeResult(polling_st* polling) {

    int   headersPos = 0;

    char* resultMemory = Polling::allocMemory(headersPos);

    if (resultMemory == nullptr) {
        polling->p_obj->status = 500;
        standardResponseAsync(polling->p_obj);
        return;
    }

    polling->p_obj->response_body = Polling::setContent(polling->p_obj, polling, resultMemory, headersPos);

    if (polling->p_obj->response_body == nullptr) {
        standardResponseAsync(polling->p_obj);
        return;
    }
}

void Polling::setHeader(char* resultMemory, int& headersPos, const char* key, const char* value, size_t key_len, size_t value_len) {
    
    size_t len;

    if(key_len > 0) {
        len = key_len;
        memcpy(resultMemory + headersPos, key, key_len);
    }
    else {
        len = strlen(key);
        memcpy(resultMemory + headersPos, key, len);
    }

    headersPos += len;

    memcpy(resultMemory + headersPos, ": ", 2);

    headersPos += 2;

    if(value_len > 0) {
        len = value_len;
        memcpy(resultMemory + headersPos, value, value_len);
    }
    else {
        len = strlen(value);
        memcpy(resultMemory + headersPos, value, len);
    }

    headersPos += len;

    memcpy(resultMemory + headersPos, "\r\n", 2);

    headersPos += 2;
}

char* Polling::allocMemory(int& headersPos) {

    char* resultMemory = (char*)malloc(HEADBUF);

    if(resultMemory == nullptr) return nullptr;

    headersPos = 37;

    Polling::setHeader(resultMemory, headersPos, "Cache-Control",               "no-cache",                  13,  8);
    Polling::setHeader(resultMemory, headersPos, "Connection",                  "keep-alive",                10, 10);
    Polling::setHeader(resultMemory, headersPos, "Content-Type",                "application/json; charset=UTF-8", 12, 31);

    // Polling::setHeader(resultMemory, headersPos, "Strict-Transport-Security",   "max-age=31536000",          25, 16);
    Polling::setHeader(resultMemory, headersPos, "Access-Control-Allow-Origin", "*",                         27,  1);
    Polling::setHeader(resultMemory, headersPos, "Access-Control-Allow-Headers", "Token",                    28,  5);
    // Polling::setHeader(resultMemory, headersPos, "Access-Control-Allow-Credentials", "true",                 32,  4);

    return resultMemory;
}

char* Polling::setContent(obj* p_obj, polling_st* polling, char* resultMemory, int& headersPos) {

    char* memContenLength = (char*)malloc(16);

    if(memContenLength == nullptr) {
        free(resultMemory);
        p_obj->status = 500;
        return nullptr;
    }

    unique_ptr<string> content(new string());

    ssize_t data_size = polling->data->size();

    for (int i = 0; i < data_size; i++) {
        content->append(",");
        content->append(polling->data->at(i)->c_str());
    }

    int contentLength = content->size();

    if (contentLength > 0) {
        content->at(0) = '[';
        content->append("]");

        contentLength++; // append ']'
    }



    Polling::setHeader(resultMemory, headersPos, "Content-Length", itoa(contentLength, memContenLength), 14);

    pair_char_int status = http_status_text[p_obj->status];

    unsigned int offset = 28 - status.length; // 37 - 9 = 38

    memcpy(resultMemory + offset, "HTTP/1.1 ", 9);

    memcpy(resultMemory + offset + 9, status.key, status.length);

    memcpy(resultMemory + headersPos, "\r\n", 2);

    headersPos += 2;

    p_obj->response_head_length = headersPos - offset;

    p_obj->response_body_length = p_obj->response_head_length;

    memmove(resultMemory, resultMemory + offset, p_obj->response_head_length);

    p_obj->response_body_length += contentLength;

    char* res_page = (char*)realloc(resultMemory, p_obj->response_body_length);

    if (res_page == nullptr) {
        free(resultMemory);
        free(memContenLength);
        p_obj->status = 500;
        return nullptr;
    }

    resultMemory = res_page;

    if(contentLength > 0) {
        memcpy(resultMemory + p_obj->response_head_length, content->c_str(), contentLength);
    }

    free(memContenLength);

    return resultMemory;
}

} // namespace