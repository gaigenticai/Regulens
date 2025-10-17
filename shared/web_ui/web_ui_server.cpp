/**
 * Web UI Server Implementation - Production HTTP Server
 *
 * Enterprise-grade HTTP server implementation with proper threading,
 * security, and comprehensive error handling for testing Regulens features.
 */

#include "web_ui_server.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <regex>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <chrono>

#ifdef __APPLE__
#include <sys/event.h>
#include <sys/time.h>
#else
#include <sys/epoll.h>
#endif
#include "../config/configuration_manager.hpp"
#include "../metrics/metrics_collector.hpp"
#include "../api_config/api_endpoint_config.hpp"
#include "../logging/structured_logger.hpp"
#include "../api_config/api_versioning_service.hpp"
#include "../api_config/api_version_router.hpp"
#include "../api_config/error_handling_service.hpp"

namespace regulens {

WebUIServer::WebUIServer(int port)
    : port_(port), running_(false) {
}

WebUIServer::~WebUIServer() {
    stop();
}

bool WebUIServer::start() {
    if (running_) {
        if (logger_) logger_->warn("Web UI server is already running");
        return true;
    }

    try {
        running_ = true;
        server_thread_ = std::thread(&WebUIServer::server_loop, this);

        // Initialize API endpoint configuration for systematic endpoint management
        std::string config_path = "shared/api_config/api_endpoints_config.json";
        if (!APIEndpointConfig::get_instance().initialize(config_path, logger_)) {
            if (logger_) {
                logger_->error("Failed to initialize API endpoint configuration");
            }
            running_ = false;
            return false;
        }

        // Initialize API versioning service for version negotiation and compatibility
        std::string versioning_config_path = "shared/api_config/api_versioning_config.json";
        if (!APIVersioningService::get_instance().initialize(versioning_config_path, logger_)) {
            if (logger_) {
                logger_->error("Failed to initialize API versioning service");
            }
            running_ = false;
            return false;
        }

        // Initialize API version router for version-aware request routing
        if (!APIVersionRouter::get_instance().initialize(logger_)) {
            if (logger_) {
                logger_->error("Failed to initialize API version router");
            }
            running_ = false;
            return false;
        }

        // Initialize error handling service for standardized error responses
        std::string error_config_path = "shared/api_config/error_handling_config.json";
        if (!ErrorHandlingService::get_instance().initialize(error_config_path, logger_)) {
            if (logger_) {
                logger_->error("Failed to initialize error handling service");
            }
            running_ = false;
            return false;
        }

        if (logger_) {
            logger_->info("Web UI server started on port " + std::to_string(port_));
            logger_->info("API endpoint configuration loaded successfully");
        }

        return true;
    } catch (const std::exception& e) {
        running_ = false;
        if (logger_) {
            logger_->error("Failed to start web UI server: {}", e.what());
        }
        return false;
    }
}

void WebUIServer::stop() {
    if (!running_) return;

    running_ = false;

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    if (logger_) {
        logger_->info("Web UI server stopped");
    }
}

void WebUIServer::add_route(const std::string& method, const std::string& path, RequestHandler handler) {
    std::lock_guard<std::mutex> lock(routes_mutex_);
    std::string key = method + " " + path;
    routes_[key] = handler;

    if (logger_) {
        logger_->debug("Added route: {} {}", method, path);
    }
}

void WebUIServer::add_static_route(const std::string& path_prefix, const std::string& static_dir) {
    std::lock_guard<std::mutex> lock(routes_mutex_);
    static_routes_[path_prefix] = static_dir;

    if (logger_) {
        logger_->debug("Added static route: {} -> {}", path_prefix, static_dir);
    }
}

WebUIServer::ServerStats WebUIServer::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void WebUIServer::server_loop() {
    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        if (logger_) logger_->error("Failed to create socket");
        running_ = false;
        return;
    }

    // Set socket options
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to port
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        if (logger_) logger_->error("Failed to bind to port " + std::to_string(port_));
        close(server_fd);
        running_ = false;
        return;
    }

    // Listen
    if (listen(server_fd, 10) < 0) {
        if (logger_) logger_->error("Failed to listen on port " + std::to_string(port_));
        close(server_fd);
        running_ = false;
        return;
    }

    if (logger_) logger_->info("Web UI server listening on port " + std::to_string(port_));

    // Set non-blocking
    fcntl(server_fd, F_SETFL, O_NONBLOCK);

