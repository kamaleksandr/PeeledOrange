/* 
 * File:   multiplexer.h
 * Author: kamyshev.a
 *
 * Created on 5 July 2019, 10:32
 */

#ifndef MULTIPLEXER_H
#define MULTIPLEXER_H

#include "types.h"

namespace highload {

  /**
   * Implementation of epoll multiplexer thread. 
   * Calls OnEvent, OnDisconnect and OnTimer.
   */
  class Multiplexer {
  public:
    Multiplexer();
    virtual ~Multiplexer(){}

    /**
     * Create and run epoll thread.
     */
    void Start();

    /**
     * Close epoll thread.
     */
    void Close();

    /**
     * Add a file descriptor to the epoll.
     */
    bool AddFD(int32_t fd);

    /**
     * Remove a file descriptor from the epoll.
     */
    void RemoveFD(int32_t fd);

    /**
     * On MUX event call. 
     * @param fd Socket file descriptor  
     */
    virtual void OnEvent(int32_t fd){}

    /**
     * On TCP session shutdown.
     * @param fd Socket file descriptor
     */
    virtual void OnDisconnect(int32_t fd){}

    /**
     * OnTimer call.
     */
    virtual void OnTimer(){}

  private:
    types::multiplexer_data_t md;
  };
} // namespace highload

#endif /* MULTIPLEXER_H */

