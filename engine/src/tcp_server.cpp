/* 
 * File:   tcp_server.cpp
 * Author: kamyshev.a
 * 
 * Created on 30 July 2019, 16:02
 */
#include <arpa/inet.h>
#include <string.h>
#include <math.h>

#include "tcp_server.h"
#include "ini_file.h"
#include "common.h"
//#include "granitx1_server.h"

#ifdef TLS_MODE
#include "ssl/open_ssl.h"

#include <sys/stat.h>
#define CHECK_SSL_PERIOD 3600
#endif

#define CHECK_EXPIRED_STEP 100

using namespace highload;
using namespace types;
using namespace common;

#ifdef TLS_MODE
common::TTimer ssl_timer;
uint64_t ssl_time = 0;

void CheckSSLCertificat(std::string path) {

#ifdef DEBUG_MODE  
  Log(2, " [TCPServer] Check SSL certificate\r\n");
#endif  

  FILE *file = fopen((path + "fullchain.pem").c_str(), "r");
  if (!file) {
    Log(0, " [TCPServer] Check SSL certificat fail: '%s'\r\n", strerror(errno));
    return;
  }
  struct stat statbuf;
  fstat(fileno(file), &statbuf);
  fclose(file);
  if (!ssl_time)
    ssl_time = statbuf.st_mtim.tv_sec;
  if (ssl_time < statbuf.st_mtim.tv_sec)
    Restart(" [TCPServer] Updated certificate found, restart\r\n");
}
#endif

static inline connection_data_t* NewConnection() {
  try {
    return new connection_data_t;
  } catch (std::exception &e) {
    Restart(" [TCPServer] Create new connection_data_t failed (%s)\r\n", e.what());
  }
  return 0;
}

static inline uint8_t* NewUInt8(uint32_t size) {
  try {
    return new uint8_t[size];
  } catch (std::exception &e) {
    Restart(" [TCPServer] NewChar( %d ) failed (%s)\r\n", size, e.what());
  }
  return 0;
}

TCPServer::TCPServer() {
  time(&operating_time);
  current_time = operating_time;
  now_time = operating_time;
  last_checked_fd = 0;
  OnSecTimer = [] {
  };
#ifdef DATABASE_CLIENT 
  db_client = 0;
#endif     
}

bool TCPServer::Open(const char *name) {

#ifdef DATABASE_CLIENT 
  if (!db_client) {
    Log(0, " [TCPServer] No database client specified\r\n");
    return 0;
  }
#endif

  IniFile ini_file;
  std::string fn = std::string(name) + ".cfg";
  if (!ini_file.Open(fn)) {
    Log(0, " [TCPServer] Configuration file %s is not available\r\n", fn.c_str());
    return 0;
  }

  uint32_t count = ini_file.ReadInteger("tcp server", "instances_count", 1);
  for (uint32_t i = 0; i < count; i++) {
    p_server_port_t ptr(new server_port_t);
    server_port_t *sp = ptr.get();
    sp->name =
      ini_file.ReadString("tcp server", "name_" + std::to_string(i), "noname");
    sp->port = ini_file.ReadInteger("tcp server", sp->name + "_port", 80);
    sp->max_conns =
      ini_file.ReadInteger("tcp server", sp->name + "_max_conns", 1024);
    sp->connect_timeout =
      ini_file.ReadInteger("tcp server", sp->name + "_connect_timeout", 60);
    sp->max_header_size =
      ini_file.ReadInteger("tcp server", sp->name + "_max_header_size", 2048);
    Log(1, " [TCPServer] Config: "
      "name %s, "
      "port %d, "
      "max connections %d, "
      "connect timeout %d sec, "
      "max header size %d\r\n",
      sp->name.c_str(),
      sp->port,
      sp->max_conns,
      sp->connect_timeout,
      sp->max_header_size);

    if (!AddListenSocket(move(ptr))) return 0;
  }

#ifdef TLS_MODE
  try {
    ssl.reset(new openssl::TOpenSSL);
  } catch (std::exception &e) {
    Log(0, " [TCPServer] %s", e.what());
    return 0;
  }
  ssl_dir_path = ini_file.ReadString("tcp server", "ssl_dir_path", "");
  if (ssl_dir_path.empty())
    ssl_dir_path = ParentDirectory(name);
  else if (ssl_dir_path[ssl_dir_path.size() - 1] != '/')
    ssl_dir_path += "/";
  Log(1, (" [TCPServer] SSL certificate directory: '"
    + ssl_dir_path + "'\r\n").c_str());
  (*ssl).certificat = ssl_dir_path + "fullchain.pem";
  (*ssl).private_key = ssl_dir_path + "privkey.pem";
  if (!(*ssl).GetContext()) {
    Log(0, " [TCPServer] GetSSL failed\r\n");
    while (const char *err = openssl::GetErrorString())
      Log(0, " [OpenSSL] %s\r\n", err);
    return 0;
  }
  CheckSSLCertificat(ssl_dir_path);
#endif

  Log(1, " [TCPServer] Opened \r\n");
  return 1;
}

