/* Copyright (C) 2016 Alexander Shishenko <alex@shishenko.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */
#pragma once
#include "folder/RemoteFolder.h"
#include "p2p/P2PProvider.h"
#include "p2p/WSService.h"
#include "p2p/BandwidthCounter.h"
#include "util/scoped_timer.h"
#include <librevault/protocol/V1Parser.h>
#include <json/json-forwards.h>
#include <websocketpp/common/connection_hdl.hpp>

namespace librevault {

class FolderGroup;
class P2PProvider;
class WSService;

class P2PFolder : public RemoteFolder {
	friend class P2PProvider;
	friend class WSService;
public:
	/* Errors */
	struct error : public std::runtime_error {
		error(const char* what) : std::runtime_error(what){}
	};
	struct protocol_error : public error {
		protocol_error() : error("Protocol error") {}
	};
	struct auth_error : public error {
		auth_error() : error("Remote node couldn't verify its authenticity") {}
	};

	P2PFolder(P2PProvider& provider, WSService& ws_service, NodeKey& node_key, FolderService& folder_service, WSService::connection conn, io_service& ios);
	~P2PFolder();

	/* Getters */
	const blob& remote_pubkey() const {return conn_.remote_pubkey;}
	const tcp_endpoint& remote_endpoint() const {return conn_.remote_endpoint;}
	const WSService::connection::role_type role() const {return conn_.role;}
	const std::string& client_name() const {return client_name_;}
	const std::string& user_agent() const {return user_agent_;}
	std::shared_ptr<FolderGroup> folder_group() const {return std::shared_ptr<FolderGroup>(group_);}
	Json::Value collect_state();

	blob local_token();
	blob remote_token();

	/* RPC Actions */
	void send_message(const blob& message);

	// Handshake
	void perform_handshake();
	bool ready() const {return is_handshaken_;}

	/* Message senders */
	void choke();
	void unchoke();
	void interest();
	void uninterest();

	void post_have_meta(const Meta::PathRevision& revision, const bitfield_type& bitfield);
	void post_have_chunk(const blob& ct_hash);

	void request_meta(const Meta::PathRevision& revision);
	void post_meta(const SignedMeta& smeta, const bitfield_type& bitfield);
	void cancel_meta(const Meta::PathRevision& revision);

	void request_block(const blob& ct_hash, uint32_t offset, uint32_t size);
	void post_block(const blob& ct_hash, uint32_t offset, const blob& block);
	void cancel_block(const blob& ct_hash, uint32_t offset, uint32_t size);

protected:
	const WSService::connection conn_;
	std::weak_ptr<FolderGroup> group_;

	void handle_message(const blob& message);

private:
	P2PProvider& provider_;
	WSService& ws_service_;
	NodeKey& node_key_;

	V1Parser parser_;   // Protocol parser
	bool is_handshaken_ = false;

	BandwidthCounter counter_;

	// These needed primarily for UI
	std::string client_name_;
	std::string user_agent_;

	/* Ping/pong and timeout handlers */
	ScopedTimer ping_timer_, timeout_timer_;

	void bump_timeout();

	void send_ping();

	void handle_ping(std::string payload);
	void handle_pong(std::string payload);

	std::chrono::milliseconds rtt_ = std::chrono::milliseconds(0);

	/* Message handlers */
	void handle_Handshake(const blob& message_raw);

	void handle_Choke(const blob& message_raw);
	void handle_Unchoke(const blob& message_raw);
	void handle_Interested(const blob& message_raw);
	void handle_NotInterested(const blob& message_raw);

	void handle_HaveMeta(const blob& message_raw);
	void handle_HaveChunk(const blob& message_raw);

	void handle_MetaRequest(const blob& message_raw);
	void handle_MetaReply(const blob& message_raw);
	void handle_MetaCancel(const blob& message_raw);

	void handle_BlockRequest(const blob& message_raw);
	void handle_BlockReply(const blob& message_raw);
	void handle_BlockCancel(const blob& message_raw);
};

} /* namespace librevault */
