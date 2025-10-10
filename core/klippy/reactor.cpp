#include "reactor.h"
#include "klippy.h"
#include "my_string.h"

#define LOG_TAG "reactor"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#include "log.h"

ReactorTimer::ReactorTimer(std::string name, std::function<double(double)> callback, double waketime)
	: m_name(name), m_callback(callback), m_waketime(waketime), is_delete(false) // 初始化删除标记
{
	if (!callback)
	{
		throw std::invalid_argument("Timer callback cannot be null");
	}
}

ReactorTimer::~ReactorTimer()
{
}

ReactorMutex::ReactorMutex(SelectReactor *reactor, bool is_locked)
{
	m_is_locked = is_locked;
	m_next_pending = false;
	// m_queue = [];
	// m_lock = __enter__;
	// m_unlock = __exit__;
}

ReactorMutex::~ReactorMutex()
{
}

bool ReactorMutex::test()
{
	return m_is_locked;
}

void ReactorMutex::__enter__()
{
	if (!m_is_locked)
	{
		m_is_locked = true;
		return;
	}
	// g = greenlet.getcurrent()
	// self.queue.append(g)
	while (1)
	{
		// m_reactor.pause(m_reactor->m_NEVER);
		// if m_next_pending && self.queue[0] is g:
		// 	self.next_pending = False
		// 	self.queue.pop(0)
		// 	return
	}
}

void ReactorMutex::__exit__(std::string type, std::string value, std::string tb)
{
	// if not self.queue:
	// 	self.is_locked = False
	// 	return
	// self.next_pending = True
	// self.reactor.update_timer(self.queue[0].timer, self.reactor.NOW)
}

ReactorCompletion::ReactorCompletion()
{
	// self.reactor = reactor
	// self.result = self.sentinel
	// self.waiting = []
}

ReactorCompletion::~ReactorCompletion()
{
}

void ReactorCompletion::test()
{
	// return self.result is not self.sentinel;
}

void ReactorCompletion::complete(std::string result)
{
	// self.result = result
	// for wait in self.waiting:
	// 	self.reactor.update_timer(wait.timer, self.reactor.NOW)
}

void ReactorCompletion::wait(double waketime, std::string waketime_result)
{
	// if self.result is self.sentinel:
	// 	wait = greenlet.getcurrent()
	// 	self.waiting.append(wait)
	// 	self.reactor.pause(waketime)
	// 	self.waiting.remove(wait)
	// 	if self.result is self.sentinel:
	// 		return waketime_result
	// return self.result
}

// ReactorWebRequestCallback::ReactorWebRequestCallback(std::function<void(WebRequest *)> callback, double waketime, WebRequest *wr)
// {
// 	ReactorTimer* m_timer = Printer::GetInstance()->m_reactor->register_timer(std::bind(&ReactorWebRequestCallback::invoke, this, std::placeholders::_1), waketime);
// 	std::function<void(WebRequest *)> m_callback = callback;
// 	ReactorCompletion* m_completion = new ReactorCompletion();
// 	WebRequest * m_wr = wr;
// }

// ReactorWebRequestCallback::~ReactorWebRequestCallback()
// {

// }

// double ReactorWebRequestCallback::invoke(double eventtime)
// {
// 	Printer::GetInstance()->m_reactor->unregister_timer(&m_timer);
// 	m_callback(m_wr);
// 	// m_completion.complete(res);
// 	return _NEVER;
// }

ReactorFileHandler::ReactorFileHandler(int fd, std::function<void(double)> _callback)
{
	m_fd = fd;
	m_callback = _callback;
}

ReactorFileHandler::~ReactorFileHandler()
{
}

int ReactorFileHandler::fileno()
{
	return m_fd;
}

SelectReactor::SelectReactor(bool gc_checking)
{
	m_NEVER = _NEVER;
	m_NOW = _NOW;
	// Main code
	m_process = false;
	// Python garbage collection
	// self._check_gc = gc_checking
	// self._last_gc_times = [0., 0., 0.]
	// Timers
	m_next_timer = _NEVER;
	// Callbacks
	// self._pipe_fds = None
	// self._async_queue = queue.Queue()
	// File descriptors
	// Greenlets
	// self._g_dispatch = None
	// self._greenlets = []
	// self._all_greenlets = []
}

SelectReactor::~SelectReactor()
{
	// 清理所有定时器
	// for (auto *timer : m_timers)
	// {
	// 	if (timer)
	// 	{
	// 		delete timer;
	// 	}
	// }
	// m_timers.clear();
}

// ReactorCompletion* SelectReactor::register_web_request_callback(std::function<void(WebRequest *)> callback, WebRequest *wr, double waketime)
// {
// 	ReactorWebRequestCallback *rcb = new ReactorWebRequestCallback(callback, waketime, wr);
// 	return rcb->m_completion;
// }