TCPServer::~TCPServer() { }

int32_t TCPServer::ReceiveData(connection_data_t *cd) {

#ifdef TLS_MODE
  int res = BIO_read(cd->bio,
    cd->input.data.get() + cd->input.size,
    cd->input.capacity - cd->input.size);
  if (res == -1) { // it is possible to check error EPIPE (Broken pipe)
    Log(2, " [TCPServer] BIO write failed\r\n");
    while (const char *err = openssl::GetErrorString())
      Log(2, " [OpenSSL] %s\r\n", err);
  }
  return res;
#else
  return recv(cd->sock_fd,
    cd->input.data.get() + cd->input.size,
    cd->input.capacity - cd->input.size, 0);
#endif
  return 0;
}

int32_t TCPServer::SendData(connection_data_t *cd, const void* data, uint32_t size) {

#ifdef TLS_MODE
  // MSG_NOSIGNAL ???
  int res = BIO_write(cd->bio, data, size);
  if (res == -1) { // it is possible to check error EPIPE (Broken pipe)
    Log(2, " [TCPServer] BIO write failed\r\n");
    while (const char *err = openssl::GetErrorString())
      Log(2, " [OpenSSL] %s\r\n", err);
  }
  return res;
#else

  int32_t sended = send(cd->sock_fd, data, size, 0);

  //Log(3, " [TCPServer] sended %d from %d\r\n", sended, size);

  if (sended < size) {
    Log(0, " [TCPServer] SendData error, sended %d from %d\r\n", sended, size);
  }

  return sended;
#endif
}

void TCPServer::EraseExpiredConnections() {
  int32_t counter = 0;
  context_data_t cx;
  auto it_srv = server_ports.begin();
  for (; it_srv != server_ports.end(); it_srv++) {
    std::deque<int32_t> expired_conns;
    server_port_t *sp = it_srv->second.get();
    connections_map::iterator it_cnt;
    if (last_checked_fd) {
      for (; it_srv != server_ports.end(); it_srv++) {
        it_cnt = sp->connections.find(last_checked_fd);
        if (it_cnt++ != sp->connections.end()) break;
      }
      last_checked_fd = 0;
    } else it_cnt = sp->connections.begin();

    for (; it_cnt != sp->connections.end(); it_cnt++) {
      connection_data_t *cd = it_cnt->second.get();
      if (cd->last_time + cd->timeout < now_time) {
        expired_conns.push_back(it_cnt->first);
      }
      if (++counter >= CHECK_EXPIRED_STEP) {
        last_checked_fd = it_cnt->first;
        break;
      }
    }

    std::deque< int32_t >::iterator it_exp = expired_conns.begin();
    for (; it_exp != expired_conns.end(); ++it_exp) {
      sp->connections.erase(*it_exp);
      
#ifdef DEBUG_MODE
      Log(2, " [TCPServer] Client disconnect on timeout from '%s'\r\n", sp->name.c_str());
#endif  
    }
  }
  if (counter != CHECK_EXPIRED_STEP) last_checked_fd = 0;
}

