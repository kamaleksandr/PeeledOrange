/* 
 * File:   db_client.cpp
 * Author: kamyshev.a
 * 
 * Created on 12 January 2023, 15:14
 */

#include "db_client.h"
#include "common.h"

#include <climits>

#define IDLE_PING_TIME 3600

using namespace common;
using namespace database;

query_data_t::query_data_t() {
  fd = -1;
}

db_connection_t::db_connection_t() {
  fd = -1;
  status = CONNECTION_WAIT;
}

#ifdef AUX_MODE 

TAbstractDbClient::stat_data_t::stat_data_t() {
  reset();
}

void TAbstractDbClient::stat_data_t::reset() {
  timer.Start();
  max = 0;
  min = INT_MAX;
  counter = 0;
  max_queue_size = 0;
}
#endif

TAbstractDbClient::TAbstractDbClient() {
  AfterConnect = [](int32_t) {
  };
  OnResult = [](p_query_t) {
  };
  connected = 0;
}

TAbstractDbClient::~TAbstractDbClient() { }

bool TAbstractDbClient::DataProcessing(int32_t fd) {
  for (const auto &pconnection : connections) {
    db_connection_t *connection = pconnection.get();
    if (connection->fd != fd) continue;
    switch (connection->status) {
      case CONNECTION_READY:
        if (connection->query)
          CheckResult(connection);
        break;
      case CONNECTION_CONNECT:
        if (CheckConnect(connection)) connected = 1;
        break;
    }
    return 1;
  }
  return 0;
}

void TAbstractDbClient::OnTimer(uint32_t now_time) {
  for (auto &pconnection : connections) {
    db_connection_t *connection = pconnection.get();
    //if (!connection) continue;
    switch (connection->status) {
      case CONNECTION_READY:
        if (connection->query) {
          if (now_time - connection->timestamp > client.query_timeout) {

#ifdef DEBUG_MODE     
            Log(2, " [DBClient] Query timeout expired\r\n");
#endif             

            connection->query->status = RESULT_EXPIRED;
            connection->query->message = "Query timeout expired";
            connection->status = CONNECTION_ERROR;

            Result(connection);
          }
          break;
        }
        if (NextQuery(connection)) break;
        if (now_time - connection->timestamp > IDLE_PING_TIME) {

#ifdef DEBUG_MODE     
          Log(2, " [DBClient] Ping\r\n");
#endif        

          DoPing(connection);
          connection->timestamp = now_time;
        }
        break;
      case CONNECTION_WAIT:
        connection->timestamp = now_time;
        DoConnect(connection);
        if (!connected) return;
        break;
      case CONNECTION_CONNECT:
        if (CheckConnect(connection)) {
          connected = 1;
          break;
        }
        if (now_time - connection->timestamp > client.connect_timeout) {
          if (connection->query) {
            connection->query->status = database::RESULT_EXPIRED;
            connection->query->message = "Connect timeout expired";

            Result(connection);
          }
          connection->status = CONNECTION_ERROR;

#ifdef DEBUG_MODE
          Log(2, " [DBClient] Connect timeout expired.\r\n");
#endif
        }
        break;
      case CONNECTION_ERROR:
        connected = 0;
        if (now_time - connection->timestamp > client.connect_timeout) {
          connection->timestamp = now_time;
          bad_connections.push_back(std::move(pconnection));
          pconnection.reset(NewConnection());
        }
        return;
    }
  }
  std::deque<p_db_connection_t>::iterator it;
  for (it = bad_connections.begin(); it != bad_connections.end();) {
    if (now_time - (*it)->timestamp > client.connect_timeout) {
      it = bad_connections.erase(it);
      continue;
    }
    it++;
  }

#ifdef AUX_MODE
  if (!stat.timer.getSec()) return;
  if (stat.min == INT_MAX) stat.min = 0;
  std::string s =
    " queries per second " + std::to_string(stat.counter) + "\r\n"
    " query time min-max " + std::to_string(stat.min) +
    " - " + std::to_string(stat.max) + "\r\n"
    " max queue size " + std::to_string(stat.max_queue_size) + "\r\n"
    " connections count " + std::to_string(connections.size()) + "\r\n";
  Statistic("[DBClient]", s.c_str());
  stat.reset();

#endif
}

bool TAbstractDbClient::isFitInQueue(uint32_t size) {
  return queries.size() + size < client.max_queue_size;
}

bool TAbstractDbClient::Query(p_query_t pq) {
  for (const auto &pconnection : connections) {
    db_connection_t *connection = pconnection.get();
    //if (!connection) continue;
    if (connection->status != CONNECTION_READY) continue;
    if (connection->query) continue;
    connection->query.reset(pq.release());
    DoQuery(connection);
    time(&connection->timestamp);
    return 1;
  }
  if (queries.size() > client.max_queue_size) {

#ifdef DEBUG_MODE
    Log(2, " [DBClient] Excess max queries queue size\r\n");
#endif  

    return 0;
  }
  queries.push_back(std::move(pq));

#ifdef AUX_MODE  
  stat.current = queries.size();
  if (stat.max_queue_size < stat.current)
    stat.max_queue_size = stat.current;
#endif

  return 1;
}

bool TAbstractDbClient::NextQuery(db_connection_t * cd) {
  if (!queries.size()) return 0;

  //Log(3, "[DEBUG NextQuery] size %d\r\n", queries.size());

  cd->query = std::move(queries.front());
  queries.pop_front();
  DoQuery(cd);
  time(&cd->timestamp);
  return 1;
}

void TAbstractDbClient::Result(db_connection_t *cd) {
  if (cd->query) OnResult(move(cd->query)); // Process current query

// If no active query but connection is ready, 
// take next query from the queue
if (!cd->query && cd->status == CONNECTION_READY) {
    NextQuery(cd);  // Get next query from queue
}

#ifdef AUX_MODE
  stat.counter++;
  stat.current = cd->timer.getMSec();
  if (stat.max < stat.current) stat.max = stat.current;
  if (stat.min > stat.current) stat.min = stat.current;
#endif
}

db_connection_t* TAbstractDbClient::NewConnection() {
  common::Log(0, " [DBClient] !Virtual metod NewConnection is not imlemented!");
  return 0;
}

void TAbstractDbClient::DoConnect(db_connection_t*) { }

bool TAbstractDbClient::CheckConnect(db_connection_t*) {
  return 0;
}

void TAbstractDbClient::DoQuery(db_connection_t*) { }

void TAbstractDbClient::CheckResult(db_connection_t*) { }

void TAbstractDbClient::DoPing(db_connection_t*) { }

#ifdef AUX_MODE

void TAbstractDbClient::CheckAllConnect() {
  uint8_t counter = 0;
  for (const auto &connection : connections) {
    if ((*connection).status == CONNECTION_READY) counter++;
  }
  if (counter == connections.size())
    Log(1, " [DBClient] Connections (%d) successfully created.\r\n", counter);
}
#endif

