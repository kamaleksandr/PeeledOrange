/* 
 * File:   types.h
 * Author: kamyshev.a
 *
 * Created on 28 September 2018, 13:12
 */

#ifndef HL_SERVER_TYPES_H
#define HL_SERVER_TYPES_H

#define LISTEN_QUEUE_SIZE 256
#define RECV_BUFFER_SIZE 1024
#define MAX_EPOLL_EVENTS 1024

#include <memory>
#include <map>
#include <deque>
#include <sys/epoll.h>
#include <semaphore.h>

#include "common.h"

#ifdef TLS_MODE
#include "ssl/open_ssl.h"
#endif

namespace highload {
  namespace types {

    struct buffer_t {
      buffer_t(): size(0){};
      std::unique_ptr<uint8_t> data;
      uint32_t size;
    };

    struct data_storage_t : buffer_t {
      data_storage_t();
      uint32_t capacity;
      uint32_t pos;
    };
    
    struct abstract_extension_t{
      virtual ~abstract_extension_t(); 
    };
    
    struct connection_data_t {
      connection_data_t();
      ~connection_data_t();
      //void(*OnDisconnect)(connection_data_t*);
      std::unique_ptr<abstract_extension_t> extension;
      int32_t sock_fd;
      uint32_t required_size, last_time, timeout;
      uint8_t close_connection;
#ifdef TLS_MODE
      BIO *bio;
      bool handshake;
#endif 
      data_storage_t input;
    };

    typedef std::unique_ptr<connection_data_t> p_connection_t;
    typedef std::map<int32_t, p_connection_t> connections_map;
    
    struct server_port_t{
      int32_t port, sock_fd;
      uint32_t max_conns, connect_timeout, max_header_size;
      std::string name;
      connections_map connections;
    };
    
    typedef std::unique_ptr<server_port_t> p_server_port_t; 
    typedef std::map<int32_t, p_server_port_t> server_ports_map;
    
    /**
     * Contains connection and server port data.
     */
    struct context_data_t{
      //context_data_t(connection_data_t &cd, server_port_t &sp):
      context_data_t(connection_data_t *cd, server_port_t *sp):
        connection(cd), server(sp){}
      context_data_t():connection(0), server(0){};
      //context_data_t get(){ return *this; }
      //connection_data_t &connection;
      //server_port_t &server;    
      connection_data_t *connection;
      server_port_t *server;      
    };
    
    struct thread_data_t {
      thread_data_t();
      ~thread_data_t();
      pthread_t ptr;
      uint8_t is_valid;
    };

    struct epoll_fd_t {
      epoll_fd_t();
      ~epoll_fd_t();
      int32_t fd;
    };

    struct multiplexer_data_t {
      void *owner;
      thread_data_t thread;
      types::epoll_fd_t epoll;
      std::unique_ptr<epoll_event> p_epoll_events;
    };

    struct listener_data_t {
      listener_data_t();
      ~listener_data_t();
      thread_data_t thread;
      uint32_t max_conns_count;
      int32_t sock_fd;
    };

    typedef std::unique_ptr< listener_data_t > p_listener_t;

  } // namespace types 
} // namespace highload
#endif /* HL_SERVER_TYPES_H */