void TCPServer::ConnectionProcessing(context_data_t &cx) {

#ifdef TLS_MODE
  if (!cd->handshake) {
    res = BIO_do_handshake(cd->bio);
    if (res == 1) cd->handshake = 1;
    return;
  }
#endif

#ifdef DEBUG_MODE
  if (cx.connection->input.size > cx.connection->input.capacity) {
    Log(2, " [TCPServer] Invalid input buffer, capacity = %d, size = %d\r\n",
      cx.connection->input.capacity, cx.connection->input.size);
    Disconnect(cx);
    return;
  }
#endif 

  int32_t res = ReceiveData(cx.connection);

  //Log(2, " [TCPServer] Receive data, size = %d\r\n", res);  

  if (res < 1) {

#ifdef DEBUG_MODE
    if (res == -1) {
      Log(2, " [TCPServer] Receive data failed (%s)\r\n", strerror(errno));
    }
#endif

    Disconnect(cx);
    return;
  }
  cx.connection->last_time = now_time;
  cx.connection->input.size += res;

  //Log(2, " [TCPServer] Receive data, input size = %d, pos = %d, required_size = %d\r\n"
  //  , cd->input.size, cd->input.pos, cd->required_size);  

  if (cx.connection->required_size) {
    if (cx.connection->required_size > cx.connection->input.size) return;
    cx.connection->required_size = 0;
  }

  OnReadData(cx);

  if (cx.connection->close_connection) {

#ifdef DEBUG_MODE
    Log(3, " [TCPServer] Close connection after processing(%d)\r\n",
      cx.connection->sock_fd);
#endif

    Disconnect(cx);
    return;
  }

#ifdef DEBUG_MODE
  if (cx.connection->input.pos > cx.connection->input.size) {
    Log(2, " [TCPServer] Invalid input buffer, size = %d, pos = %d\r\n",
      cx.connection->input.size, cx.connection->input.pos);
    Disconnect(cx);
    return;
  }
#endif

  cx.connection->input.size -= cx.connection->input.pos;
  if (!cx.connection->required_size &&
    cx.connection->input.size > cx.server->max_header_size) {

#ifdef DEBUG_MODE
    Log(2, " [TCPServer] Excess max header size %d\r\n",
      cx.server->max_header_size);
#endif

    Disconnect(cx);
    return;
  }
  if (cx.connection->required_size)
    required_capacity = ceil((float) cx.connection->required_size /
    (float) RECV_BUFFER_SIZE) * RECV_BUFFER_SIZE + RECV_BUFFER_SIZE;
  else required_capacity = ceil((float) cx.connection->input.size /
    (float) RECV_BUFFER_SIZE) * RECV_BUFFER_SIZE + RECV_BUFFER_SIZE;
  if (cx.connection->input.capacity != required_capacity) { // change receive buffer capacity
    buf = NewUInt8(required_capacity);
    if (cx.connection->input.size)
      memcpy(buf, cx.connection->input.data.get() + cx.connection->input.pos, cx.connection->input.size);
    cx.connection->input.data.reset(buf);
    cx.connection->input.capacity = required_capacity;
  } else if (cx.connection->input.size)
    memcpy(cx.connection->input.data.get(), cx.connection->input.data.get() + cx.connection->input.pos, cx.connection->input.size);
  cx.connection->input.pos = 0;
}

#ifdef TLS_MODE

void SSLError(std::string message) {
  while (const char *err = openssl::GetErrorString())
    Log(0, " [OpenSSL] %s\r\n", err);
  Restart(message.c_str());
}
#endif

void TCPServer::OnTimer() {
  time(&operating_time);
  now_time = operating_time;

#ifdef DATABASE_CLIENT
  db_client->OnTimer(now_time);
#endif     

  if (now_time - current_time) {
    current_time = now_time;
    EraseExpiredConnections();
    OnSecTimer();

#ifdef TLS_MODE   
    if (ssl_timer.GetSec() > CHECK_SSL_PERIOD) {
      ssl_timer.Start();
      CheckSSLCertificat(ssl_dir_path);
    }
#endif      
  } else if (last_checked_fd) EraseExpiredConnections();
}

void TCPServer::OnEvent(int32_t fd) {
  context_data_t cx;
  auto port = server_ports.find(fd);
  if (port != server_ports.end()) {
    cx.server = port->second.get();
    sockaddr_in client_addr;
    socklen_t sin_size = sizeof (sockaddr_in);
    //fcntl(listener->sock_fd, F_SETFL, fcntl(listener->sock_fd, F_GETFL, 0) | O_NONBLOCK);
    //int32_t sock_fd = accept4(fd, (sockaddr*) & client_addr, &sin_size, SOCK_NONBLOCK);
    int32_t sock_fd = accept(fd, (sockaddr*) & client_addr, &sin_size);
    if (sock_fd == -1) {
      Log(1, " [TCPServer] Accept failed (%s)\r\n", strerror(errno));
      return;
    }

#ifdef SPECIAL_LOG
    SpecLog(" [TCPServer] Client connected(%d) to '%s'\r\n",
      sock_fd, cx.server->name.c_str());
#endif

#ifdef DEBUG_MODE
    Log(3, " [TCPServer] Client connected(%d) to '%s'\r\n",
      sock_fd, cx.server->name.c_str());
#endif

    p_connection_t pcd(NewConnection());
    cx.connection = pcd.get();
    cx.connection->sock_fd = sock_fd;
    cx.connection->input.data.reset(NewUInt8(RECV_BUFFER_SIZE));
    cx.connection->input.capacity = RECV_BUFFER_SIZE;
    cx.connection->last_time = now_time;
    cx.connection->timeout = cx.server->connect_timeout;

#ifdef TLS_MODE
    BIO *bio = BIO_new_socket(sock_fd, BIO_NOCLOSE);
    if (!bio) SSLError(" [TCPServer] BIO_new_socket failed\r\n");
    if (!(cd->bio = BIO_new_ssl((*ssl).GetContext(), 0)))
      SSLError(" [TCPServer] BIO_new_ssl failed\r\n");
    SSL *ssl = 0;
    BIO_get_ssl(cd->bio, &ssl);
    if (!ssl) return;
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    SSL_set_bio(ssl, bio, bio);
#endif

    OnConnect(cx);
    if (!AddFD(sock_fd)) return;
    cx.server->connections[sock_fd] = std::move(pcd);
    return;
  }

#ifdef DATABASE_CLIENT
  if (db_client->DataProcessing(fd)) return;
#endif

  if (!getContext(fd, cx)) {
    //RemoveFD(fd);
    return;
  }
  ConnectionProcessing(cx);
}