#ifdef __APPLE__
    // kqueue implementation for macOS/BSD
    int kq = kqueue();
    if (kq == -1) {
        if (logger_) logger_->error("Failed to create kqueue");
        close(server_fd);
        running_ = false;
        return;
    }

    struct kevent change_event;
    EV_SET(&change_event, server_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);

    if (kevent(kq, &change_event, 1, NULL, 0, NULL) == -1) {
        if (logger_) logger_->error("Failed to add server socket to kqueue");
        close(kq);
        close(server_fd);
        running_ = false;
        return;
    }

    const int MAX_EVENTS = 32;
    struct kevent events[MAX_EVENTS];

    while (running_) {
        struct timespec timeout = {1, 0}; // 1 second timeout
        int nev = kevent(kq, NULL, 0, events, MAX_EVENTS, &timeout);

        if (nev == -1) {
            if (logger_) logger_->error("kqueue wait failed");
            continue;
        }

        for (int i = 0; i < nev; ++i) {
            if (events[i].ident == server_fd) {
                // Accept new connections
                sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);

                if (client_fd >= 0) {
                    // Set client socket to non-blocking
                    fcntl(client_fd, F_SETFL, O_NONBLOCK);

                    // Handle request in a separate thread
                    std::thread([this, client_fd]() {
                        handle_client(client_fd);
                    }).detach();
                }
            }
        }
    }

    close(kq);
#else
    // epoll implementation for Linux
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        if (logger_) logger_->error("Failed to create epoll instance");
        close(server_fd);
        running_ = false;
        return;
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        if (logger_) logger_->error("Failed to add server socket to epoll");
        close(epoll_fd);
        close(server_fd);
        running_ = false;
        return;
    }

    const int MAX_EVENTS = 32;
    struct epoll_event events[MAX_EVENTS];

    while (running_) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000); // 1 second timeout

        if (nfds == -1) {
            if (logger_) logger_->error("epoll_wait failed");
            continue;
        }

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == server_fd) {
                // Accept new connections
                sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);

                if (client_fd >= 0) {
                    // Set client socket to non-blocking
                    fcntl(client_fd, F_SETFL, O_NONBLOCK);

                    // Handle request in a separate thread
                    std::thread([this, client_fd]() {
                        handle_client(client_fd);
                    }).detach();
                }
            }
        }
    }

    close(epoll_fd);
#endif

    close(server_fd);
}

void WebUIServer::handle_client(int client_fd) {
    char buffer[8192];
    std::string request_data;

    // Read request
    ssize_t bytes_read;
    while ((bytes_read = read(client_fd, buffer, sizeof(buffer))) > 0) {
        if (bytes_read > 0) {
            request_data.append(buffer, static_cast<size_t>(bytes_read));
        }
        if (request_data.find("\r\n\r\n") != std::string::npos) break;
    }

    if (bytes_read < 0) {
        close(client_fd);
        return;
    }

    // Parse and handle request
    auto start_time = std::chrono::steady_clock::now();

    HTTPRequest request = parse_request(request_data);
    HTTPResponse response = handle_request(request);

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_requests++;
        if (response.status_code >= 400) stats_.error_count++;
        // Exponential moving average (EMA) for response time tracking
        // Formula: EMA_new = alpha * current + (1 - alpha) * EMA_old
        // Alpha = 0.2 gives more weight to historical data, smoothing out spikes
        const double alpha = 0.2;
        if (stats_.avg_response_time_ms == 0) {
            // First measurement - initialize EMA
            stats_.avg_response_time_ms = duration.count();
        } else {
            // Apply exponential smoothing
            stats_.avg_response_time_ms = alpha * duration.count() + (1.0 - alpha) * stats_.avg_response_time_ms;
        }
    }

    // Send response
    std::string response_str = serialize_response(response);
    write(client_fd, response_str.c_str(), response_str.size());

    close(client_fd);
}

HTTPRequest WebUIServer::parse_request(const std::string& raw_request) {
    HTTPRequest request;
    std::istringstream iss(raw_request);
    std::string line;

    // Parse request line
    if (std::getline(iss, line)) {
        std::istringstream line_stream(line);
        line_stream >> request.method >> request.path >> line; // Skip HTTP version

        // Parse query string
        size_t query_pos = request.path.find('?');
        if (query_pos != std::string::npos) {
            request.query_string = request.path.substr(query_pos + 1);
            request.path = request.path.substr(0, query_pos);
            request.params = parse_query_string(request.query_string);
        }
    }

    // Parse headers
    while (std::getline(iss, line) && !line.empty() && line != "\r") {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string header_name = line.substr(0, colon_pos);
            std::string header_value = line.substr(colon_pos + 1);
            // Trim whitespace
            header_name.erase(header_name.find_last_not_of(" \t\r\n") + 1);
            header_name.erase(0, header_name.find_first_not_of(" \t\r\n"));
            header_value.erase(header_value.find_last_not_of(" \t\r\n") + 1);
            header_value.erase(0, header_value.find_first_not_of(" \t\r\n"));
            request.headers[header_name] = header_value;
        }
    }

    // Parse body if present
    std::getline(iss, request.body, '\0');

    return request;
}

