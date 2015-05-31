/*
 * Copyright 2015, alex at staticlibs.net
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// ---------------------------------------------------------------------
// pion:  a Boost C++ framework for building lightweight HTTP interfaces
// ---------------------------------------------------------------------
// Copyright (C) 2007-2014 Splunk Inc.  (https://github.com/splunk/pion)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_HTTP_WRITER_HEADER__
#define __PION_HTTP_WRITER_HEADER__

#include <vector>
#include <list>
#include <string>
#include <memory>
#include <functional>

#include "asio.hpp"

#include "pion/noncopyable.hpp"
#include "pion/config.hpp"
#include "pion/logger.hpp"
#include "pion/tcp/connection.hpp"
#include "pion/http/message.hpp"

namespace pion {
namespace http {

/**
 * Sends HTTP data asynchronously
 */
class PION_API writer : private pion::noncopyable {
    
protected:
    
    /**
     * Function called after the HTTP message has been sent
     */
    typedef std::function<void(const asio::error_code&)> finished_handler_t;

    /**
     * Data type for a function that handles write operations
     */
    typedef std::function<void(const asio::error_code&, std::size_t)> write_handler_t;    
    
private:    

    /**
     * Used to cache binary data included within the payload content
     */
    class binary_cache_t : public std::vector<std::pair<const char *, size_t>> {
        public:
        /**
         * Destructor
         */
        ~binary_cache_t();

        /**
         * Add data to cache
         * 
         * @param ptr data
         * @param size data length in bytes
         * @return asio buffer pointing to copy of passed data
         */
        asio::const_buffer add(const void *ptr, const size_t size);
    };

    /**
     * Primary logging interface used by this class
     */
    logger m_logger;

    /**
     * The HTTP connection that we are writing the message to
     */
    tcp::connection_ptr m_tcp_conn;

    /**
     * I/O write buffers that wrap the payload content to be written
     */
    http::message::write_buffers_t m_content_buffers;

    /**
     * Caches binary data included within the payload content
     */
    binary_cache_t m_binary_cache;

    /**
     * Caches text (non-binary) data included within the payload content
     */
    std::list<std::string> m_text_cache;

    /**
     * Caches strings moved into writer and included within the payload content
     */
    std::vector<std::string> m_moved_cache;

    /**
     * Incrementally creates strings of text data for the text_cache
     */
    std::ostringstream m_content_stream;

    /**
     * The length (in bytes) of the response content to be sent (Content-Length)
     */
    size_t m_content_length;

    /**
     * True if the content_stream is empty (avoids unnecessary string copies)
     */
    bool m_stream_is_empty;

    /**
     * True if the HTTP client supports chunked transfer encodings
     */
    bool m_client_supports_chunks;

    /**
     * True if data is being sent to the client using multiple chunks
     */
    bool m_sending_chunks;

    /**
     * True if the HTTP message headers have already been sent
     */
    bool m_sent_headers;

    /**
     * Function called after the HTTP message has been sent
     */
    finished_handler_t m_finished;
    
protected:
    
    /**
     * Protected constructor: only derived classes may create objects
     * 
     * @param tcp_conn TCP connection used to send the message
     * @param handler function called after the request has been sent
     */
    writer(tcp::connection_ptr& tcp_conn, finished_handler_t handler);
    
    /**
     * Called after the message is sent
     * 
     * @param write_error error status from the last write operation
     * @param bytes_written number of bytes sent by the last write operation
     */
    virtual void handle_write(const asio::error_code& write_error, std::size_t bytes_written) = 0;
    
    /**
     * Initializes a vector of write buffers with the HTTP message information
     *
     * @param write_buffers vector of write buffers to initialize
     */
    virtual void prepare_buffers_for_send(http::message::write_buffers_t& write_buffers) = 0;
                                      
    /**
     * Returns a function bound to writer::handle_write()
     * 
     * @return function bound to writer::handle_write()
     */
    virtual write_handler_t bind_to_write_handler() = 0;
    
    /**
     * called after we have finished sending the HTTP message
     * 
     * @param ec
     */
    void finished_writing(const asio::error_code& ec);
    
public:

    /**
     * Default destructor
     */
    virtual ~writer();

    /**
     * Clears out all of the memory buffers used to cache payload content data
     */
    void clear();

    /**
     * Write text (non-binary) payload content
     *
     * @param data the data to append to the payload content
     */
    template <typename T> void write(const T& data) {
        m_content_stream << data;
        if (m_stream_is_empty) m_stream_is_empty = false;
    }

    /**
     * Write using manipulator
     * 
     * @param iomanip manipulator
     */
    void write(std::ostream& (*iomanip)(std::ostream&));

    /**
     * Write binary payload content
     *
     * @param data points to the binary data to append to the payload content
     * @param length the length, in bytes, of the binary data
     */
    void write(const void *data, size_t length);
    
    /**
     * Write text (non-binary) payload content; the data written is not
     * copied, and therefore must persist until the message has finished
     * sending
     *
     * @param data the data to append to the payload content
     */
    void write_no_copy(const std::string& data);
    
    /**
     * Write binary payload content;  the data written is not copied, and
     * therefore must persist until the message has finished sending
     *
     * @param data points to the binary data to append to the payload content
     * @param length the length, in bytes, of the binary data
     */
    void write_no_copy(void *data, size_t length);

    /**
     * Write binary payload content using r-value string
     *
     * @param data points to the binary data to append to the payload content
     * @param length the length, in bytes, of the binary data
     */
    void write_move(std::string&& data);
    
