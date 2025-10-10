#ifndef WEBHOOKS_H
#define WEBHOOKS_H

#include<iostream>
#include<assert.h>
#include<string.h>

#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>

#include<unistd.h>
#include <fcntl.h>
#include "gcode.h"
#include "reactor.h"

class WebRequest;
class ServerSocket;
class ClientConnection;
class WebHooks;
class GCodeHelper;
class QueryStatusHelper;


class WebRequest{
	private:

	public:
		WebRequest(ClientConnection *client_conn, std::string request);
		~WebRequest();

		ClientConnection * m_client_conn;
		int m_id;
		std::string m_method;
		std::map<std::string, std::string> m_params;
		std::string m_response;
		bool m_is_error;

		ClientConnection * get_client_connection();
		std::string get(std::string item, std::string _default="");
		std::string get_str(std::string item, std::string _default = "");
		int get_int(std::string item, int _default=INT32_MIN);
		double get_double(std::string item, double _default=DBL_MIN);
		std::map<std::string, std::string> get_dict(std::string item, std::map<std::string, std::string> _default=std::map<std::string, std::string>());
		std::string get_method();
		void set_error(std::string error);
		void send(std::string data);
		std::vector<std::string> finish();
};

class ServerSocket{
	private:

	public:
		ServerSocket(WebHooks *webhooks);
		~ServerSocket();

		WebHooks *m_webhooks;
		int m_sock_fd;
		ReactorFileHandler * m_fd_handle;
		std::map<int, ClientConnection*> m_clients;

		void _handle_accept(double eventtime);
		void _handle_disconnect();
		void _remove_socket_file(std::string file_path);    
		void pop_client(int client_id);
};

class ClientConnection{
	private:

	public:
		ClientConnection(ServerSocket* server, int sock);
		~ClientConnection();

		WebHooks* m_webhooks;
		ServerSocket* m_server;
		int m_uid;
		int m_sock;
		ReactorFileHandler * m_fd_handle;
		std::string m_partial_data;
		std::string m_send_buffer;
		bool m_is_sending_data;

		void set_client_info(std::string client_info, std::string state_msg="");
		void _close();
		bool is_closed();
		void process_received(double eventtime);
		void _process_request(WebRequest * web_request);
		void _send(std::string data);
		void _do_send(double eventtime);

};

class WebHooks{
    private:

    public:
        WebHooks();
        ~WebHooks();

		std::map<std::string, std::function<void(WebRequest*)>> m_endpoints;
		int m_client_id;
		ServerSocket *m_sconn;

		int creat_client_id();
		void register_endpoint(std::string path, std::function<void(WebRequest*)>callback);
		void _handle_list_endpoints(WebRequest* web_request);
		void _handle_info_request(WebRequest* web_request);
		void _handle_estop_request(WebRequest* web_request);
		void _handle_rpc_registration(WebRequest* web_request);
		ServerSocket * get_connection();
		std::function<void(WebRequest*)> get_callback(std::string path);
		void get_status(double eventtime);
		void call_remote_method(std::string method, void **kwargs);


};

class GCodeHelper{
	private:

	public:
		GCodeHelper();
		~GCodeHelper();

		bool m_is_output_registered;
		std::vector<ClientConnection*> m_clients;

		void _handle_help(WebRequest* web_request); 
		void _handle_script(WebRequest* web_request);   
		void _handle_restart(WebRequest* web_request);      
		void _handle_firmware_restart(WebRequest* web_request);
		void _output_callback(std::string msg);  
		void _handle_subscribe_output(WebRequest* web_request);
};

class QueryStatusHelper{
	private:

	public:
		QueryStatusHelper();
		~QueryStatusHelper();

		std::vector<ClientConnection*> m_clients;
		std::vector<std::string> m_pending_queries;
		ReactorTimerPtr m_query_timer;
		std::vector<std::string> m_last_query;
		bool m_is_subscribe;

		void _handle_list(WebRequest* web_request);
		double _do_query(double eventtime);
		void _handle_query(WebRequest* web_request);
		void _handle_subscribe(WebRequest* web_request);
};

#endif