void TCPServer::OnDisconnect(int32_t fd) {
  context_data_t cx;
  if (!getContext(fd, cx)) return;
  Disconnect(cx);
}

void TCPServer::Disconnect(types::context_data_t &cx) {

#ifdef SPECIAL_LOG
  SpecLog(" [TCPServer] Client disconnect(%d) from '%s'\r\n",
    cx.connection->sock_fd, cx.server->name.c_str());
#endif  

#ifdef DEBUG_MODE
  Log(2, " [TCPServer] Client disconnect(%d) from '%s'\r\n",
    cx.connection->sock_fd, cx.server->name.c_str());
#endif

  cx.server->connections.erase(cx.connection->sock_fd);
  OnDisconnect(cx);
}

bool TCPServer::AddListenSocket(p_server_port_t psp) {
  server_port_t *sp = psp.get();
  sp->sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sp->sock_fd == -1) {
    Log(0, " [TCPServer] Create listening socked failed (%s)\r\n", strerror(errno));
    return 0;
  }

  sockaddr_in srv_addr;
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_port = htons(sp->port);
  srv_addr.sin_addr.s_addr = INADDR_ANY;
  sockaddr_in *psrv_addr = &srv_addr;

  int32_t val = 1;
  //setsockopt(sp->sock_fd, SOL_SOCKET, SO_OOBINLINE, (void *)&val, sizeof(int));
  setsockopt(sp->sock_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof ( val));
  setsockopt(sp->sock_fd, SOL_SOCKET, SOCK_NONBLOCK, &val, sizeof ( val));

  if (bind(sp->sock_fd, (sockaddr*) psrv_addr, sizeof (sockaddr)) == -1) {
    Log(0, " [TCPServer] Bind socket address failed (%s)\r\n", strerror(errno));
    return 0;
  }

  if (listen(sp->sock_fd, LISTEN_QUEUE_SIZE) == -1) {
    Log(0, " [TCPServer] Create listen queue failed (%s)\r\n", strerror(errno));
    return 0;
  }
  if (!AddFD(sp->sock_fd)) {
    Log(0, " [TCPServer] Listening socket '%s', port = %d was not added\r\n",
      sp->name.c_str(), sp->port);
    return 0;
  }

  Log(1, " [TCPServer] Added listen socket '%s', port = %d, fd = %d\r\n",
    sp->name.c_str(), sp->port, sp->sock_fd);

  server_ports[sp->sock_fd] = move(psp);
  return 1;
}

void TCPServer::Start() {

#ifdef DATABASE_CLIENT 
  if (!db_client) {
    Log(0, " [TCPServer] No database client specified\r\n");
    return;
  }
#endif

  Multiplexer::Start();
  Log(1, " [TCPServer] Started \r\n");
}

void TCPServer::Close() {
  Multiplexer::Close();
  Log(1, " [TCPServer] Stopped \r\n");
}

bool TCPServer::getContext(int32_t fd, context_data_t &cx) {
  auto it_srv = server_ports.begin();
  for (; it_srv != server_ports.end(); it_srv++) {
    auto it_cnt = it_srv->second->connections.find(fd);
    if (it_cnt == it_srv->second->connections.end()) continue;
    cx.connection = it_cnt->second.get();
    cx.server = it_srv->second.get();
    return 1;
  }
  return 0;
}

int32_t TCPServer::getPortByName(std::string name) {
  auto it_srv = server_ports.begin();
  for (; it_srv != server_ports.end(); it_srv++) {
    if (it_srv->second->name == name)
      return it_srv->second->port;
  }
  return -1;
}