void SelectReactor::update_timer(ReactorTimerPtr timer_handler, double waketime)
{
	timer_handler->m_waketime = waketime;
	m_next_timer = std::min(m_next_timer, waketime);
}

ReactorTimerPtr SelectReactor::register_timer(std::string name, std::function<double(double)> callback, double waketime)
{
	auto timer = std::make_shared<ReactorTimer>(name, callback, waketime);
	LOG_D("Registering timer: %s, waketime=%.3f, addr=%p\n",
		  name.c_str(), waketime, timer.get());
	m_timers.push_back(timer);
	m_next_timer = std::min(m_next_timer, waketime);
	return timer;
}

void SelectReactor::unregister_timer(ReactorTimerPtr &timer)
{
	if (!timer)
		return;

	timer->is_delete = true;
	timer->m_waketime = m_NEVER;
}

void SelectReactor::delay_unregister_timer(ReactorTimerPtr &timer)
{
	if (!timer)
		return;

	timer->is_delete = true;
	timer->m_waketime = m_NEVER;
}

double SelectReactor::_check_timers(double eventtime, bool busy)
{
	m_check_timer_depth++;

	// 只在最外层检查时间
	if (m_check_timer_depth == 1 && eventtime < m_next_timer)
	{
		m_check_timer_depth--;
		if (busy)
			return 0.;
		return std::min(1., std::max(.001, m_next_timer - eventtime));
	}

	// 只在最外层重置next_timer
	if (m_check_timer_depth == 1)
	{
		m_next_timer = m_NEVER;
	}

	// 使用临时vector存储当前需要执行的定时器
	std::vector<ReactorTimerPtr> current_timers;
	current_timers.reserve(m_timers.size());
	
	// 复制当前需要执行的定时器列表，避免在执行过程中的修改影响遍历
	for (const auto &timer : m_timers)
	{
		if (!timer || timer->is_delete)
			continue;
			
		if (eventtime >= timer->m_waketime)
		{
			current_timers.push_back(timer);
		}
		else if (m_check_timer_depth == 1)
		{
			m_next_timer = std::min(m_next_timer, timer->m_waketime);
		}
	}

	// 执行当前收集到的定时器回调
	for (const auto &timer : current_timers)
	{
		// 再次检查定时器状态
		if (!timer || timer->is_delete)
			continue;

		double next_waketime = m_NEVER;
		try
		{
			LOG_D("Executing timer %s (depth=%d)\n",
				  timer->m_name.c_str(), m_check_timer_depth);
			
			// 保存当前waketime，以便在回调中被注销时使用
			double old_waketime = timer->m_waketime;
			timer->m_waketime = m_NEVER;
			
			// 执行回调
			next_waketime = timer->m_callback(eventtime);
			
			// 如果定时器在回调中被注销，不更新waketime
			if (!timer->is_delete)
			{
				timer->m_waketime = next_waketime;
				if (m_check_timer_depth == 1)
				{
					m_next_timer = std::min(m_next_timer, next_waketime);
				}
			}
		}
		catch (const std::exception &e)
		{
			LOG_E("Timer %s callback error: %s\n",
				  timer->m_name.c_str(), e.what());
			timer->is_delete = true;
		}
	}

	// 只在最外层清理定时器列表
	if (m_check_timer_depth == 1)
	{
		auto new_end = std::remove_if(m_timers.begin(), m_timers.end(),
			[](const ReactorTimerPtr &timer) {
				return !timer || timer->is_delete;
			});
		m_timers.erase(new_end, m_timers.end());
	}

	m_check_timer_depth--;
	return 0.;
}

void SelectReactor::completion()
{
	// return ReactorCompletion(self);
}

void SelectReactor::register_callback(std::function<void(double)> callback, double waketime)
{
	// rcb = ReactorCallback(callback, waketime);
	// return rcb.completion;
}

// Asynchronous (from another thread) callbacks and completions
void SelectReactor::register_async_callback(std::function<double(double)> callback, double waketime)
{
	// self._async_queue.put_nowait(
	// 	(ReactorCallback, (self, callback, waketime)))
	// try:
	// 	os.write(self._pipe_fds[1], '.')
	// except os.error:
	// 	pass
}

void SelectReactor::async_complete(std::string completion, std::string result)
{
	// self._async_queue.put_nowait((completion.complete, (result,)))
	// try:
	// 	os.write(self._pipe_fds[1], '.')
	// except os.error:
	// 	pass
}

