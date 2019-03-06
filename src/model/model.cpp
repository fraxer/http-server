#include "model.h"

namespace server {

Model::Model() {
    this->pgconn = nullptr;
    this->conn_obj = nullptr;
}

Model::~Model() {

    // unordered_map<int, conn_st*>::iterator it;

    // if (this->tid > 0) {
    //     it = db_connection.find(this->tid);
    // } else {
    //     it = db_connection.find(gettid());
    // }

    // if (it == db_connection.end()) {
    //     printf("Error find of connection by tid: %ld\n", this->tid > 0 ? this->tid : gettid());
    // }

    // PQfinish(it->second->conn);
    // it->second->conn = nullptr;



    if (this->conn_obj != nullptr) {
        pthread_mutex_lock(&lock_list_conn);

        this->conn_obj->acquired = false;

        pthread_cond_signal(&cond_list_conn);

        pthread_mutex_unlock(&lock_list_conn);
    }
}

bool Model::isConnected() {
    return this->isConnected(0);
}

PGconn* Model::loginDb() {
    return PQconnectdb("host=127.0.0.1 port=5432 dbname=dtrack connect_timeout=3 user=slave password=(@Byf8f32jioJHg2y7)");
    // return PQsetdbLogin("127.0.0.1", "5432", nullptr, nullptr, "dtrack", "slave", "(@Byf8f32jioJHg2y7)");
}

bool Model::isConnected(long int _tid) {

    // printf("%d - %d\n", pthread_self(), gettid());

    this->tid = _tid;

    conn_st* db_conn = nullptr;

    unordered_map<int, conn_st*>::iterator it;

    if (this->tid > 0) {
        it = db_connection.find(this->tid);
    } else {
        it = db_connection.find(gettid());
    }

    if (it == db_connection.end()) {
        printf("Error find of connection by tid: %ld\n", this->tid > 0 ? this->tid : gettid());
        return false;
    }

    db_conn = it->second;

    auto secs = chrono::duration_cast<chrono::duration<int>>(chrono::system_clock::now() - db_conn->ttl);

    // printf("thread %d, mysql ttl %d\n", gettid(), secs.count());

    if(PQstatus(db_conn->conn) == CONNECTION_OK && secs.count() < DB_CONNECTION_TTL) {

        this->pgconn = db_conn->conn;

        // printf("already connected\n");

        return true;
    }

    if(db_conn->conn != nullptr) {
        PQfinish(db_conn->conn);
        db_conn->conn = nullptr;
    }

    if (!this->createConnection()) {
        return false;
    }

    // printf("create new connection\n");

    db_conn->conn = this->pgconn;
    db_conn->ttl  = chrono::system_clock::now();

    return true;
}

bool Model::isConnected2() {

    this->setConnection();

    if (this->conn_obj == nullptr) return false;

    // printf("%d - %d\n", pthread_self(), gettid());

    auto secs = chrono::duration_cast<chrono::duration<int>>(chrono::system_clock::now() - this->conn_obj->ttl);

    // printf("thread %d, db ttl %d\n", gettid(), secs.count());

    if(PQstatus(this->conn_obj->conn) == CONNECTION_OK && secs.count() < DB_CONNECTION_TTL) {

        this->pgconn = this->conn_obj->conn;

        // printf("already connected\n");

        return true;
    }

    if(this->conn_obj->conn != nullptr) {
        PQfinish(this->conn_obj->conn);
        this->conn_obj->conn = nullptr;
    }

    if (!this->createConnection()) {
        return false;
    }

    this->conn_obj->conn = this->pgconn;

    // printf("create new connection\n");

    this->conn_obj->ttl = chrono::system_clock::now();

    return true;
}

void Model::setConnection() {

    bool conn_finded = false;
    bool tried = false;

    pthread_mutex_lock(&lock_list_conn);

    while (1) {

        for (int i = 0; i < 1; ++i) {

            if(list_connections[i]->acquired) {
                continue;
            }

            list_connections[i]->acquired = true;

            this->conn_obj = list_connections[i];
            this->pgconn = this->conn_obj->conn;
            conn_finded = true;
            break;
        }

        if (conn_finded) break;

        if (tried) break;

        // printf("waited\n");

        struct timeval now;
        struct timespec timeout;

        gettimeofday(&now, 0);
        timeout.tv_nsec = now.tv_sec + 1;

        pthread_cond_timedwait(&cond_list_conn, &lock_list_conn, &timeout);

        // printf("after waited\n");

        tried = true;
    }

    pthread_mutex_unlock(&lock_list_conn);
}

bool Model::createConnection() {
    this->pgconn = this->loginDb();

    if(PQstatus(this->pgconn) != CONNECTION_OK) {
        printf("Error create of connection: %s\n", PQerrorMessage(this->pgconn));
        return false;
    }

    return true;
}

void Model::freeConnection() {

    // if (this->pgconn != nullptr) {
    //     PQfinish(this->pgconn);
    // }

    return;
}

vector<vector<string*>*>* Model::queryNew(string s) {
    this->successQuery = true;

    bool insert_tuples = false;

    if(!PQsendQuery(this->pgconn, s.c_str())){
        printf("Send query error: %s\nError: %s", s.c_str(), PQerrorMessage(this->pgconn));
        return nullptr;
    }

    vector<vector<string*>*>* array = new vector<vector<string*>*>();

    int row, col, num_cols, num_rows;

    while(PGresult* res = PQgetResult(this->pgconn)) {

        ExecStatusType status = PQresultStatus(res);

        if(status == PGRES_TUPLES_OK || status == PGRES_SINGLE_TUPLE) {

            insert_tuples = true;

            num_cols = PQnfields(res);
            num_rows = PQntuples(res);

            // printf("%d, %d\n", num_cols, num_rows);

            for (col = 0; col < num_cols; col++) {

                vector<string*>* v = new vector<string*>();

                // printf("%s\n", PQfname(res, col));

                for (row = 0; row < num_rows; row++) {
                    // printf("%s\n", PQgetvalue(res, row, col));
                    v->push_back(new string(PQgetvalue(res, row, col)));
                }

                array->push_back(v);
            }
        }
        else if(status == PGRES_FATAL_ERROR){
            printf("Fatal error: %s\nError: %s", s.c_str(), PQresultErrorMessage(res));
            this->successQuery = false;
        }
        else if(status == PGRES_NONFATAL_ERROR){
            printf("Nonfatal error: %s\nError: %s", s.c_str(), PQresultErrorMessage(res));
            this->successQuery = false;
        }
        else if(status == PGRES_EMPTY_QUERY){
            printf("Empty query: %s", PQresultErrorMessage(res));
            this->successQuery = false;
        }
        else if(status == PGRES_BAD_RESPONSE){
            printf("Bad response: %s", PQresultErrorMessage(res));
            this->successQuery = false;
        }
        else if(status == PGRES_COMMAND_OK){
            // this->successQuery = true;
        }

        PQclear(res);
    }

    if(!this->successQuery) {
        this->freeRes(array);
        return nullptr;
    }

    if (!insert_tuples) {
        delete array;
        return nullptr;
    }

    return array;
}

void Model::freeRes(vector<vector<string*>*>*& array) {

    if (array == nullptr) return;

    for (auto& column : *array) {
        for (auto& row : *column) {
            delete row;
        }
        column->clear();
        delete column;
    }
    array->clear();
    delete array;

    return;
}

vector<vector<string>> Model::query(string s) {

    this->successQuery = true;

    vector<vector<string>> array;

    if(!PQsendQuery(this->pgconn, s.c_str())){
        printf("Send query error: %s\nError: %s", s.c_str(), PQerrorMessage(this->pgconn));
        return array;
    }

    int row, col, num_cols, num_rows;

    while(PGresult* res = PQgetResult(this->pgconn)) {

        ExecStatusType status = PQresultStatus(res);

        if(status == PGRES_TUPLES_OK || status == PGRES_SINGLE_TUPLE) {

            num_cols = PQnfields(res);
            num_rows = PQntuples(res);

            // printf("%d, %d\n", num_cols, num_rows);

            for (col = 0; col < num_cols; col++) {

                vector<string> v;

                // printf("%s\n", PQfname(res, col));

                for (row = 0; row < num_rows; row++) {
                    // printf("%s\n", PQgetvalue(res, row, col));
                    v.push_back(PQgetvalue(res, row, col));
                }

                array.push_back(v);
            }
        }
        else if(status == PGRES_FATAL_ERROR){
            printf("Fatal error: %s\nError: %s", s.c_str(), PQresultErrorMessage(res));
            this->successQuery = false;
        }
        else if(status == PGRES_NONFATAL_ERROR){
            printf("Nonfatal error: %s\nError: %s", s.c_str(), PQresultErrorMessage(res));
            this->successQuery = false;
        }
        else if(status == PGRES_EMPTY_QUERY){
            printf("Empty query: %s", PQresultErrorMessage(res));
            this->successQuery = false;
        }
        else if(status == PGRES_BAD_RESPONSE){
            printf("Bad response: %s", PQresultErrorMessage(res));
            this->successQuery = false;
        }
        else if(status == PGRES_COMMAND_OK){
            // this->successQuery = true;
        }

        PQclear(res);
    }

    if(!this->successQuery) {
        array.clear();
    }

    return array;
}

char* Model::trim(char*& str) {

    size_t len = strlen(str);

    if(len == 0) return str;

    size_t start = 0;

    while(start < len && isspace(str[start])) {
        start++;
    }
    
    size_t end = len - 1;

    while(end > 0 && isspace(str[end])) {
        end--;
    }
    
    end++;
    
    memmove(str, str + start, (len - start) - (len - end));

    str[(len - start) - (len - end)] = 0;

    return str;
}

string& Model::trim(string& str) {

    size_t len = str.size();

    if(len == 0) return str;

    size_t start = 0;

    while(start < len && isspace(str[start])) {
        start++;
    }

    size_t end = len - 1;

    while(end > 0 && isspace(str[end])) {
        end--;
    }
    
    end++;

    str = str.substr(start, (len - start) - (len - end));
    
    return str;
};

char* Model::shielding(char*& str) {
    
    size_t len = strlen(str) + 1;
    size_t p   = 0;
    size_t pos = 0;
    
    char* new_str = (char*)malloc(len);

    while(str[p] != 0) {

        if(str[p] == '\'') {
            len += 2;
            new_str = (char*)realloc(new_str, len);
            memcpy(new_str + pos, "''", 2);
            pos += 2;
        }
        else if(str[p] == '\"' && str[p - 1] != '\\') {
            len += 2;
            new_str = (char*)realloc(new_str, len);
            memcpy(new_str + pos, "\\\"", 2);
            pos += 2;
        }
        else if(str[p] == '\n') {
            len += 2;
            new_str = (char*)realloc(new_str, len);
            memcpy(new_str + pos, "\\n", 2);
            pos += 2;
        }
        else if(str[p] == '\r') {
            len += 2;
            new_str = (char*)realloc(new_str, len);
            memcpy(new_str + pos, "\\r", 2);
            pos += 2;
        }
        else if(str[p] == '\a') {
            len += 2;
            new_str = (char*)realloc(new_str, len);
            memcpy(new_str + pos, "\\a", 2);
            pos += 2;
        }
        else if(str[p] == '\b') {
            len += 2;
            new_str = (char*)realloc(new_str, len);
            memcpy(new_str + pos, "\\b", 2);
            pos += 2;
        }
        else if(str[p] == '\t') {
            len += 2;
            new_str = (char*)realloc(new_str, len);
            memcpy(new_str + pos, "\\t", 2);
            pos += 2;
        }
        else if(str[p] == '\v') {
            len += 2;
            new_str = (char*)realloc(new_str, len);
            memcpy(new_str + pos, "\\v", 2);
            pos += 2;
        }
        else if(str[p] == '\f') {
            len += 2;
            new_str = (char*)realloc(new_str, len);
            memcpy(new_str + pos, "\\f", 2);
            pos += 2;
        }
        else if(str[p] == '\e') {
            len += 2;
            new_str = (char*)realloc(new_str, len);
            memcpy(new_str + pos, "\\e", 2);
            pos += 2;
        }
        else if(str[p] == 127 || (str[p] > 0 && str[p] < 32)) {}
        else {
            new_str[pos] = str[p];
            pos += 1;
        }

        p++;
    }

    new_str[pos] = 0;

    free(str);

    return new_str;
}

string& Model::shielding(string& str) {
    int g = 0;

    while(str[g] != '\0')
    {
        if(str[g] == '\'') {
            str.replace(g, 1, "''");
            g += 2;
        }
        else if(str[g] == '\"' && str[g - 1] != '\\') {
            str.replace(g, 1, "\\\"");
            g += 2;
        }
        else if(str[g] == '\n') {
            str.replace(g, 1, "\\n");
            g += 2;
        }
        else if(str[g] == '\r') {
            str.replace(g, 1, "\\r");
            g += 2;
        }
        else if(str[g] == '\a') {
            str.replace(g, 1, "\\a");
            g += 2;
        }
        else if(str[g] == '\b') {
            str.replace(g, 1, "\\b");
            g += 2;
        }
        else if(str[g] == '\t') {
            str.replace(g, 1, "\\t");
            g += 2;
        }
        else if(str[g] == '\v') {
            str.replace(g, 1, "\\v");
            g += 2;
        }
        else if(str[g] == '\f') {
            str.replace(g, 1, "\\f");
            g += 2;
        }
        else if(str[g] == '\e') {
            str.replace(g, 1, "\\e");
            g += 2;
        }
        else if(str[g] == 127 || (str[g] > 0 && str[g] < 32)) {
            str.replace(g, 1, "");
        }
        else {
            g++;
        }
    }

    return str;
}

bool Model::isInteger(const char* str) {
    size_t len = strlen(str);

    if (*str == '-') {
        str++;
        len--;
    }

    if (len == 0) return false;

    if (strspn(str, "0123456789") == len) {
        return true;
    }
    return false;
}

bool Model::isPositiveInteger(const char* str) {
    size_t len = strlen(str);

    if (len == 0) return false;

    if (strspn(str, "0123456789") == len) {
        return true;
    }
    return false;
}

} // namespace