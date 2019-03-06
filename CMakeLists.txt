cmake_minimum_required (VERSION 2.8)
project (HTTP-Server)

set(PostgreSQL_INCLUDE_DIR "/usr/local/pgsql/include")
set(PostgreSQL_LIBRARY "/usr/local/pgsql/lib")
set(PostgreSQL_TYPE_INCLUDE_DIR "/usr/local/pgsql")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${PostgreSQL_LIBRARY} -Wno-unused-result -Ofast -lresolv -lcrypto -lssl -pthread -lpq -lpcre -std=c++14 -Wall")




find_package(OpenSSL)
find_package(PostgreSQL REQUIRED)

add_library(dkim        STATIC src/dkim/dkim.cpp)
add_library(controller  STATIC src/controller/controller.cpp)
add_library(model       STATIC src/model/model.cpp)
add_library(view        STATIC src/view/view.cpp)
add_library(mail        STATIC src/mail/mail.cpp)
add_library(functions   STATIC src/functions/functions.cpp)
add_library(json        STATIC src/json/json.cpp)
add_library(polling     STATIC src/polling/polling.cpp)
add_library(hashids     STATIC src/hashids/hashids.cpp)
add_library(serv        STATIC src/server.cpp)


add_library(testController   STATIC src/pages/test/controller.cpp)




link_libraries(mail dkim)

add_executable(server
	src/server.cpp
	src/main.cpp
)

target_link_libraries(server 
	${OPENSSL_LIBRARIES}
	dkim
	controller
	model
	view
	json
	polling
	hashids
	mail
	functions
	serv
	pcre
	resolv
	pthread
	pq

	testController
)