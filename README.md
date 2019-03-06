# HTTP-server

## Currently implemented

- return of static files, including video files
- caching of static files
- processing url (path, uri, query, extension)
- redirects
- listening on multiple ports
- multithreading
- support for https protocol
- standard error codes
- sending email in a separate thread (with DKIM signature)
- interaction in PostgreSQL


## Build & run
```
mkdir build tmp;
cd build;
cmake ..;
make -j2;
./server
```


## Generating DH parameters
```
cd required;
openssl dhparam -out dh2048.pem 2048;
openssl dhparam -out dh4096.pem 4096;
```
