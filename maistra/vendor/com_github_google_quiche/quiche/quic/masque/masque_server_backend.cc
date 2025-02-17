// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/masque/masque_server_backend.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace quic {

MasqueServerBackend::MasqueServerBackend(MasqueMode masque_mode,
                                         const std::string& server_authority,
                                         const std::string& cache_directory)
    : masque_mode_(masque_mode), server_authority_(server_authority) {
  if (!cache_directory.empty()) {
    QuicMemoryCacheBackend::InitializeBackend(cache_directory);
  }
}

bool MasqueServerBackend::MaybeHandleMasqueRequest(
    const spdy::Http2HeaderBlock& request_headers,
    const std::string& request_body,
    QuicSimpleServerBackend::RequestHandler* request_handler) {
  auto method_pair = request_headers.find(":method");
  if (method_pair == request_headers.end()) {
    // Request is missing a method.
    return false;
  }
  absl::string_view method = method_pair->second;
  std::string masque_path = "";
  if (masque_mode_ == MasqueMode::kLegacy) {
    auto path_pair = request_headers.find(":path");
    auto scheme_pair = request_headers.find(":scheme");
    if (path_pair == request_headers.end() ||
        scheme_pair == request_headers.end()) {
      // This request is missing required headers.
      return false;
    }
    absl::string_view path = path_pair->second;
    absl::string_view scheme = scheme_pair->second;
    if (scheme != "https" || method != "POST" || request_body.empty()) {
      // MASQUE requests MUST be a non-empty https POST.
      return false;
    }

    if (path.rfind("/.well-known/masque/", 0) != 0) {
      // This request is not a MASQUE path.
      return false;
    }
    masque_path = std::string(path.substr(sizeof("/.well-known/masque/") - 1));
  } else {
    QUICHE_DCHECK_EQ(masque_mode_, MasqueMode::kOpen);
    auto protocol_pair = request_headers.find(":protocol");
    if (method != "CONNECT" || protocol_pair == request_headers.end() ||
        protocol_pair->second != "connect-udp") {
      // This is not a MASQUE request.
      return false;
    }
  }

  if (!server_authority_.empty()) {
    auto authority_pair = request_headers.find(":authority");
    if (authority_pair == request_headers.end()) {
      // Cannot enforce missing authority.
      return false;
    }
    absl::string_view authority = authority_pair->second;
    if (server_authority_ != authority) {
      // This request does not match server_authority.
      return false;
    }
  }

  auto it = backend_client_states_.find(request_handler->connection_id());
  if (it == backend_client_states_.end()) {
    QUIC_LOG(ERROR) << "Could not find backend client for "
                    << masque_path << request_headers.DebugString();
    return false;
  }

  BackendClient* backend_client = it->second.backend_client;

  std::unique_ptr<QuicBackendResponse> response =
      backend_client->HandleMasqueRequest(masque_path, request_headers,
                                          request_body, request_handler);
  if (response == nullptr) {
    QUIC_LOG(ERROR) << "Backend client did not process request for "
                    << masque_path << request_headers.DebugString();
    return false;
  }

  QUIC_DLOG(INFO) << "Sending MASQUE response for "
                  << request_headers.DebugString();

  request_handler->OnResponseBackendComplete(response.get());
  it->second.responses.emplace_back(std::move(response));

  return true;
}

void MasqueServerBackend::FetchResponseFromBackend(
    const spdy::Http2HeaderBlock& request_headers,
    const std::string& request_body,
    QuicSimpleServerBackend::RequestHandler* request_handler) {
  if (MaybeHandleMasqueRequest(request_headers, request_body,
                               request_handler)) {
    // Request was handled as a MASQUE request.
    return;
  }
  QUIC_DLOG(INFO) << "Fetching non-MASQUE response for "
                  << request_headers.DebugString();
  QuicMemoryCacheBackend::FetchResponseFromBackend(
      request_headers, request_body, request_handler);
}

void MasqueServerBackend::CloseBackendResponseStream(
    QuicSimpleServerBackend::RequestHandler* request_handler) {
  QUIC_DLOG(INFO) << "Closing response stream";
  QuicMemoryCacheBackend::CloseBackendResponseStream(request_handler);
}

void MasqueServerBackend::RegisterBackendClient(QuicConnectionId connection_id,
                                                BackendClient* backend_client) {
  QUIC_DLOG(INFO) << "Registering backend client for " << connection_id;
  QUIC_BUG_IF(quic_bug_12005_1, backend_client_states_.find(connection_id) !=
                                    backend_client_states_.end())
      << connection_id << " already in backend clients map";
  backend_client_states_[connection_id] =
      BackendClientState{backend_client, {}};
}

void MasqueServerBackend::RemoveBackendClient(QuicConnectionId connection_id) {
  QUIC_DLOG(INFO) << "Removing backend client for " << connection_id;
  backend_client_states_.erase(connection_id);
}

}  // namespace quic