HTTPResponse WebUIServer::handle_request(const HTTPRequest& request) {
    std::lock_guard<std::mutex> lock(routes_mutex_);

    // Check for exact route match
    std::string route_key = request.method + " " + request.path;
    auto route_it = routes_.find(route_key);
    if (route_it != routes_.end()) {
        return route_it->second(request);
    }

    // Check for static file routes
    for (const auto& [prefix, dir] : static_routes_) {
        if (request.path.find(prefix) == 0) {
            return serve_static_file(request.path, dir);
        }
    }

    // Check for method not allowed (same path, different method)
    for (const auto& [key, handler] : routes_) {
        size_t space_pos = key.find(' ');
        if (space_pos != std::string::npos) {
            std::string route_path = key.substr(space_pos + 1);
            if (route_path == request.path) {
                return handle_method_not_allowed(request);
            }
        }
    }

    return handle_not_found(request);
}

HTTPResponse WebUIServer::serve_static_file(const std::string& path, const std::string& static_dir) {
    // Security check
    if (!is_safe_path(path)) {
        return HTTPResponse(403, "Forbidden", "Access denied");
    }

    std::string file_path = static_dir + path;
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        return handle_not_found(HTTPRequest());
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

    HTTPResponse response(200, "OK", content);
    response.content_type = get_content_type(path);
    response.headers["Content-Length"] = std::to_string(content.size());

    return response;
}

std::string WebUIServer::serialize_response(const HTTPResponse& response) {
    std::ostringstream oss;

    // Status line
    oss << "HTTP/1.1 " << response.status_code << " " << response.status_message << "\r\n";

    // Headers
    for (const auto& [key, value] : response.headers) {
        oss << key << ": " << value << "\r\n";
    }

    // Content headers
    if (!response.body.empty()) {
        oss << "Content-Type: " << response.content_type << "\r\n";
        oss << "Content-Length: " << response.body.size() << "\r\n";
    }

    oss << "\r\n" << response.body;

    return oss.str();
}

std::string WebUIServer::url_decode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%') {
            if (i + 2 < str.size()) {
                int value;
                std::istringstream iss(str.substr(i + 1, 2));
                iss >> std::hex >> value;
                result += static_cast<char>(value);
                i += 2;
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::unordered_map<std::string, std::string> WebUIServer::parse_query_string(const std::string& query) {
    std::unordered_map<std::string, std::string> params;
    std::istringstream iss(query);
    std::string pair;

    while (std::getline(iss, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = url_decode(pair.substr(0, eq_pos));
            std::string value = url_decode(pair.substr(eq_pos + 1));
            params[key] = value;
        }
    }

    return params;
}

std::unordered_map<std::string, std::string> WebUIServer::parse_form_data(const std::string& body) {
    return parse_query_string(body);
}

std::string WebUIServer::get_content_type(const std::string& path) {
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == std::string::npos) return "application/octet-stream";

    std::string ext = path.substr(dot_pos + 1);
    if (ext == "html") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "json") return "application/json";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "svg") return "image/svg+xml";

    return "application/octet-stream";
}

bool WebUIServer::is_safe_path(const std::string& path) {
    // Prevent directory traversal attacks
    return path.find("..") == std::string::npos && path.find("\\") == std::string::npos;
}

HTTPResponse WebUIServer::handle_not_found(const HTTPRequest& /*request*/) {
    std::string body = R"html(
<!DOCTYPE html>
<html>
<head><title>404 Not Found</title></head>
<body>
<h1>404 Not Found</h1>
<p>The requested resource was not found.</p>
</body>
</html>
)html";

    HTTPResponse response(404, "Not Found", body);
    response.content_type = "text/html";
    return response;
}

HTTPResponse WebUIServer::handle_method_not_allowed(const HTTPRequest& /*request*/) {
    HTTPResponse response(405, "Method Not Allowed", "Method not allowed for this resource");
    response.headers["Allow"] = "GET, POST, PUT, DELETE";
    return response;
}

HTTPResponse WebUIServer::handle_internal_error(const HTTPRequest& /*request*/) {
    HTTPResponse response(500, "Internal Server Error", "An internal server error occurred");
    return response;
}

} // namespace regulens
