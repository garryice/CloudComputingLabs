#include <sys/select.h>
#include "reactor.hpp"

namespace tcp_server
{

Reactor::Reactor(std::size_t thread_num)
    : callback_workers_(thread_num)
    , poll_stop_(false)
{
    poll_worker_ = std::thread(std::bind(&Reactor::poll, this));
}

Reactor::~Reactor()
{
    poll_stop_ = true;

    if (poll_worker_.joinable())
        poll_worker_.join();
}

void Reactor::register_fd(int fd,
                          const event_handler_t &rd_callback,
                          const event_handler_t &wr_callback)
{
    std::lock_guard<std::mutex> lock(tracked_fds_mutex_);

    auto &info = tracked_fds_[fd];
    info.rd_callback = rd_callback;
    info.wr_callback = wr_callback;
    info.marked_untrack = false;

    notifier_.notify();
}

void Reactor::set_thread_num(std::size_t thread_num)
{
    callback_workers_.set_thread_num(thread_num);
}

void Reactor::set_rd_callback(int fd, const event_handler_t &cb)
{
    std::lock_guard<std::mutex> lock(tracked_fds_mutex_);

    auto &info = tracked_fds_[fd];
    info.rd_callback = cb;

    // Force poll_worker_ to wake up.
    notifier_.notify();
}

void Reactor::set_wr_callback(int fd, const event_handler_t &cb)
{
    std::lock_guard<std::mutex> lock(tracked_fds_mutex_);

    auto &info = tracked_fds_[fd];
    info.wr_callback = cb;

    notifier_.notify();
}

void Reactor::unregister(int fd)
{
    std::lock_guard<std::mutex> lock(tracked_fds_mutex_);

    // In case this is called multiple times.
    if (!tracked_fds_.count(fd))
        return;

    auto &info = tracked_fds_[fd];
    if (info.is_executing_rd_cb || info.is_executing_wr_cb)
    {
        // Let the callback worker erase it.
        info.marked_untrack = true;
    }
    else
    {
        // Remove it immediately.
        auto it = tracked_fds_.find(fd);
        tracked_fds_.erase(it);
        removal_cond.notify_all();
    }

    notifier_.notify();
}

/// TODO: Add epoll for linux.
void Reactor::poll()
{
    while (!poll_stop_)
    {
        int nfds = init_select_fds();
        int ret = ::select(nfds, &rd_set_, &wr_set_, NULL, NULL);
        if (ret > 0)
            dispatch();
        else if (errno == EINTR)
            continue;
        else
        {
            // TODO: add log.
        }
    }
}

int Reactor::init_select_fds()
{
    std::lock_guard<std::mutex> lock(tracked_fds_mutex_);

    // Clear previous status.
    polled_fds_.clear();
    FD_ZERO(&rd_set_);
    FD_ZERO(&wr_set_);

    // Notifier.
    int nfds = notifier_.get_read_fd();
    FD_SET(notifier_.get_read_fd(), &rd_set_);
    polled_fds_.push_back(notifier_.get_read_fd());

    // process all tracked fds.
    for (const auto &p : tracked_fds_)
    {
        const auto &fd = p.first;
        const auto &info = p.second;

        bool exec_rd = info.rd_callback && !info.is_executing_rd_cb;
        if (exec_rd)
            FD_SET(fd, &rd_set_);

        bool exec_wr = info.wr_callback && !info.is_executing_wr_cb;
        if (exec_wr)
            FD_SET(fd, &wr_set_);

        if (exec_rd || exec_wr || info.marked_untrack)
            polled_fds_.push_back(fd);

        nfds = (exec_rd || exec_wr) ? std::max(nfds, fd) : nfds;
    }

    return nfds + 1;
}

void Reactor::dispatch()
{
    dispatch_select();
}

void Reactor::dispatch_select()
{
    std::lock_guard<std::mutex> lock(tracked_fds_mutex_);

    for (int fd : polled_fds_)
    {
        // Ignore notifier.
        if (fd == notifier_.get_read_fd() && FD_ISSET(fd, &rd_set_))
        {
            notifier_.clear_pipe();
            continue;
        }

        auto it = tracked_fds_.find(fd);
        if (it == tracked_fds_.end())
            continue;

        auto &info = it->second;
        if (FD_ISSET(fd, &rd_set_) && info.rd_callback && !info.is_executing_rd_cb)
            dispatch_read(fd);
        
        if (FD_ISSET(fd, &wr_set_) && info.wr_callback && !info.is_executing_wr_cb)
            dispatch_write(fd);

        if (info.marked_untrack && !info.is_executing_rd_cb && !info.is_executing_wr_cb)
        {
            tracked_fds_.erase(it);
            removal_cond.notify_all();
        }
    }
}

void Reactor::dispatch_read(int fd)
{
    if (!tracked_fds_.count(fd))
        return;

    auto &info = tracked_fds_[fd];

    // Update info.
    auto rd_callback = info.rd_callback;
    info.is_executing_rd_cb = true;

    // Call the user provided callback.
    callback_workers_.add_task([=] {
        rd_callback(fd);

        // Update info associated with this fd.
        std::lock_guard<std::mutex> lock(this->tracked_fds_mutex_);
        auto it = this->tracked_fds_.find(fd);
        if (it == this->tracked_fds_.end())
            // Nothing to do.
            return;

        auto &info = it->second;
        info.is_executing_rd_cb = false;
        // Clear it if [fd] is called with [unregister_fd].
        if (info.marked_untrack && !info.is_executing_wr_cb)
        {
            // Erase the info with this fd.
            this->tracked_fds_.erase(it);
            // Wake the thread that blocks on [wait_on_removal_cond].
            this->removal_cond.notify_all();
        }

        // Wake poll_worker_
        this->notifier_.notify();
    });
}

void Reactor::dispatch_write(int fd)
{
    if (!tracked_fds_.count(fd))
        return;

    auto &info = tracked_fds_[fd];

    auto wr_callback = info.wr_callback;
    info.is_executing_wr_cb = true;

    // Call the user provided callback.
    callback_workers_.add_task([=] {
        wr_callback(fd);

        // Update info associated with this fd.
        std::lock_guard<std::mutex> lock(this->tracked_fds_mutex_);
        auto it = this->tracked_fds_.find(fd);
        if (it == this->tracked_fds_.end())
            return;

        auto &info = it->second;
        info.is_executing_wr_cb = false;
        if (info.marked_untrack && !info.is_executing_rd_cb)
        {
            this->tracked_fds_.erase(it);
            this->removal_cond.notify_all();
        }

        // Wake poll_worker_
        this->notifier_.notify();
    });
}

} // namespace tcp_server