    /**
     * Sends all data buffered as a single HTTP message (without chunking).
     * Following a call to this function, it is not thread safe to use your
     * reference to the writer object.
     */
    void send();
    
    /**
     * Sends all data buffered as a single HTTP message (without chunking).
     * Following a call to this function, it is not thread safe to use your
     * reference to the writer object until the send_handler has been called.
     *
     * @param send_handler function that is called after the message has been
     *                     sent to the client.  Your callback function must end
     *                     the connection by calling connection::finish().
     */
    template <typename SendHandler> void send(SendHandler send_handler) {
        send_more_data(false, send_handler);
    }
    
    /**
     * Sends all data buffered as a single HTTP chunk.  Following a call to this
     * function, it is not thread safe to use your reference to the writer
     * object until the send_handler has been called.
     * 
     * @param send_handler function that is called after the chunk has been sent
     *                     to the client.  Your callback function must end by
     *                     calling one of send_chunk() or send_final_chunk().  Also,
     *                     be sure to clear() the writer before writing data to it.
     */
    template <typename SendHandler> void send_chunk(SendHandler send_handler) {
        m_sending_chunks = true;
        if (!supports_chunked_messages()) {
            // sending data in chunks, but the client does not support chunking;
            // make sure that the connection will be closed when we are all done
            m_tcp_conn->set_lifecycle(tcp::connection::LIFECYCLE_CLOSE);
        }
        // send more data
        send_more_data(false, send_handler);
    }

    /**
     * Sends all data buffered (if any) and also sends the final HTTP chunk.
     * This function (either overloaded version) must be called following any 
     * calls to send_chunk().
     * Following a call to this function, it is not thread safe to use your
     * reference to the writer object until the send_handler has been called.
     *
     * @param send_handler function that is called after the message has been
     *                     sent to the client.  Your callback function must end
     *                     the connection by calling connection::finish().
     */ 
    template <typename SendHandler> void send_final_chunk(SendHandler send_handler) {
        m_sending_chunks = true;
        send_more_data(true, send_handler);
    }
    
    /**
     * Sends all data buffered (if any) and also sends the final HTTP chunk.
     * This function (either overloaded version) must be called following any 
     * calls to send_chunk().
     * Following a call to this function, it is not thread safe to use your
     * reference to the writer object.
     */ 
    void send_final_chunk();
    
    /**
     * Returns a shared pointer to the TCP connection
     * 
     * @return shared pointer to the TCP connection
     */
    tcp::connection_ptr& get_connection();

    /**
     * Returns the length of the payload content (in bytes)
     * 
     * @return length of the payload content (in bytes)
     */
    size_t get_content_length() const;

    /**
     * Sets whether or not the client supports chunked messages
     * 
     * @param b whether or not the client supports chunked messages
     */
    void supports_chunked_messages(bool b);
    
    /**
     * Returns true if the client supports chunked messages
     * 
     * @return true if the client supports chunked messages
     */
    bool supports_chunked_messages() const;

    /**
     * Returns true if we are sending a chunked message to the client
     * 
     * @return true if we are sending a chunked message to the client
     */
    bool sending_chunked_message() const;
    
    /**
     * Sets the logger to be used
     * 
     * @param log_ptr logger to be used
     */
    void set_logger(logger log_ptr);
    
    /**
     * Returns the logger currently in use
     * 
     * @return logger currently in use
     */
    logger get_logger();

private:

    /**
     * Sends all of the buffered data to the client
     *
     * @param send_final_chunk true if the final 0-byte chunk should be included
     * @param send_handler function called after the data has been sent
     */
    template <typename SendHandler>
    void send_more_data(const bool send_final_chunk, SendHandler send_handler) {
        // make sure that we did not lose the TCP connection
        if (m_tcp_conn->is_open()) {
            // make sure that the content-length is up-to-date
            flush_content_stream();
            // prepare the write buffers to be sent
            http::message::write_buffers_t write_buffers;
            prepare_write_buffers(write_buffers, send_final_chunk);
            // send data in the write buffers
            m_tcp_conn->async_write(write_buffers, send_handler);
        } else {
            finished_writing(asio::error::connection_reset);
        }
    }
    
    /**
     * Prepares write_buffers for next send operation
     *
     * @param write_buffers buffers to which data will be appended
     * @param send_final_chunk true if the final 0-byte chunk should be included
     */
    void prepare_write_buffers(http::message::write_buffers_t &write_buffers, 
            const bool send_final_chunk);
    
    /**
     * Flushes any text data in the content stream after caching it in the text_cache_t
     */
    void flush_content_stream();
    
};


/**
 * Data type for a writer pointer
 */
typedef std::shared_ptr<writer> writer_ptr;


/**
 * Override operator shift for convenience
 * 
 * @param writer pointer to writer
 * @param data data to write
 * @return pointer to passed writer
 */

template <typename T>
inline const writer_ptr& operator<<(const writer_ptr& writer, const T& data) {
    writer->write(data);
    return writer;
}

/**
 * Override operator shift for convenience
 * 
 * @param writer pointer to writer
 * @param iomanip manipulator to write
 * @return pointer to passed writer
 */
inline const writer_ptr& operator<<(const writer_ptr& writer, std::ostream& (*iomanip)(std::ostream&)) {
    writer->write(iomanip);
    return writer;
}

}   // end namespace http
}   // end namespace pion

#endif
