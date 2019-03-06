#include "headers.h"
#include "structs.h"
#include "functions/functions.h"

using namespace std;
using namespace server;

namespace server {
    extern SSL_CTX* ctx;
}

int main(int argc , char *argv[]) {

    if(daemon(1, 1) < 0) return 0;

    // int opt;

    // while ((opt = getopt(argc, argv, "ds:")) != -1) {
    //     switch (opt) {
    //        case 'd':
    //            break;
    //        case 's':
    //            break;
    //        default:
    //            printf("Usage: %s [-d server dir] [-s site dir]\n");
    //            exit(EXIT_FAILURE);
    //     }
    // }

    // if (optind >= argc) {
    //     fprintf(stderr, "Expected argument after options\n");
    //     exit(EXIT_FAILURE);
    // }

    // printf("%s\n", get_current_dir_name());

    signal(SIGPIPE, sigpipeHandler);

    setRedirect("^/$",                            "^/$",                                "/index.html",                    true,  0);

    setRedirect("./$",                            "(.+?)/+$",                           "$1",                           true,  301);

    for(size_t k = 0; k < rewrite.size(); k++) {

        if(rewrite[k].location_error != nullptr) {
            printf("error location %s\n", rewrite[k].location_error);
            printf("error location pos %d\n", rewrite[k].location_erroffset);
            exit(0);
        }

        if(rewrite[k].rule_error != nullptr) {
            printf("error rule %s\n", rewrite[k].rule_error);
            printf("error rule pos %d\n", rewrite[k].rule_erroffset);
            exit(0);
        }
    }

    for (int i = 0; i < PQ_MAX_CONNECTIONS; ++i) {
        list_connections[i] = new conn_st();
        list_connections[i]->acquired = false;
    }

    rewrite_size = rewrite.size();

    init_OpenSSL();

    setup_server_ctx();

    loadMimeTypes();

    unordered_map<int, obj*>* objs = new unordered_map<int, obj*>();

    pthread_t threads[WORKERS + 2];

    for(int i = 0; i < WORKERS; i++) {

        int err_thread = pthread_create(&threads[i], NULL, thread_func, (void*)objs);

        if (err_thread != 0) {
            printf("невозможно создать поток %d, %d", i, err_thread);
            return 0;
        }
        else {
            printf("создан поток %d\n", i);
        }
    }

    int err_thread = pthread_create(&threads[WORKERS], NULL, thread_mail, NULL);

    if (err_thread != 0) {
        printf("невозможно создать поток %d, %d", WORKERS, err_thread);
        return 0;
    }
    else {
        printf("создан \"почтовый\" поток %d\n", WORKERS);
    }

    err_thread = pthread_create(&threads[WORKERS + 1], NULL, thread_polling, NULL);

    if (err_thread != 0) {
        printf("невозможно создать поток %d, %d", WORKERS + 1, err_thread);
        return 0;
    }
    else {
        printf("создан поток \"опросы\" %d\n", WORKERS);
    }

    for(int i = 0; i < WORKERS + 2; i++)
        pthread_join(threads[i], NULL);

    SSL_CTX_free(ctx);

    ERR_free_strings();
}