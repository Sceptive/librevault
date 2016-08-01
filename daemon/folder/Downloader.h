/* Copyright (C) 2015 Alexander Shishenko <GamePad64@gmail.com>
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
 */
#pragma once
#include <util/file_util.h>
#include <util/periodic_process.h>
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include "RemoteFolder.h"
#include "util/AvailabilityMap.h"

#define CLUSTERED_COEFFICIENT 10.0f
#define IMMEDIATE_COEFFICIENT 20.0f
#define RARITY_COEFFICIENT 25.0f

#ifndef FOPEN_BACKEND
#   include <boost/iostreams/device/mapped_file.hpp>
#endif

namespace librevault {

class Client;
class FolderGroup;

/* MissingChunk constructs a chunk in a file. If complete(), then an encrypted chunk is located in  */
struct MissingChunk {
	MissingChunk(const fs::path& system_path, blob ct_hash, uint32_t size);

	// File-related accessors
	fs::path release_chunk();

	// Content-related accessors
	void put_block(uint32_t offset, const blob& content);

	// Size-related functions
	uint64_t size() const {return file_map_.size_original();}
	bool complete() const {return file_map_.full();}

	// AvailabilityMap accessors
	AvailabilityMap<uint32_t>::const_iterator begin() {return file_map_.begin();}
	AvailabilityMap<uint32_t>::const_iterator end() {return file_map_.end();}
	const AvailabilityMap<uint32_t>& file_map() const {return file_map_;}

	/* Request-oriented functions */
	struct BlockRequest {
		uint32_t offset;
		uint32_t size;
		std::chrono::steady_clock::time_point started;
	};
	std::unordered_multimap<std::shared_ptr<RemoteFolder>, BlockRequest> requests;
	std::unordered_map<std::shared_ptr<RemoteFolder>, std::shared_ptr<RemoteFolder::InterestGuard>> owned_by;

	const blob ct_hash_;

private:
	AvailabilityMap<uint32_t> file_map_;
	fs::path this_chunk_path_;
#ifndef FOPEN_BACKEND
	boost::iostreams::mapped_file mapped_file_;
#else
	file_wrapper wrapped_file_;
#endif
};

class WeightedDownloadQueue {
	struct Weight {
		bool clustered = false;
		bool immediate = false;

		size_t owned_by = 0;
		size_t remotes_count = 0;

		float value() const;
		bool operator<(const Weight& b) const {return value() > b.value();}
		bool operator==(const Weight& b) const {return value() == b.value();}
		bool operator!=(const Weight& b) const {return !(*this == b);}
	};
	using weight_ordered_chunks_t = boost::bimap<
		boost::bimaps::unordered_set_of<std::shared_ptr<MissingChunk>>,
		boost::bimaps::multiset_of<Weight>
	>;
	using queue_left_value = weight_ordered_chunks_t::left_value_type;
	using queue_right_value = weight_ordered_chunks_t::right_value_type;
	weight_ordered_chunks_t weight_ordered_chunks_;

	Weight get_current_weight(std::shared_ptr<MissingChunk> chunk);
	void reweight_chunk(std::shared_ptr<MissingChunk> chunk, Weight new_weight);

public:
	void add_chunk(std::shared_ptr<MissingChunk> chunk);
	void remove_chunk(std::shared_ptr<MissingChunk> chunk);

	void set_overall_remotes_count(size_t count);
	void set_chunk_remotes_count(std::shared_ptr<MissingChunk> chunk, size_t count);

	void mark_clustered(std::shared_ptr<MissingChunk> chunk);
	void mark_immediate(std::shared_ptr<MissingChunk> chunk);

	std::list<std::shared_ptr<MissingChunk>> chunks() const;
};

class Downloader : public std::enable_shared_from_this<Downloader>, protected Loggable {
public:
	Downloader(Client& client, FolderGroup& exchange_group);
	~Downloader();

	void notify_local_meta(const Meta::PathRevision& revision, const bitfield_type& bitfield);
	void notify_local_chunk(const blob& ct_hash);

	void notify_remote_meta(std::shared_ptr<RemoteFolder> remote, const Meta::PathRevision& revision, bitfield_type bitfield);
	void notify_remote_chunk(std::shared_ptr<RemoteFolder> remote, const blob& ct_hash);

	void handle_choke(std::shared_ptr<RemoteFolder> remote);
	void handle_unchoke(std::shared_ptr<RemoteFolder> remote);

	void put_block(const blob& ct_hash, uint32_t offset, const blob& data, std::shared_ptr<RemoteFolder> from);

	void erase_remote(std::shared_ptr<RemoteFolder> remote);

private:
	Client& client_;
	FolderGroup& exchange_group_;

	std::map<blob, std::shared_ptr<MissingChunk>> missing_chunks_;
	WeightedDownloadQueue download_queue_;

	size_t requests_overall() const;

	/* Request process */
	PeriodicProcess periodic_maintain_;
	void maintain_requests(PeriodicProcess& process);
	bool request_one();
	std::shared_ptr<RemoteFolder> find_node_for_request(const blob& ct_hash);

	/* Node management */
	std::set<std::shared_ptr<RemoteFolder>> remotes_;
};

} /* namespace librevault */
