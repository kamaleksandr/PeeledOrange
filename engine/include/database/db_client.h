/* 
 * File:   db_client.h
 * Author: kamyshev.a
 *
 * Created on 12 January 2023, 15:14
 */

#ifndef DB_CLIENT_H
#define DB_CLIENT_H

#include "string"
#include <memory>
#include <deque>

#include "common.h"

namespace database {
  
  struct client_set_t {
    std::string host;
    uint32_t port;
    std::string user;
    std::string pwd;
    std::string schema;
    std::string app_name;
    uint32_t conns_count;
    uint32_t max_queue_size;
    uint32_t connect_timeout;
    uint32_t query_timeout;
  };

  enum result_status_t {
    RESULT_OK,
    RESULT_ERROR,
    RESULT_EXPIRED
  };

  struct query_data_t {
    query_data_t();
    virtual ~query_data_t() = default;
    int32_t fd;
    result_status_t status;
    std::string message;
  };

  typedef std::unique_ptr<query_data_t> p_query_t;

  enum connection_status_t {
    CONNECTION_WAIT,
    CONNECTION_CONNECT,
    CONNECTION_READY,
    CONNECTION_ERROR
  };

  struct db_connection_t {
    db_connection_t();
    int32_t fd;
    time_t timestamp;
    connection_status_t status;
    p_query_t query;

#ifdef AUX_MODE
    common::TTimer timer;
#endif  
  };

  typedef std::unique_ptr<db_connection_t> p_db_connection_t;

  /** 
   * An abstract database client that requires inheritance.
   * The inheritor must have an implementation of the functions: 
   * NewConnection(), Disconnect(), CheckConnect(), 
   * DoQuery(), CheckResult(), DoPing().    
   * Maintains database connections and data exchange.
   * Calls AfterConnect and OnResult. 
   * Provides Query function.
   * There is a need to call DataProcessing function. 
   */
  class TAbstractDbClient {
  public:

    TAbstractDbClient();
    virtual ~TAbstractDbClient();

    /**
     * After connect callback.
     * @param fd Socket file descriptor 
     */
    void (*AfterConnect)(int32_t fd);

    /**
     * Callback of the database query result.
     * @param cd Reference to connection data structure
     */
    void (*OnResult)(p_query_t pq);

    /**
     * Data processing, call on file descriptor events.
     * @param fd File descriptor
     * @return True if the file descriptor in operation 
     */
    virtual bool DataProcessing(int32_t fd);

    /**
     * Timer for maintenance database connection.
     * @param now Current time
     */
    virtual void OnTimer(uint32_t now);
    
    /**
     * Checking the available space in the request queue.
     * @param size To check for size matching
     */
    bool isFitInQueue(uint32_t size);

    /**
     * Request to the database.
     * @param query Query data structure
     * @return True if request accepted successfully
     */
    bool Query(p_query_t query);

  protected:

    client_set_t client;

    std::deque<p_db_connection_t> connections;
    std::deque<p_query_t> queries;
    
    /**
     * Create custom inheritor for the abstract connection data structure.
     * @return Reference to connection data structure
     */
    virtual db_connection_t* NewConnection();
    
    /**
     * Implementing a custom database connect
     * @param Reference to connection data structure
     */
    virtual void DoConnect(db_connection_t*);
    
    /**
     * Check the connect to the database
     * @param connection data structure
     * @return 
     */
    virtual bool CheckConnect(db_connection_t*);
    
    /**
     * Implementing the custom query
     * @param connection data structure
     */
    virtual void DoQuery(db_connection_t*);
    
    /**
     * Check result of query. Must contain a call Result().
     * @param connection data structure
     */
    virtual void CheckResult(db_connection_t*);
    
    /**
     * Implementing a custom ping request.
     * @param connection data structure
     */
    virtual void DoPing(db_connection_t*);
    
    /**
     * Processing the results of the request, calling the next query.
     * @param connection data structure
     */
    void Result(db_connection_t*);

#ifdef AUX_MODE    
    /**
     * Сhecks all connections for log output.
     */
    void CheckAllConnect();
#endif

  private:
    std::deque<p_db_connection_t> bad_connections;
    bool connected;

    bool NextQuery(db_connection_t*);

#ifdef AUX_MODE

    struct stat_data_t {
      stat_data_t();
      void reset();
      common::TTimer timer;
      uint32_t current;
      uint32_t max;
      uint32_t min;
      uint32_t counter;
      uint32_t max_queue_size;
    } stat;
#endif
    
};
} // database
#endif /* DB_CLIENT_H */