void SelectReactor::_got_pipe_signal(double eventtime)
{
	// try:
	// 	os.read(self._pipe_fds[0], 4096)
	// except os.error:
	// 	pass
	// while 1:
	// 	try:
	// 		func, args = self._async_queue.get_nowait()
	// 	except queue.Empty:
	// 		break
	// 	func(*args)
}

void SelectReactor::_setup_async_callbacks()
{
	// self._pipe_fds = os.pipe()
	// util.set_nonblock(self._pipe_fds[0])
	// util.set_nonblock(self._pipe_fds[1])
	// self.register_fd(self._pipe_fds[0], self._got_pipe_signal)
}

// Greenlets
double SelectReactor::_sys_pause(double waketime)
{
	// Pause using system sleep for when reactor not running
	double delay = waketime - get_monotonic();
	if (delay > 0.)
	{
		usleep(delay * 10e6);
		return get_monotonic();
	}
}

double SelectReactor::pause(double waketime)
{
	_check_timers(waketime, true);
	return get_monotonic();
	// g = greenlet.getcurrent()
	// if g is not self._g_dispatch:
	// 	if self._g_dispatch is None:
	// 		return self._sys_pause(waketime)
	// 	# Switch to _check_timers (via g.timer.callback return)
	// 	return self._g_dispatch.switch(waketime)
	// # Pausing the dispatch greenlet - prepare a new greenlet to do dispatch
	// if self._greenlets:
	// 	g_next = self._greenlets.pop()
	// else:
	// 	g_next = ReactorGreenlet(run=self._dispatch_loop)
	// 	self._all_greenlets.append(g_next)
	// g_next.parent = g.parent
	// g.timer = self.register_timer(g.switch, waketime)
	// self._next_timer = self.NOW
	// # Switch to _dispatch_loop (via _end_greenlet or direct)
	// eventtime = g_next.switch()
	// # This greenlet activated from g.timer.callback (via _check_timers)
	// return eventtime
}

void SelectReactor::_end_greenlet(std::string g_old)
{
	// Cache this greenlet for later use
	// self._greenlets.append(g_old)
	// self.unregister_timer(g_old.timer)
	// g_old.timer = None
	// # Switch to _check_timers (via g_old.timer.callback return)
	// self._g_dispatch.switch(self.NEVER)
	// # This greenlet reactivated from pause() - return to main dispatch loop
	// self._g_dispatch = g_old
}

// Mutexes
ReactorMutex *SelectReactor::mutex(bool is_locked)
{
	return new ReactorMutex(this, is_locked);
}

// File descriptors
ReactorFileHandler *SelectReactor::register_fd(int fd, std::function<void(double)> callback)
{
	ReactorFileHandler *file_handler = new ReactorFileHandler(fd, callback);
	m_fds.push_back(file_handler);
	return file_handler;
}

void SelectReactor::unregister_fd(ReactorFileHandler *file_handler)
{
	std::vector<ReactorFileHandler *>::iterator iter;
	iter = std::find(m_fds.begin(), m_fds.end(), file_handler);
	if (iter != m_fds.end())
	{
		m_fds.erase(iter);
	}
}

void SelectReactor::run()
{
	// if self._pipe_fds is None:
	// 	self._setup_async_callbacks()
	// self._process = True
	// g_next = ReactorGreenlet(run=self._dispatch_loop)
	// self._all_greenlets.append(g_next)
	// g_next.switch()
}

void SelectReactor::end()
{
	m_process = false;
}

void SelectReactor::finalize()
{
	// self._g_dispatch = None
	// self._greenlets = []
	// for g in self._all_greenlets:
	// 	try:
	// 		g.throw()
	// 	except:
	// 		logging.exception("reactor finalize greenlet terminate")
	// self._all_greenlets = []
	// if self._pipe_fds is not None:
	// 	os.close(self._pipe_fds[0])
	// 	os.close(self._pipe_fds[1])
	// 	self._pipe_fds = None
}

bool SelectReactor::is_timer_valid(const ReactorTimerPtr &timer) const
{
	if (!timer)
	{
		LOG_D("Checking null timer\n");
		return false;
	}

	// 检查定时器是否在活动列表中
	bool is_valid = std::find(m_timers.begin(), m_timers.end(), timer) != m_timers.end();

	// 检查是否已被标记删除
	if (is_valid && timer->is_delete)
	{
		LOG_D("Timer %s marked for deletion\n", timer->m_name.c_str());
		return false;
	}

	// 检查是否在待删除队列中
	if (is_valid && std::find(m_pending_delete.begin(), m_pending_delete.end(), timer) != m_pending_delete.end())
	{
		LOG_D("Timer %s in pending delete queue\n", timer->m_name.c_str());
		return false;
	}

	LOG_D("Timer %s validity check: %d\n",
		  timer->m_name.c_str(), is_valid);
	return is_valid;
}
