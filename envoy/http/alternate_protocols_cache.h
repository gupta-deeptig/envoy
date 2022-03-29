#pragma once

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "envoy/common/optref.h"
#include "envoy/common/time.h"
#include "envoy/config/core/v3/protocol.pb.h"
#include "envoy/event/dispatcher.h"

#include "absl/strings/string_view.h"

namespace Envoy {
namespace Http {

/**
 * Tracks alternate protocols that can be used to make an HTTP connection to an origin server.
 * See https://tools.ietf.org/html/rfc7838 for HTTP Alternative Services and
 * https://datatracker.ietf.org/doc/html/draft-ietf-dnsop-svcb-https-04 for the
 * "HTTPS" DNS resource record.
 */
class AlternateProtocolsCache {
public:
  /**
   * Represents an HTTP origin to be connected too.
   */
  struct Origin {
  public:
    Origin(absl::string_view scheme, absl::string_view hostname, uint32_t port)
        : scheme_(scheme), hostname_(hostname), port_(port) {}

    bool operator==(const Origin& other) const {
      return std::tie(scheme_, hostname_, port_) ==
             std::tie(other.scheme_, other.hostname_, other.port_);
    }

    bool operator!=(const Origin& other) const { return !this->operator==(other); }

    bool operator<(const Origin& other) const {
      return std::tie(scheme_, hostname_, port_) <
             std::tie(other.scheme_, other.hostname_, other.port_);
    }

    bool operator>(const Origin& other) const {
      return std::tie(scheme_, hostname_, port_) >
             std::tie(other.scheme_, other.hostname_, other.port_);
    }

    bool operator<=(const Origin& other) const {
      return std::tie(scheme_, hostname_, port_) <=
             std::tie(other.scheme_, other.hostname_, other.port_);
    }

    bool operator>=(const Origin& other) const {
      return std::tie(scheme_, hostname_, port_) >=
             std::tie(other.scheme_, other.hostname_, other.port_);
    }

    std::string scheme_;
    std::string hostname_;
    uint32_t port_{};
  };

  /**
   * Represents an alternative protocol that can be used to connect to an origin
   * with a specified expiration time.
   */
  struct AlternateProtocol {
  public:
    AlternateProtocol(absl::string_view alpn, absl::string_view hostname, uint32_t port,
                      MonotonicTime expiration)
        : alpn_(alpn), hostname_(hostname), port_(port), expiration_(expiration) {}

    bool operator==(const AlternateProtocol& other) const {
      return std::tie(alpn_, hostname_, port_, expiration_) ==
             std::tie(other.alpn_, other.hostname_, other.port_, other.expiration_);
    }

    bool operator!=(const AlternateProtocol& other) const { return !this->operator==(other); }

    std::string alpn_;
    std::string hostname_;
    uint32_t port_;
    MonotonicTime expiration_;
  };

  class Http3StatusTracker {
  public:
    virtual ~Http3StatusTracker() = default;

    // Returns true if HTTP/3 is broken.
    virtual bool isHttp3Broken() const PURE;
    // Returns true if HTTP/3 is confirmed to be working.
    virtual bool isHttp3Confirmed() const PURE;
    // Marks HTTP/3 broken for a period of time, subject to backoff.
    virtual void markHttp3Broken() PURE;
    // Marks HTTP/3 as confirmed to be working and resets the backoff timeout.
    virtual void markHttp3Confirmed() PURE;
  };

  virtual ~AlternateProtocolsCache() = default;

  /**
   * Sets the possible alternative protocols which can be used to connect to the
   * specified origin. Expires after the specified expiration time.
   * @param origin The origin to set alternate protocols for.
   * @param protocols A list of alternate protocols. This list may be truncated
   * by the cache.
   */
  virtual void setAlternatives(const Origin& origin,
                               std::vector<AlternateProtocol>& protocols) PURE;

  /**
   * Sets the srtt estimate for an origin, assuming the origin exists in the cache.
   * Otherwise this is a no-op.
   * @param origin The origin to set network characteristics for.
   * @param srtt The smothed round trip time for the origin.
   */
  virtual void setSrtt(const Origin& origin, std::chrono::microseconds srtt) PURE;

  /**
   * Returns the srtt estimate for an origin, or zero, if no srtt is cached.
   * @param origin The origin to get network characteristics for.
   */
  virtual std::chrono::microseconds getSrtt(const Origin& origin) const PURE;

  /**
   * Returns the possible alternative protocols which can be used to connect to the
   * specified origin, or nullptr if not alternatives are found. The returned reference
   * is owned by the AlternateProtocolsCache and is valid until the next operation on the
   * AlternateProtocolsCache.
   * @param origin The origin to find alternate protocols for.
   * @return An optional list of alternate protocols for the given origin.
   */
  virtual OptRef<const std::vector<AlternateProtocol>> findAlternatives(const Origin& origin) PURE;

  /**
   * Returns the number of entries in the map.
   * @return the number if entries in the map.
   */
  virtual size_t size() const PURE;

  /**
   * @param origin The origin to get HTTP/3 status tracker for.
   * @return the existing status tracker or creating a new one if there is none.
   */
  virtual AlternateProtocolsCache::Http3StatusTracker&
  getOrCreateHttp3StatusTracker(const Origin& origin) PURE;
};

using AlternateProtocolsCacheSharedPtr = std::shared_ptr<AlternateProtocolsCache>;
using Http3StatusTrackerPtr = std::unique_ptr<AlternateProtocolsCache::Http3StatusTracker>;

/**
 * A manager for multiple alternate protocols caches.
 */
class AlternateProtocolsCacheManager {
public:
  virtual ~AlternateProtocolsCacheManager() = default;

  /**
   * Get an alternate protocols cache.
   * @param config supplies the cache parameters. If a cache exists with the same parameters it
   *               will be returned, otherwise a new one will be created.
   * @param dispatcher supplies the current thread's dispatcher, for cache creation.
   */
  virtual AlternateProtocolsCacheSharedPtr
  getCache(const envoy::config::core::v3::AlternateProtocolsCacheOptions& config,
           Event::Dispatcher& dispatcher) PURE;
};

using AlternateProtocolsCacheManagerSharedPtr = std::shared_ptr<AlternateProtocolsCacheManager>;

/**
 * Factory for getting an alternate protocols cache manager.
 */
class AlternateProtocolsCacheManagerFactory {
public:
  virtual ~AlternateProtocolsCacheManagerFactory() = default;

  /**
   * Get the alternate protocols cache manager.
   */
  virtual AlternateProtocolsCacheManagerSharedPtr get() PURE;
};

} // namespace Http
} // namespace Envoy
