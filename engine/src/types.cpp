/* 
 * File:   types.cpp
 * Author: kamyshev.a
 * 
 * Created on 28 September 2018, 13:12
 */

#include <unistd.h>
#include <memory>
#include <map>
#include <sys/epoll.h>

#include "highload/types.h"

using namespace highload::types;

data_storage_t::data_storage_t() {
  capacity = 0;
  pos = 0;
}

abstract_extension_t::~abstract_extension_t() {
}

connection_data_t::connection_data_t() {
  sock_fd = -1;
  required_size = 0;
  close_connection = 0;
#ifdef TLS_MODE
  bio = 0;
  handshake = 0;
#endif
}

connection_data_t::~connection_data_t() {
#ifdef TLS_MODE
  if (bio) BIO_free_all(bio);
#endif
  if (sock_fd != -1) close(sock_fd);
}

thread_data_t::thread_data_t() {
  is_valid = 0;
}

thread_data_t::~thread_data_t() {
  if (!is_valid) return;
  pthread_cancel(ptr);
  pthread_join(ptr, 0);
}

epoll_fd_t::epoll_fd_t() {
  fd = -1;
}

epoll_fd_t::~epoll_fd_t() {
  if (fd != -1) close(fd);
}

listener_data_t::listener_data_t() {
  sock_fd = -1;
}

listener_data_t::~listener_data_t() {
  if (sock_fd != -1) close(sock_fd);
  sock_fd = -1;
}
