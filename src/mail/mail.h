#ifndef __MAIL__
#define __MAIL__

#include "../headers.h"
#include "../structs.h"
#include "../dkim/dkim.h"

using namespace std;

namespace server {

class Mail {
    private:
        bool  connected;

        int   socket;

        char* from;
        char* to;
        char* subject;
        char* date;
        char* message_id;
        char* ddata;
        char* hostname;

        char* private_key;
        char* domain;
        char* selector;

        stringpair** headers;

        int headers_size;

        size_t len_ddata;

        size_t len_from;
        size_t len_to;
        size_t len_subject;
        size_t len_date;
        size_t len_message_id;

        bool makeSocketNonblocking(int&);

        void parseMxRecord(unsigned char*, size_t, ns_sect, int, ns_msg*, map<int, mx_domain*>&);

		int  getMXServers(const char*, map<int, mx_domain*>&);

		void getMessage();

		void sendCommand(char*);

		void sendFrom();

		void sendTo();

		void sendData();

	public:

		Mail();

		~Mail();

        bool isConnected();

		int createConnection(char*, u_short);

		void setFrom(const char*);

		void setTo(const char*);

		void setSubject(const char*);

		void setData(const char*);

		void send();

		void closeConnection();

        static bool isRealService(const char*);
};

} // namespace

#endif