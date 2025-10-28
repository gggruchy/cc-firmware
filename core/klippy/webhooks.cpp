#include "webhooks.h"
#include "klippy.h"
#include "my_string.h"
#include "debug.h"
#define LOG_TAG "webhooks"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

WebRequest::WebRequest(ClientConnection *client_conn, std::string request)
{
	m_client_conn = client_conn;
	std::string base_request = json.loads(request, object_hook=byteify);
	if (type(base_request) != dict){
		raise ValueError("Not a top-level dictionary")
	}
	m_id = base_request.get('id', None);
	m_method = base_request.get('method');
	m_params = base_request.get('params', {});
	if (type(self.method) != str or type(self.params) != dict){
		raise ValueError("Invalid request type")
	}
	m_response = "";
	m_is_error = false;
}

WebRequest::~WebRequest()
{

}
        

ClientConnection * WebRequest::get_client_connection()
{
	return m_client_conn;
}

std::string WebRequest::get(std::string item, std::string _default)
{
	return m_params[item];
}

std::string WebRequest::get_str(std::string item, std::string _default)
{
	std::string value = get(item);
	if (value == "")
	{
		value = _default;
	}
	return value;

}
    
int WebRequest::get_int(std::string item, int _default)
{
	std::string value = get(item);
	if (value == "")
	{
		return _default;
	}
	return stoi(value);
	
}

double WebRequest::get_double(std::string item, double _default)
{
	std::string value = get(item);
	if (value == "")
	{
		return _default;
	}
	return stof(value);
}

std::map<std::string, std::string> WebRequest::get_dict(std::string item, std::map<std::string, std::string> _default)
{
	return self.get(item, default, types=(dict,));
}
        

std::string WebRequest::get_method()
{
	return m_method;
}

void WebRequest::set_error(std::string error)
{
	m_is_error = true;
	m_response = error;
}

void WebRequest::send(std::string data)
{
	if (m_response != "")
	{
		raise WebRequestError("Multiple calls to send not allowed");
	}
	m_response = data;
}
        
std::vector<std::string> WebRequest::finish()
{
	std::vector<std::string> ret;
	if (m_id == -1)
	{
		ret.push_back(std::to_string(-1));
		return ret;
	}
	std::string rtype = "result";
	if (m_is_error)
	{
		rtype = "error";
	}
	if (m_response == "")
	{
		No error was set and the user never executed
		send, default response is {}
		m_response = "";
	}
	ret.push_back(std::to_string(m_id));
	ret.push_back(m_response);
	return ret;
}


#define PORT 0x8888
ServerSocket::ServerSocket(WebHooks *webhooks)
{
	m_webhooks = webhooks;
	m_sock_fd = -1;
	std::string server_address = Printer::GetInstance()->get_start_args("apiserver");
	std::string is_fileinput = Printer::GetInstance()->get_start_args("debuginput");
	if (server_address == "" || is_fileinput != "")
	{
		//Do not enable server
		return;
	}
	_remove_socket_file(server_address);

	int newSockFd, valread;
    int opt = 1;
    struct sockaddr_in address;
	m_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == m_sock_fd)
	{
		printf("socket fail ! \r\n");
	}
	int flags = fcntl(m_sock_fd, F_GETFL, NULL);
	fcntl(m_sock_fd, F_SETFL, flags | O_NONBLOCK);
	setsockopt(m_sock_fd, SOL_SOCKET, SO_REUSEADDR|SO_REUSEPORT, &opt, sizeof(opt));
	printf("socket seccess ! \r\n");
	bzero(&address,sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);//INADDR_ANY;
    address.sin_port = htons(PORT);
    int addrlen = sizeof(address);
	
    if (-1 == bind(m_sock_fd,(struct sockaddr *)(&address), sizeof(struct sockaddr)))
	{
		printf("bind fail !\r\n");
	}
	printf("bind ok !\r\n");
    if(-1 == listen(m_sock_fd,5))
	{
		printf("listen fail !\r\n");
	}
	printf("listen ok\r\n");
	m_fd_handle = Printer::GetInstance()->m_reactor->register_fd(m_sock_fd, std::bind(&ServerSocket::_handle_accept, this, std::placeholders::_1));
	Printer::GetInstance()->register_event_handler("klippy:disconnect:ServerSocket", std::bind(&ServerSocket::_handle_disconnect, this));
}

