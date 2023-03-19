#include "cyclone/web.h"

namespace cyclone {
namespace web {

bool RequestHandler::handleGet(Server* server, Connection* conn) {
  if (0 == register_cb_.count(RequestMethod::GET)) {
    server_ = server;
    conn_ = conn;
    Get();
  } else {
    register_cb_[RequestMethod::GET](server, conn);
  }
  return true;
}

bool RequestHandler::handlePost(Server* server, Connection* conn) {
  if (0 == register_cb_.count(RequestMethod::POST)) {
    server_ = server;
    conn_ = conn;
    Post();
  } else {
    register_cb_[RequestMethod::POST](server, conn);
  }
  return true;
}

bool RequestHandler::handlePut(Server* server, Connection* conn) {
  if (0 == register_cb_.count(RequestMethod::PUT)) {
    server_ = server;
    conn_ = conn;
    Put();
  } else {
    register_cb_[RequestMethod::PUT](server, conn);
  }
  return true;
}

bool RequestHandler::handleDelete(Server* server, Connection* conn) {
  if (0 == register_cb_.count(RequestMethod::DELETE)) {
    server_ = server;
    conn_ = conn;
    Delete();
  } else {
    register_cb_[RequestMethod::DELETE](server, conn);
  }
  return true;
}

bool RequestHandler::handlePatch(Server* server, Connection* conn) {
  if (0 == register_cb_.count(RequestMethod::PATCH)) {
    server_ = server;
    conn_ = conn;
    Patch();
  } else {
    register_cb_[RequestMethod::PATCH](server, conn);
  }
  return true;
}

void RequestHandler::Get() { Response("get"); }

void RequestHandler::Post() { Response("post"); }

void RequestHandler::Put() { Response("put"); }

void RequestHandler::Delete() { Response("delete"); }

void RequestHandler::Patch() { Response("patch"); }

void RequestHandler::Response(const std::string& data,
                              const std::string& content_type) {
  RequestHandler::Response(server_, conn_, data, content_type);
}

void RequestHandler::Response(Server* server, Connection* conn,
                              const std::string& data,
                              const std::string& content_type) {
  mg_send_http_ok(conn, content_type.c_str(), data.size());
  mg_write(conn, data.c_str(), data.size());
}

int RequestHandler::Write(const void* data, size_t len) {
  return RequestHandler::Write(server_, conn_, data, len);
}

int RequestHandler::Write(Server* server, Connection* conn, const void* data,
                          size_t len) {
  return mg_write(conn, data, len);
}

std::string RequestHandler::GetRequestData() {
  return RequestHandler::GetRequestData(conn_);
}

std::string RequestHandler::GetRequestData(Connection* conn) {
  return Server::getPostData(conn);
}

std::string RequestHandler::GetParam(const char* key, size_t occurrence) {
  return RequestHandler::GetParam(conn_, key, occurrence);
}

std::string RequestHandler::GetParam(Connection* conn, const char* key,
                                     size_t occurrence) {
  std::string ret;
  Server::getParam(conn, key, ret, occurrence);
  return ret;
}

const RequestInfo* RequestHandler::GetRequestInfo() {
  return RequestHandler::GetRequestInfo(conn_);
}

const RequestInfo* RequestHandler::GetRequestInfo(Connection* conn) {
  return mg_get_request_info(conn);
}

std::string RequestHandler::GetCookie(const std::string& name) {
  return RequestHandler::GetCookie(conn_, name);
}

std::string RequestHandler::GetCookie(Connection* conn,
                                      const std::string& name) {
  std::string s;
  Server::getCookie(conn, name, s);
  return s;
}

std::string RequestHandler::GetMethod() {
  return RequestHandler::GetMethod(conn_);
}

std::string RequestHandler::GetMethod(Connection* conn) {
  return Server::getMethod(conn);
}

int RequestHandler::AddResoposeHeader(const std::string& header,
                                      const std::string& value) {
  return RequestHandler::AddResoposeHeader(conn_, header, value);
}

int RequestHandler::AddResoposeHeader(Connection* conn,
                                      const std::string& header,
                                      const std::string& value) {
  if (header.empty() || value.empty()) {
    return -1;
  }
  mg_response_header_start(conn, 200);
  return mg_response_header_add(conn, header.c_str(), value.c_str(),
                                value.size());
}

void RequestHandler::RegisterMethod(RequestMethod method, Callback callback) {
  register_cb_[method] = callback;
}

Application::~Application() {}

Application::Application() {}

void Application::CivetInit() { mg_init_library(MG_FEATURES_DEFAULT); }

void Application::ParseParam(const Options& options) {
  options_.clear();
  options_ = {
      "document_root",
      options.root,
      "listening_ports",
      std::to_string(options.port),
      "access_control_allow_headers",
      options.access_control_allow_headers,
      "access_control_allow_methods",
      options.access_control_allow_methods,
      "access_control_allow_origin",
      options.access_control_allow_origin,
  };
}

void Application::BuildServer() {
  server_ = std::make_shared<Server>(options_);
}

void Application::Bind(const URL& url, RequestMethod method,
                       Callback callback) {
  RequestHandler::Ptr handler = nullptr;
  if (0 == handlers_.count(url)) {
    handler = std::make_shared<RequestHandler>();
    handlers_[url] = handler;
    server_->addHandler(url, *handler);
  } else {
    handler = handlers_[url];
  }
  handler->RegisterMethod(method, callback);
}

void Application::Init(const Options& options) {
  CivetInit();
  ParseParam(options);
  BuildServer();
}

void Application::Init(Options* options) {
  CivetInit();
  ParseParam(*options);
  BuildServer();
}

void Application::Spin() {
  while (true) {
    sleep(1);
  }
}

void Application::Stop() {
  server_->close();
  mg_exit_library();
}

int Application::AddHandler(const URL& url, RequestHandler::Ptr handler) {
  std::lock_guard<std::mutex> guard(handlers_mutex_);
  if (0 != handlers_.count(url) || !handler) {
    return -1;
  }
  handlers_[url] = handler;
  server_->addHandler(url, *handler);
  return 0;
}

int Application::AddHandler(const URL& url, RequestHandler* handler) {
  std::lock_guard<std::mutex> guard(handlers_mutex_);
  if (0 != handlers_.count(url) || !handler) {
    return -1;
  }
  handlers_[url] = std::shared_ptr<RequestHandler>(handler);
  server_->addHandler(url, handler);
  return 0;
}

void Application::Get(const URL& url, Callback callback) {
  std::lock_guard<std::mutex> guard(handlers_mutex_);
  Bind(url, RequestMethod::GET, callback);
}

void Application::Post(const URL& url, Callback callback) {
  std::lock_guard<std::mutex> guard(handlers_mutex_);
  Bind(url, RequestMethod::POST, callback);
}

void Application::Put(const URL& url, Callback callback) {
  std::lock_guard<std::mutex> guard(handlers_mutex_);
  Bind(url, RequestMethod::PUT, callback);
}

void Application::Delete(const URL& url, Callback callback) {
  std::lock_guard<std::mutex> guard(handlers_mutex_);
  Bind(url, RequestMethod::DELETE, callback);
}

void Application::Patch(const URL& url, Callback callback) {
  std::lock_guard<std::mutex> guard(handlers_mutex_);
  Bind(url, RequestMethod::PATCH, callback);
}

}  // namespace web
}  // namespace cyclone
