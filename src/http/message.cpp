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

#include <iostream>
#include <algorithm>
#include <regex>
#include <cassert>

#include "asio.hpp"

#include "pion/tribool.hpp"
#include "pion/http/request.hpp"
#include "pion/http/parser.hpp"
#include "pion/tcp/connection.hpp"
#include "pion/http/message.hpp"

namespace pion {
namespace http {

message::receive_error_t::~receive_error_t() PION_NOEXCEPT { }

const char* message::receive_error_t::name() const PION_NOEXCEPT {
    return "receive_error_t";
}

std::string message::receive_error_t::message(int ev) const {
    std::string result;
    switch (ev) {
    case 1:
        result = "HTTP message parsing error";
        break;
    default:
        result = "Unknown receive error";
        break;
    }
    return result;
}

// static members of message

const std::regex  message::REGEX_ICASE_CHUNKED(".*chunked.*", std::regex::icase);

message::message() : 
m_is_valid(false), 
m_is_chunked(false),
m_chunks_supported(false),
m_do_not_send_content_length(false),
m_version_major(1),
m_version_minor(1),
m_content_length(0),
m_content_buf(),
m_status(STATUS_NONE),
m_has_missing_packets(false),
m_has_data_after_missing(false) { }

message::message(const message& http_msg) : 
m_first_line(http_msg.m_first_line),
m_is_valid(http_msg.m_is_valid),
m_is_chunked(http_msg.m_is_chunked),
m_chunks_supported(http_msg.m_chunks_supported),
m_do_not_send_content_length(http_msg.m_do_not_send_content_length),
m_remote_ip(http_msg.m_remote_ip),
m_version_major(http_msg.m_version_major),
m_version_minor(http_msg.m_version_minor),
m_content_length(http_msg.m_content_length),
m_content_buf(http_msg.m_content_buf),
m_chunk_cache(http_msg.m_chunk_cache),
m_headers(http_msg.m_headers),
m_status(http_msg.m_status),
m_has_missing_packets(http_msg.m_has_missing_packets),
m_has_data_after_missing(http_msg.m_has_data_after_missing) { }

message::~message() { }

message& message::operator=(const message& http_msg) {
    m_first_line = http_msg.m_first_line;
    m_is_valid = http_msg.m_is_valid;
    m_is_chunked = http_msg.m_is_chunked;
    m_chunks_supported = http_msg.m_chunks_supported;
    m_do_not_send_content_length = http_msg.m_do_not_send_content_length;
    m_remote_ip = http_msg.m_remote_ip;
    m_version_major = http_msg.m_version_major;
    m_version_minor = http_msg.m_version_minor;
    m_content_length = http_msg.m_content_length;
    m_content_buf = http_msg.m_content_buf;
    m_chunk_cache = http_msg.m_chunk_cache;
    m_headers = http_msg.m_headers;
    m_status = http_msg.m_status;
    m_has_missing_packets = http_msg.m_has_missing_packets;
    m_has_data_after_missing = http_msg.m_has_data_after_missing;
    return *this;
}

void message::clear() {
    clear_first_line();
    m_is_valid = m_is_chunked = m_chunks_supported
            = m_do_not_send_content_length = false;
    m_remote_ip = asio::ip::address_v4(0);
    m_version_major = m_version_minor = 1;
    m_content_length = 0;
    m_content_buf.clear();
    m_chunk_cache.clear();
    m_headers.clear();
    m_cookie_params.clear();
    m_status = STATUS_NONE;
    m_has_missing_packets = false;
    m_has_data_after_missing = false;
}

bool message::is_valid() const {
    return m_is_valid;
}

bool message::get_chunks_supported() const {
    return m_chunks_supported;
}

asio::ip::address& message::get_remote_ip() {
    return m_remote_ip;
}

uint16_t message::get_version_major() const {
    return m_version_major;
}

uint16_t message::get_version_minor() const {
    return m_version_minor;
}

std::string message::get_version_string() const {
    std::string http_version(STRING_HTTP_VERSION);
    http_version += pion::algorithm::to_string(get_version_major());
    http_version += '.';
    http_version += pion::algorithm::to_string(get_version_minor());
    return http_version;
}

size_t message::get_content_length() const {
    return m_content_length;
}

bool message::is_chunked() const {
    return m_is_chunked;
}

bool message::is_content_buffer_allocated() const {
    return !m_content_buf.is_empty();
}

std::size_t message::get_content_buffer_size() const {
    return m_content_buf.size();
}

char* message::get_content() {
    return m_content_buf.get();
}

const char* message::get_content() const {
    return m_content_buf.get();
}

message::chunk_cache_t& message::get_chunk_cache() {
    return m_chunk_cache;
}

const std::string& message::get_header(const std::string& key) const {
    return get_value(m_headers, key);
}

std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& 
        message::get_headers() {
    return m_headers;
}

bool message::has_header(const std::string& key) const {
    return (m_headers.find(key) != m_headers.end());
}

const std::string& message::get_cookie(const std::string& key) const {
    return get_value(m_cookie_params, key);
}

std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>& 
        message::get_cookies() {
    return m_cookie_params;
}

bool message::has_cookie(const std::string& key) const {
    return (m_cookie_params.find(key) != m_cookie_params.end());
}

void message::add_cookie(const std::string& key, const std::string& value) {
    m_cookie_params.insert(std::make_pair(key, value));
}

void message::change_cookie(const std::string& key, const std::string& value) {
    change_value(m_cookie_params, key, value);
}

void message::delete_cookie(const std::string& key) {
    delete_value(m_cookie_params, key);
}

const std::string& message::get_first_line() const {
    if (m_first_line.empty()) {
        update_first_line();
    }
    return m_first_line;
}

bool message::has_missing_packets() const {
    return m_has_missing_packets;
}

void message::set_missing_packets(bool newVal) {
    m_has_missing_packets = newVal;
}

bool message::has_data_after_missing_packets() const {
    return m_has_data_after_missing;
}

void message::set_data_after_missing_packet(bool newVal) {
    m_has_data_after_missing = newVal;
}

void message::set_is_valid(bool b) {
    m_is_valid = b;
}

void message::set_chunks_supported(bool b) {
    m_chunks_supported = b;
}

void message::set_remote_ip(const asio::ip::address& ip) {
    m_remote_ip = ip;
}

void message::set_version_major(const uint16_t n) {
    m_version_major = n;
    clear_first_line();
}

void message::set_version_minor(const uint16_t n) {
    m_version_minor = n;
    clear_first_line();
}

void message::set_content_length(size_t n) {
    m_content_length = n;
}

void message::set_do_not_send_content_length() {
    m_do_not_send_content_length = true;
}

message::data_status_t message::get_status() const {
    return m_status;
}

void message::set_status(data_status_t newVal) {
    m_status = newVal;
}

void message::update_content_length_using_header() {
    std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>
            ::const_iterator i = m_headers.find(HEADER_CONTENT_LENGTH);
    if (i == m_headers.end()) {
        m_content_length = 0;
    } else {
        std::string trimmed_length(i->second);
        pion::algorithm::trim(trimmed_length);
        m_content_length = pion::algorithm::parse_sizet(trimmed_length);
    }
}

void message::update_transfer_encoding_using_header() {
    m_is_chunked = false;
    std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>
            ::const_iterator i = m_headers.find(HEADER_TRANSFER_ENCODING);
    if (i != m_headers.end()) {
        // From RFC 2616, sec 3.6: All transfer-coding values are case-insensitive.
        m_is_chunked = std::regex_match(i->second, REGEX_ICASE_CHUNKED);
        // ignoring other possible values for now
    }
}

char* message::create_content_buffer() {
    m_content_buf.resize(m_content_length);
    return m_content_buf.get();
}

void message::set_content(const std::string& content) {
    set_content_length(content.size());
    create_content_buffer();
    memcpy(m_content_buf.get(), content.c_str(), content.size());
}

void message::clear_content() {
    set_content_length(0);
    create_content_buffer();
    delete_value(m_headers, HEADER_CONTENT_TYPE);
}

void message::set_content_type(const std::string& type) {
    change_value(m_headers, HEADER_CONTENT_TYPE, type);
}

void message::add_header(const std::string& key, const std::string& value) {
    m_headers.insert(std::make_pair(key, value));
}

void message::change_header(const std::string& key, const std::string& value) {
    change_value(m_headers, key, value);
}

void message::delete_header(const std::string& key) {
    delete_value(m_headers, key);
}

bool message::check_keep_alive() const {
    return (get_header(HEADER_CONNECTION) != "close"
            && (get_version_major() > 1
            || (get_version_major() >= 1 && get_version_minor() >= 1)));
}

void message::prepare_buffers_for_send(write_buffers_t& write_buffers, const bool keep_alive,
        const bool using_chunks) {
    // update message headers
    prepare_headers_for_send(keep_alive, using_chunks);
    // add first message line
    write_buffers.push_back(asio::buffer(get_first_line()));
    write_buffers.push_back(asio::buffer(STRING_CRLF));
    // append cookie headers (if any)
    append_cookie_headers();
    // append HTTP headers
    append_headers(write_buffers);
}


// message member functions

std::size_t message::send(tcp::connection& tcp_conn,
                          asio::error_code& ec, bool headers_only) {
    // initialize write buffers for send operation using HTTP headers
    write_buffers_t write_buffers;
    prepare_buffers_for_send(write_buffers, tcp_conn.get_keep_alive(), false);

    // append payload content to write buffers (if there is any)
    if (!headers_only && get_content_length() > 0 && get_content() != NULL)
        write_buffers.push_back(asio::buffer(get_content(), get_content_length()));

    // send the message and return the result
    return tcp_conn.write(write_buffers, ec);
}

std::size_t message::receive(tcp::connection& tcp_conn,
                             asio::error_code& ec,
                             parser& http_parser) {
    std::size_t last_bytes_read = 0;

    // make sure that we start out with an empty message
    clear();

    if (tcp_conn.get_pipelined()) {
        // there are pipelined messages available in the connection's read buffer
        const char *read_ptr;
        const char *read_end_ptr;
        tcp_conn.load_read_pos(read_ptr, read_end_ptr);
        last_bytes_read = (read_end_ptr - read_ptr);
        http_parser.set_read_buffer(read_ptr, last_bytes_read);
    } else {
        // read buffer is empty (not pipelined) -> read some bytes from the connection
        last_bytes_read = tcp_conn.read_some(ec);
        if (ec) return 0;
        assert(last_bytes_read > 0);
        http_parser.set_read_buffer(tcp_conn.get_read_buffer().data(), last_bytes_read);
    }

    // incrementally read and parse bytes from the connection
    bool force_connection_closed = false;
    pion::tribool parse_result;
    for (;;) {
        // parse bytes available in the read buffer
        parse_result = http_parser.parse(*this, ec);
        if (! pion::indeterminate(parse_result)) break;

        // read more bytes from the connection
        last_bytes_read = tcp_conn.read_some(ec);
        if (ec || last_bytes_read == 0) {
            if (http_parser.check_premature_eof(*this)) {
                // premature EOF encountered
                if (! ec)
                    ec = make_error_code(asio::error::misc_errors::eof);
                return http_parser.get_total_bytes_read();
            } else {
                // EOF reached when content length unknown
                // assume it is the correct end of content
                // and everything is OK
                force_connection_closed = true;
                parse_result = true;
                ec.clear();
                break;
            }
            break;
        }

        // update the HTTP parser's read buffer
        http_parser.set_read_buffer(tcp_conn.get_read_buffer().data(), last_bytes_read);
    }
    
    if (parse_result == false) {
        // an error occurred while parsing the message headers
        return http_parser.get_total_bytes_read();
    }

    // set the connection's lifecycle type
    if (!force_connection_closed && check_keep_alive()) {
        if ( http_parser.eof() ) {
            // the connection should be kept alive, but does not have pipelined messages
            tcp_conn.set_lifecycle(tcp::connection::LIFECYCLE_KEEPALIVE);
        } else {
            // the connection has pipelined messages
            tcp_conn.set_lifecycle(tcp::connection::LIFECYCLE_PIPELINED);
            
            // save the read position as a bookmark so that it can be retrieved
            // by a new HTTP parser, which will be created after the current
            // message has been handled
            const char *read_ptr;
            const char *read_end_ptr;
            http_parser.load_read_pos(read_ptr, read_end_ptr);
            tcp_conn.save_read_pos(read_ptr, read_end_ptr);
        }
    } else {
        // default to close the connection
        tcp_conn.set_lifecycle(tcp::connection::LIFECYCLE_CLOSE);
        
        // save the read position as a bookmark so that it can be retrieved
        // by a new HTTP parser
        if (http_parser.get_parse_headers_only()) {
            const char *read_ptr;
            const char *read_end_ptr;
            http_parser.load_read_pos(read_ptr, read_end_ptr);
            tcp_conn.save_read_pos(read_ptr, read_end_ptr);
        }
    }

    return (http_parser.get_total_bytes_read());
}

std::size_t message::receive(tcp::connection& tcp_conn,
                             asio::error_code& ec,
                             bool headers_only,
                             std::size_t max_content_length) {
    http::parser http_parser(dynamic_cast<http::request*>(this) != NULL);
    http_parser.parse_headers_only(headers_only);
    http_parser.set_max_content_length(max_content_length);
    return receive(tcp_conn, ec, http_parser);
}

std::size_t message::write(std::ostream& out,
    asio::error_code& ec, bool headers_only) {
    // reset error_code
    ec.clear();

    // initialize write buffers for send operation using HTTP headers
    write_buffers_t write_buffers;
    prepare_buffers_for_send(write_buffers, true, false);

    // append payload content to write buffers (if there is any)
    if (!headers_only && get_content_length() > 0 && get_content() != NULL)
        write_buffers.push_back(asio::buffer(get_content(), get_content_length()));

    // write message to the output stream
    std::size_t bytes_out = 0;
    for (write_buffers_t::const_iterator i=write_buffers.begin(); i!=write_buffers.end(); ++i) {
        const char *ptr = asio::buffer_cast<const char*>(*i);
        size_t len = asio::buffer_size(*i);
        out.write(ptr, len);
        bytes_out += len;
    }

    return bytes_out;
}

std::size_t message::read(std::istream& in,
                          asio::error_code& ec,
                          parser& http_parser) {
    // make sure that we start out with an empty message & clear error_code
    clear();
    ec.clear();
    
    // parse data from file one byte at a time
    pion::tribool parse_result;
    char c;
    while (in) {
        in.read(&c, 1);
        if ( ! in ) {
            ec = make_error_code(asio::error::misc_errors::eof);
            break;
        }
        http_parser.set_read_buffer(&c, 1);
        parse_result = http_parser.parse(*this, ec);
        if (! pion::indeterminate(parse_result)) break;
    }

    if (pion::indeterminate(parse_result)) {
        if (http_parser.check_premature_eof(*this)) {
            // premature EOF encountered
            if (! ec)
                ec = make_error_code(asio::error::misc_errors::eof);
        } else {
            // EOF reached when content length unknown
            // assume it is the correct end of content
            // and everything is OK
            parse_result = true;
            ec.clear();
        }
    }
    
    return (http_parser.get_total_bytes_read());
}

std::size_t message::read(std::istream& in,
                          asio::error_code& ec,
                          bool headers_only,
                          std::size_t max_content_length) {
    http::parser http_parser(dynamic_cast<http::request*>(this) != NULL);
    http_parser.parse_headers_only(headers_only);
    http_parser.set_max_content_length(max_content_length);
    return read(in, ec, http_parser);
}

void message::concatenate_chunks(void) {
    set_content_length(m_chunk_cache.size());
    char *post_buffer = create_content_buffer();
    if (m_chunk_cache.size() > 0)
        std::copy(m_chunk_cache.begin(), m_chunk_cache.end(), post_buffer);
}

message::content_buffer_t::~content_buffer_t() { }

message::content_buffer_t::content_buffer_t() : 
m_buf(), 
m_len(0),
m_empty(0),
m_ptr(&m_empty) { }

message::content_buffer_t::content_buffer_t(const content_buffer_t& buf) : 
m_buf(), 
m_len(0),
m_empty(0),
m_ptr(&m_empty) {
    if (buf.size()) {
        resize(buf.size());
        memcpy(get(), buf.get(), buf.size());
    }
}

message::content_buffer_t& message::content_buffer_t::operator=(const content_buffer_t& buf) {
    if (buf.size()) {
        resize(buf.size());
        memcpy(get(), buf.get(), buf.size());
    } else {
        clear();
    }
    return *this;
}

bool message::content_buffer_t::is_empty() const {
    return m_len == 0;
}

std::size_t message::content_buffer_t::size() const {
    return m_len;
}

const char* message::content_buffer_t::get() const {
    return m_ptr;
}

char* message::content_buffer_t::get() {
    return m_ptr;
}

void message::content_buffer_t::resize(std::size_t len) {
    m_len = len;
    if (len == 0) {
        m_buf.reset();
        m_ptr = &m_empty;
    } else {
        m_buf.reset(new char[len + 1]);        
        m_buf[len] = '\0';
        m_ptr = m_buf.get();
    }
}

void message::content_buffer_t::clear() {
    resize(0);
}

void message::prepare_headers_for_send(const bool keep_alive, const bool using_chunks) {
    change_header(HEADER_CONNECTION, (keep_alive ? "Keep-Alive" : "close"));
    if (using_chunks) {
        if (get_chunks_supported()) {
            change_header(HEADER_TRANSFER_ENCODING, "chunked");
        }
    } else if (!m_do_not_send_content_length) {
        change_header(HEADER_CONTENT_LENGTH, pion::algorithm::to_string(get_content_length()));
    }
}

void message::append_headers(write_buffers_t& write_buffers) {
    // add HTTP headers
    for (std::unordered_multimap<std::string, std::string, algorithm::ihash, algorithm::iequal_to>
            ::const_iterator i = m_headers.begin(); i != m_headers.end(); ++i) {
        write_buffers.push_back(asio::buffer(i->first));
        write_buffers.push_back(asio::buffer(HEADER_NAME_VALUE_DELIMITER));
        write_buffers.push_back(asio::buffer(i->second));
        write_buffers.push_back(asio::buffer(STRING_CRLF));
    }
    // add an extra CRLF to end HTTP headers
    write_buffers.push_back(asio::buffer(STRING_CRLF));
}

void message::append_cookie_headers() { }

void message::clear_first_line() const {
    if (!m_first_line.empty()) {
        m_first_line.clear();
    }
}


} // end namespace http
} // end namespace pion