ServerSocket::~ServerSocket()
{

}

void ServerSocket::_handle_accept(double eventtime)
{
	struct sockaddr_in address;
	int addrlen = sizeof(address);
	int sockfd = accept(m_sock_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
	if(-1 == sockfd)
	{
		printf("accept fail !\r\n");
		return;
	}
	std::cout << "connect seccess! " << std::endl;
	int flags = fcntl(sockfd, F_GETFL, NULL);
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	char buffer[1024]={0}; 
	int recbytes = 0;
	if(-1 == (recbytes = read(sockfd,buffer,1024)))
	{
		printf("read data fail !\r\n");
		return;
	}
	printf("read ok\r\nREC:\r\n");
	buffer[recbytes]='\0';
	printf("%s\r\n",buffer);	

	if(-1 == write(sockfd,"hello, im server \r\n",32))
	{
		printf("write fail!\r\n");
		return ;
	}
	printf("write ok!\r\n");

	ClientConnection *client = new ClientConnection(this, sockfd);
	m_clients[client->m_uid] = client;
}
        

void ServerSocket::_handle_disconnect()
{
	std::map<int, ClientConnection*>::iterator it;
	while (it != m_clients.end())
	{
		it->second->_close();
	}
	if (m_sock_fd != -1)
	{
		Printer::GetInstance()->m_reactor->unregister_fd(m_fd_handle);
		close(m_sock_fd);
	}
}
        

void ServerSocket::_remove_socket_file(std::string file_path)
{
	try:
		os.remove(file_path)
	except OSError:
		if os.path.exists(file_path):
			LOG_E(
				"webhooks: Unable to delete socket file '%s'"
				% (file_path))
			raise
}
        
void ServerSocket::pop_client(int client_id)
{
	m_clients.erase(client_id);
}

ClientConnection::ClientConnection(ServerSocket* server, int sock)
{
	m_webhooks = server->m_webhooks;
	m_server = server;
	m_uid = m_webhooks->creat_client_id();
	m_sock = sock;
	m_fd_handle = Printer::GetInstance()->m_reactor->register_fd(m_sock,  std::bind(&ClientConnection::process_received, this, std::placeholders::_1));
	m_partial_data = "";
	m_send_buffer = "";
	m_is_sending_data = false;
	set_client_info("?", "New connection");
} 

ClientConnection::~ClientConnection()
{

}

void ClientConnection::set_client_info(std::string client_info, std::string state_msg)
{
	if (state_msg is None)
		state_msg = "Client info %s" % (repr(client_info),)
	LOG_I("webhooks client %s: %s", self.uid, state_msg)
	log_id = "webhooks %s" % (self.uid,)
	if client_info is None:
		self.printer.set_rollover_info(log_id, None, log=False)
		return
	rollover_msg = "webhooks client %s: %s" % (self.uid, repr(client_info))
	self.printer.set_rollover_info(log_id, rollover_msg, log=False)
}
	

void ClientConnection::_close()
{
	if (m_fd_handle == nullptr)
		return;
	set_client_info("", "Disconnected");
	Printer::GetInstance()->m_reactor->unregister_fd(m_fd_handle);
	m_fd_handle = nullptr;
	close(m_sock);
	m_server->pop_client(m_uid);
}
	

bool ClientConnection::is_closed()
{
	return m_fd_handle == nullptr;
}
	

void ClientConnection::process_received(double eventtime)
{
	char data[4096];
	read(m_sock, data, 4096);
	if (data == "")
	{
		// Socket Closed
		_close();
		return;
	}
	std::vector<std::string> requests = split(data, "\n");
	requests[0] = m_partial_data + requests[0];
	m_partial_data = requests.back();
	requests.pop_back();
	for (auto req : requests)
	{
		WebRequest *web_request = new WebRequest(this, req);
		Printer::GetInstance()->m_reactor->register_web_request_callback(std::bind(&ClientConnection::_process_request, this, std::placeholders::_1), web_request);
	}
		
}
	

void ClientConnection::_process_request(WebRequest * web_request)
{
	try:
		func = self.webhooks.get_callback(web_request.get_method())
		func(web_request)
	except self.printer.command_error as e:
		web_request.set_error(WebRequestError(str(e)))
	except Exception as e:
		msg = ("Internal Error on WebRequest: %s"
				% (web_request.get_method()))
		LOG_E(msg)
		web_request.set_error(WebRequestError(str(e)))
		self.printer.invoke_shutdown(msg)
	result = web_request.finish()
	if result is None:
		return
	self.send(result)
}
	

void ClientConnection::_send(std::string data)
{
	self.send_buffer += json.dumps(data) + "\x03"
	if not self.is_sending_data:
		self.is_sending_data = True
		self.reactor.register_callback(self._do_send)
}
	

void ClientConnection::_do_send(double eventtime)
{
	int retries = 10;
	while (m_send_buffer != "")
	{
		int ret_send = send(m_sock, m_send_buffer.c_str(), m_send_buffer.size(), 0); 
		if (ret_send < 0)
		{
			if (ret_send == -EBADF || ret_send == -EPIPE || retries == 0)
				ret_send = 0;
			else
			{
				retries -= 1;
				double waketime = get_monotonic() + .001;
				Printer::GetInstance()->m_reactor->pause(waketime);
				continue;
			}
		}
		retries = 10;
		if (ret_send > 0)
			m_send_buffer = m_send_buffer.substr(ret_send);
		else
		{
			LOG_I("webhooks: Error sending server data,  closing socket")
			_close();
			break;
		}
	}
	m_is_sending_data = false;
}
	

WebHooks::WebHooks()
{
	std::map<std::string, std::function<void(WebRequest*)>> m_endpoints;
	m_endpoints["list_endpoints"] = std::bind(&WebHooks::_handle_list_endpoints, this, std::placeholders::_1);
	self._remote_methods = {}
	register_endpoint("info", std::bind(&WebHooks::_handle_info_request, this, std::placeholders::_1));
	register_endpoint("emergency_stop", std::bind(&WebHooks::_handle_estop_request, this, std::placeholders::_1));
	register_endpoint("register_remote_method", std::bind(&WebHooks::_handle_rpc_registration, this, std::placeholders::_1));
	m_sconn = new ServerSocket(this);
	m_client_id = 0;
}

WebHooks::~WebHooks()
{

}

int WebHooks::creat_client_id()
{
	int client_id = m_client_id;
	m_client_id++;
	return client_id;
}

void WebHooks::register_endpoint(std::string path, std::function<void(WebRequest*)>callback)
{
	if (m_endpoints.find(path) != m_endpoints.end())
	{
		raise WebRequestError("Path already registered to an endpoint")
	}
	m_endpoints[path] = callback;
}

void WebHooks::_handle_list_endpoints(WebRequest* web_request)
{
	web_request->_send({'endpoints': list(self._endpoints.keys())});  //---??---
}       

void WebHooks::_handle_info_request(WebRequest* web_request)
{
	client_info = web_request.get_dict('client_info', None)
	if client_info is not None:
		web_request.get_client_connection().set_client_info(client_info)
	state_message, state = self.printer.get_state_message()
	src_path = os.path.dirname(__file__)
	klipper_path = os.path.normpath(os.path.join(src_path, ".."))
	response = {'state': state, 'state_message': state_message,
				'hostname': socket.gethostname(),
				'klipper_path': klipper_path, 'python_path': sys.executable}
	start_args = self.printer.get_start_args()
	for sa in ['log_file', 'config_file', 'software_version', 'cpu_info']:
		response[sa] = start_args.get(sa)
	web_request.send(response)
}
        

void WebHooks::_handle_estop_request(WebRequest* web_request)
{
	Printer::GetInstance()->invoke_shutdown("Shutdown due to webhooks request");
}

void WebHooks::_handle_rpc_registration(WebRequest* web_request)
{
	template = web_request.get_dict('response_template')
	method = web_request.get_str('remote_method')
	new_conn = web_request.get_client_connection();
	LOG_I("webhooks: registering remote method '%s' for connection id: %d" % (method, id(new_conn)))
	self._remote_methods.setdefault(method, {})[new_conn] = template
}
        
ServerSocket * WebHooks::get_connection()
{
	return m_sconn;
}
        
std::function<void(WebRequest*)> WebHooks::get_callback(std::string path)
{
	std::function<void(WebRequest*)> cb = m_endpoints[path];
	if (cb == nullptr)
	{
		msg = "webhooks: No registered callback for path '%s'" % (path)
		LOG_I(msg)
		raise WebRequestError(msg)
	}
	return cb;
}
        
void WebHooks::get_status(double eventtime)
{
	state_message, state = self.printer.get_state_message()
    return {'state': state, 'state_message': state_message}
}
        

void WebHooks::call_remote_method(std::string method, void **kwargs)
{
	if method not in self._remote_methods:
		raise self.printer.command_error(
			"Remote method '%s' not registered" % (method))
	conn_map = self._remote_methods[method]
	valid_conns = {}
	for conn, template in conn_map.items():
		if not conn.is_closed():
			valid_conns[conn] = template
			out = {'params': kwargs}
			out.update(template)
			conn.send(out)
	if not valid_conns:
		del self._remote_methods[method]
		raise self.printer.command_error(
			"No active connections for method '%s'" % (method))
	self._remote_methods[method] = valid_conns
}
        

GCodeHelper::GCodeHelper()
{
	// Output subscription tracking
	m_is_output_registered = false;
	// Register webhooks
	WebHooks *wh = Printer::GetInstance()->m_webhooks;
	wh->register_endpoint("gcode/help", std::bind(&GCodeHelper::_handle_help, this, std::placeholders::_1));
	wh->register_endpoint("gcode/script", std::bind(&GCodeHelper::_handle_script, this, std::placeholders::_1));
	wh->register_endpoint("gcode/restart", std::bind(&GCodeHelper::_handle_restart, this, std::placeholders::_1));
	wh->register_endpoint("gcode/firmware_restart", std::bind(&GCodeHelper::_handle_firmware_restart, this, std::placeholders::_1));
	wh->register_endpoint("gcode/subscribe_output", std::bind(&GCodeHelper::_handle_subscribe_output, this, std::placeholders::_1));
}

GCodeHelper::~GCodeHelper()
{

}
        
void GCodeHelper::_handle_help(WebRequest* web_request)
{
	web_request->_send(Printer::GetInstance()->m_gcode->get_command_help());
}
        
void GCodeHelper::_handle_script(WebRequest* web_request)
{
	Printer::GetInstance()->m_gcode->run_script(web_request->get_str("script"));
}
        
void GCodeHelper::_handle_restart(WebRequest* web_request)
{
	Printer::GetInstance()->m_gcode->run_script("restart");
}
        
void GCodeHelper::_handle_firmware_restart(WebRequest* web_request)
{
	Printer::GetInstance()->m_gcode->run_script("firmware_restart");
}
        
void GCodeHelper::_output_callback(std::string msg)
{
	for cconn, template in list(self.clients.items()):
		if cconn.is_closed():
			del self.clients[cconn]
			continue
		tmp = dict(template)
		tmp['params'] = {'response': msg}
		cconn.send(tmp)
}
        
void GCodeHelper::_handle_subscribe_output(WebRequest* web_request)
{
	ClientConnection * cconn = web_request->get_client_connection();
	template = web_request.get_dict('response_template', {})
	m_clients[cconn] = template
	if (!m_is_output_registered)
	{
		Printer::GetInstance()->m_gcode->register_output_handler(std::bind(&GCodeHelper::_output_callback, this, std::placeholders::_1));
		m_is_output_registered = true;
	}
}
        

#define SUBSCRIPTION_REFRESH_TIME = .25

QueryStatusHelper::QueryStatusHelper()
{
	m_is_subscribe = false;
	// Register webhooks
	WebHooks *webhooks = Printer::GetInstance()->m_webhooks;
	webhooks->register_endpoint("objects/list", std::bind(&QueryStatusHelper::_handle_list, this, std::placeholders::_1));
	webhooks->register_endpoint("objects/query", std::bind(&QueryStatusHelper::_handle_query, this, std::placeholders::_1));
	webhooks->register_endpoint("objects/subscribe", std::bind(&QueryStatusHelper::_handle_subscribe, this, std::placeholders::_1));
}

QueryStatusHelper::~QueryStatusHelper()
{

}

void QueryStatusHelper::_handle_list(WebRequest* web_request)
{
	objects = [n for n, o in self.printer.lookup_objects()
				if hasattr(o, 'get_status')]
	web_request.send({'objects': objects})
}
        
double QueryStatusHelper::_do_query(double eventtime)
{
	last_query = self.last_query
	query = self.last_query = {}
	msglist = self.pending_queries
	self.pending_queries = []
	msglist.extend(self.clients.values())
	# Generate get_status() info for each client
	for cconn, subscription, send_func, template in msglist:
		is_query = cconn is None
		if not is_query and cconn.is_closed():
			del self.clients[cconn]
			continue
		# Query each requested printer object
		cquery = {}
		for obj_name, req_items in subscription.items():
			res = query.get(obj_name, None)
			if res is None:
				po = self.printer.lookup_object(obj_name, None)
				if po is None or not hasattr(po, 'get_status'):
					res = query[obj_name] = {}
				else:
					res = query[obj_name] = po.get_status(eventtime)
			if req_items is None:
				req_items = list(res.keys())
				if req_items:
					subscription[obj_name] = req_items
			lres = last_query.get(obj_name, {})
			cres = {}
			for ri in req_items:
				rd = res.get(ri, None)
				if is_query or rd != lres.get(ri):
					cres[ri] = rd
			if cres or is_query:
				cquery[obj_name] = cres
		# Send data
		if cquery or is_query:
			tmp = dict(template)
			tmp['params'] = {'eventtime': eventtime, 'status': cquery}
			send_func(tmp)
	if not query:
		# Unregister timer if there are no longer any subscriptions
		reactor = self.printer.get_reactor()
		reactor.unregister_timer(self.query_timer)
		self.query_timer = None
		return reactor.NEVER
	return eventtime + SUBSCRIPTION_REFRESH_TIME
}
        
void QueryStatusHelper::_handle_query(WebRequest* web_request) //is_subscribe=False
{
	objects = web_request.get_dict('objects')
	# Validate subscription format
	for k, v in objects.items():
		if type(k) != str or (v is not None and type(v) != list):
			raise web_request.error("Invalid argument")
		if v is not None:
			for ri in v:
				if type(ri) != str:
					raise web_request.error("Invalid argument")
	# Add to pending queries
	cconn = web_request.get_client_connection()
	template = web_request.get_dict('response_template', {})
	if is_subscribe and cconn in self.clients:
		del self.clients[cconn]
	reactor = self.printer.get_reactor()
	complete = reactor.completion()
	self.pending_queries.append((None, objects, complete.complete, {}))
	# Start timer if needed
	if self.query_timer is None:
		qt = reactor.register_timer(self._do_query, reactor.NOW)
		self.query_timer = qt
	# Wait for data to be queried
	msg = complete.wait()
	web_request.send(msg['params'])
	if is_subscribe:
		self.clients[cconn] = (cconn, objects, cconn.send, template)
}
        
void QueryStatusHelper::_handle_subscribe(WebRequest* web_request)
{
	m_is_subscribe = true;
	_handle_query(web_request);
}
        
