/* 
 * File:   tcp_server.h
 * Author: kamyshev.a
 *
 * Created on 30 July 2019, 16:02
 */

#ifndef HL_TCP_SERVER_H
#define HL_TCP_SERVER_H

#include <memory.h>

#include "multiplexer.h"
#include "../database/db_client.h"

namespace highload {

  /**
   * TCP server, inheritor TMultiplexer.
   * Maintains an up-to-date list of connections data.
   * Calls OnConnect, OnReadData, provides SendData function;
   */
  class TCPServer : protected Multiplexer {
  public:
    TCPServer();
    virtual ~TCPServer();

    /**
     * Read configuration and prepare listening sockets.
     * @param name Configuration filename
     * @return True on success
     */
    bool Open(const char*);

    /**Start listening*/
    void Start();

    /**
     * After TCP session is established call.
     * @param context Context data structure
     */
    virtual void OnConnect(types::context_data_t &context) {
    }

    /**
     * On read data call.
     * @param context Context data structure 
     */
    virtual void OnReadData(types::context_data_t context) {
    }

    /**
     * After TCP session is closed.
     * @param context Context data structure 
     */
    virtual void OnDisconnect(types::context_data_t &context){}

    /**
     * One second timer callback
     */
    void (*OnSecTimer)();

    /**
     * Close multiplexer
     */
    void Close();

    /**
     * Sending binary data to the client.
     * @param cd Connection data structure
     * @param data Pointer to a data array 
     * @param size Size of data array 
     */
    int32_t SendData(types::connection_data_t *cd, const void *data, uint32_t size);

    /**
     * Search in list of connections data. 
     * @param sock_fd Socket file descriptor
     * @return Pointer to connection data or 0 
     */
    types::connection_data_t *GetConnection(int32_t sock_fd);
    
    /**
     * Search in list of server ports.
     * @param sock_fd Socket file descriptor
     * @param Reference to result
     * @return True if success   
     */
    bool getContext(int32_t sock_fd, types::context_data_t &cx);
    
    /**
     * Search port number by name.
     * @param name Port name
     * @return Port number or -1
     */
    int32_t getPortByName(std::string name);
    
    /**
     * Getter for now_time variable
     * @return server unixtime
     */
    uint32_t getTime(){ return now_time; }

#ifdef DATABASE_CLIENT
    database::TAbstractDbClient *db_client;
#endif  

  private:
    uint32_t now_time, current_time;
    time_t operating_time;
    types::server_ports_map server_ports;
    /*struct context_it_t{
      types::server_ports_map::iterator port;
      types::connections_map::iterator connection;
    };*/
    int32_t last_checked_fd;
    uint32_t required_capacity;
    uint8_t *buf;

#ifdef TLS_MODE
    std::unique_ptr<openssl::TOpenSSL> ssl;
    std::string ssl_dir_path;
#endif 

    void OnEvent(int32_t fd) override final;
    void OnDisconnect(int32_t fd) override final;
    void OnTimer() override final;
    bool AddListenSocket(types::p_server_port_t);
    void ConnectionProcessing(types::context_data_t&);
    int32_t ReceiveData(types::connection_data_t*);
    void EraseExpiredConnections();
    void Disconnect(types::context_data_t&);
  };
} // namespace highload
#endif // HL_TCP_SERVER_H

