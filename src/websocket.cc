#include "cyclone/websocket.h"

#include <mutex>

#include "cyclone/define.h"

namespace cyclone {
namespace websocket {

thread_local std::stringstream WebSocketHandler::thread_data;
thread_local OpCode WebSocketHandler::thread_opcode =
    WebSocketOpCode::CONTINUATION;

bool WebSocketHandler::handleConnection(Server* server,
                                        const Connection* conn) {
  return true;
}

void WebSocketHandler::handleReadyState(Server* server, Connection* conn) {
  {
    std::lock_guard<std::mutex> guard(mutex_);
    conns_.emplace(conn, std::make_shared<std::mutex>());
  }
  Open(server, conn);
}

bool WebSocketHandler::handleData(Server* server, Connection* conn, int bits,
                                  char* data, size_t data_len) {
  if ((bits & 0x0F) == WebSocketOpCode::CONNECTION_CLOSE) {
    return false;
  }
  thread_data.write(data, data_len);
  if (thread_opcode == WebSocketOpCode::CONTINUATION) {
    thread_opcode = bits & 0x7f;
  }
  bool is_final_fragment = bits & 0x80;
  if (is_final_fragment) {
    switch (thread_opcode) {
      case WebSocketOpCode::TEXT:
        OnMessage(server, conn, thread_data.str(), WebSocketOpCode::TEXT);
        break;
      case WebSocketOpCode::BINARY:
        OnMessage(server, conn, thread_data.str(), WebSocketOpCode::BINARY);
        break;
      default:
        std::cout << "[error] opcode does not exist!" << std::endl;
        break;
    }
    thread_opcode = WebSocketOpCode::CONTINUATION;
    thread_data.clear();
    thread_data.str(std::string());
  }
  return true;
}

void WebSocketHandler::handleClose(Server* server, const Connection* conn) {
  auto* connection = const_cast<Connection*>(conn);

  {
    std::lock_guard<std::mutex> guard(mutex_);
    if (conns_.count(connection)) {
      conns_.erase(connection);
      return;
    }
  }
  OnClose(server, conn);
}

void WebSocketHandler::Open(Server* server, const Connection* conn) {}

void WebSocketHandler::OnMessage(Server* server, Connection* conn,
                                 const std::string& msg, int op_code) {}

void WebSocketHandler::OnPong(Server* server, Connection* conn) {}

void WebSocketHandler::OnPing(Server* server, Connection* conn) {}

void WebSocketHandler::OnClose(Server* server, const Connection* conn) {}

bool WebSocketHandler::SendData(Connection* conn, const std::string& data,
                                bool skippable, unsigned char op_code) {
  std::shared_ptr<std::mutex> conn_mutex;
  {
    std::lock_guard<std::mutex> guard(mutex_);
    if (0 == conns_.count(conn)) {
      return false;
    }
    conn_mutex = conns_[conn];
  }

  if (nullptr == conn_mutex || !conn_mutex->try_lock()) {
    return false;
  }

  int ret = mg_websocket_write(conn, op_code, data.c_str(), data.size());
  conn_mutex->unlock();  // 解锁

  if (ret != static_cast<int>(data.size())) {
    // When data is empty, the header length (2) is returned.
    if (data.empty() && ret == 2) {
      return true;
    }
    {
      std::lock_guard<std::mutex> guard(mutex_);
      if (conns_.count(conn)) {
        conns_.erase(conn);
      }
    }
    return false;
  }

  return true;
}

bool WebSocketHandler::BroadcastData(const std::string& data, bool skippable,
                                     unsigned char op_code) {
  std::vector<Connection*> conns_to_send;
  if (conns_.empty()) {
    return false;
  }

  {
    // copy conn
    std::lock_guard<std::mutex> guard(mutex_);
    for (auto& kv : conns_) {
      Connection* conn = kv.first;
      conns_to_send.push_back(conn);
    }
  }

  bool all_success = true;
  for (Connection* conn : conns_to_send) {
    if (!SendData(conn, data, skippable, op_code)) {
      all_success = false;
    }
  }
  return all_success;
}

}  // namespace websocket
}  // namespace cyclone
