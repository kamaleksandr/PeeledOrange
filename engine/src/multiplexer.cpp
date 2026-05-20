/* 
 * File:   multiplexer.cpp
 * Author: kamyshev.a
 * 
 * Created on 5 July 2019, 10:32
 */

#define MAX_EPOLL_EVENTS 1024
#define EPOLL_WAIT_TIMEOUT 100

#include <memory>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <semaphore.h>

#include "multiplexer.h"
#include "common.h"
#include "highload/types.h"

using namespace highload;
using namespace types;
using namespace common;

static void MainThreadExit(void *arg) {
#ifdef DEBUG_MODE  
  Log(1, " [Multiplexer] Stopped\r\n");
#endif
}

static void* MainThread(void *arg) {
  types::multiplexer_data_t *md = (types::multiplexer_data_t*)arg;
  Multiplexer *mux = static_cast<Multiplexer*> (md->owner);
  pthread_cleanup_push(MainThreadExit, arg);
  epoll_event event, *occured_event;
  event.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP | EPOLLERR;
  epoll_event *events = md->p_epoll_events.get();
  int32_t epoll_fd = md->epoll.fd;
  TTimer timer;
  int32_t num, i;
  while (1) {
    //pthread_testcancel();
    num = epoll_wait(epoll_fd, events, MAX_EPOLL_EVENTS, EPOLL_WAIT_TIMEOUT);
    if (timer.getMSec() >= EPOLL_WAIT_TIMEOUT) {
      mux->OnTimer();
      timer.Start();
    }
#ifdef DEBUG_MODE
    if (num == -1)
      Log(0, " [Multiplexer] Error, epoll_wait return -1 (%s)\r\n", strerror(errno));
#endif

    if (num < 1) continue;
    for (i = 0; i < num; i++) {     
      occured_event = &events[i];               
      if (occured_event->events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)) {
        mux->OnDisconnect(occured_event->data.fd);
        continue;
      }
      if (occured_event->events & EPOLLIN){
        mux->OnEvent(occured_event->data.fd);
      continue;
      }
    }
  }
  pthread_cleanup_pop(1);
}

Multiplexer::Multiplexer() {
  md.owner = this;
  md.epoll.fd = epoll_create(1024);
  if (md.epoll.fd == -1) {
    throw ( std::runtime_error(
      (std::string)"Create epoll failed (" + strerror(errno) + ")\r\n"));
  }
  md.p_epoll_events.reset(new epoll_event[MAX_EPOLL_EVENTS]);

#ifdef DEBUG_MODE
  Log(1, " [Multiplexer] Created successfully\r\n");
#endif 
}

void Multiplexer::Start() {
  int32_t res = pthread_create(&md.thread.ptr, 0, MainThread, &md);
  if (res) Restart("Create thread failed %s\r\n", strerror(res));
  md.thread.is_valid = 1;

#ifdef DEBUG_MODE
  Log(1, " [Multiplexer] Started\r\n");
#endif
}

void Multiplexer::Close() {
  if (!md.thread.is_valid) return;
  pthread_cancel(md.thread.ptr);
  pthread_join(md.thread.ptr, 0);
  md.thread.is_valid = 0;
}

bool Multiplexer::AddFD(int32_t fd) {
  epoll_event event;
  event.events = EPOLLIN | EPOLLHUP;
  event.data.fd = fd;
  int32_t res = epoll_ctl(md.epoll.fd, EPOLL_CTL_ADD, fd, &event);
  if (res != -1) return 1;
  Log(1, " [Multiplexer] Add socket to epool failed (%s)\r\n", strerror(errno));
  return 0;
}

void Multiplexer::RemoveFD(int32_t fd) {
  if (epoll_ctl(md.epoll.fd, EPOLL_CTL_DEL, fd, 0)) {
#ifdef DEBUG_MODE  
    Log(2, " [Multiplexer] Remove socket (%d) from epoll failed (%s)\r\n",
      fd, strerror(errno));
#endif   
  }
}