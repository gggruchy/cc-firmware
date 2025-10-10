#ifndef REACTOR_H
#define REACTOR_H
#include <functional>
#include <vector>
#include <string>
#include <memory>

#define _NOW 0.
#define _NEVER 9999999999999999.

class SelectReactor;
class ReactorTimer;  // 前向声明

using ReactorTimerPtr = std::shared_ptr<ReactorTimer>;

class ReactorTimer
{
public:
	ReactorTimer(std::string name, std::function<double(double)> callback, double waketime);
	~ReactorTimer();
	double m_waketime;
	bool is_delete;
	std::string m_name;
	std::function<double(double)> m_callback;

private:
};

class ReactorMutex
{
private:
public:
	ReactorMutex(SelectReactor *reactor, bool is_locked);
	~ReactorMutex();

	SelectReactor *m_reactor;
	bool m_is_locked;
	bool m_next_pending;

	bool test();
	void __enter__();
	void __exit__(std::string type = "", std::string value = "", std::string tb = "");
};

class ReactorCompletion
{
private:
public:
	ReactorCompletion();
	~ReactorCompletion();

	void test();
	void complete(std::string result);
	void wait(double waketime = _NEVER, std::string waketime_result = "");
};
class ReactorFileHandler
{
private:
public:
	ReactorFileHandler(int fd, std::function<void(double)> _callback);
	~ReactorFileHandler();

	int m_fd;
	std::function<void(double)> m_callback;

	int fileno();
};

// class ReactorWebRequestCallback{
// 	private:

// 	public:
// 		ReactorWebRequestCallback(std::function<void(WebRequest *)> callback, double waketime, WebRequest *wr);
// 		~ReactorWebRequestCallback();

// 		ReactorTimer* m_timer;
// 		std::function<void(WebRequest *)> m_callback;
// 		ReactorCompletion* m_completion;
// 		WebRequest * m_wr;

// 		double invoke(double eventtime);

// };

class SelectReactor
{
public:
	SelectReactor(bool gc_checking = false);
	~SelectReactor();
	double m_NEVER;
	double m_NOW;
	bool m_process;
	std::vector<ReactorTimerPtr> m_timers;
	double m_next_timer;
	std::vector<ReactorFileHandler *> m_fds;

	// ReactorCompletion* register_web_request_callback(std::function<void(WebRequest *)> callback, WebRequest *wr, double waketime = _NOW);
	void update_timer(ReactorTimerPtr timer_handler, double waketime);
	ReactorTimerPtr register_timer(std::string name, std::function<double(double)> callback, double waketime = _NEVER);
	void unregister_timer(ReactorTimerPtr& timer);
	void delay_unregister_timer(ReactorTimerPtr& timer);
	double _check_timers(double eventtime, bool busy);
	void completion();
	void register_callback(std::function<void(double)> callback, double waketime = _NOW);
	void register_async_callback(std::function<double(double)> callback, double waketime = _NOW);
	void _dispatch_loop();
	void async_complete(std::string completion, std::string result);
	void _got_pipe_signal(double eventtime);
	void _setup_async_callbacks();
	double _sys_pause(double waketime);
	double pause(double waketime);
	void _end_greenlet(std::string g_old);
	ReactorMutex *mutex(bool is_locked = false);
	ReactorFileHandler *register_fd(int fd, std::function<void(double)> callback);
	void unregister_fd(ReactorFileHandler *file_handler);
	void run();
	void end();
	void finalize();
	bool is_timer_valid(const ReactorTimerPtr& timer) const;

private:
	int m_check_timer_depth = 0;  // 嵌套深度计数
    std::vector<ReactorTimerPtr> m_pending_delete;  // 待删除队列
};

#endif