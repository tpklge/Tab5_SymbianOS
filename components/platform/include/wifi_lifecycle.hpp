// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace tab5 {
// Called under the native event-post mutex. State represents the last event
// successfully queued, not driver command completion or association/DHCP.
class WifiLifecycle {
public:
  enum class Event { Other, Start, Stop };
private:
  enum class State { Unknown, Started, Stopped };
  State state_ = State::Unknown;
  uint32_t duplicateStarts_ = 0, duplicateStops_ = 0;
public:
  template<class Post> int post(Event event, Post deliver) {
    if (event == Event::Start && state_ == State::Started) {
      ++duplicateStarts_;
      return 0;
    }
    if (event == Event::Stop && state_ == State::Stopped) {
      ++duplicateStops_;
      return 0;
    }
    const int result = deliver();
    if (result == 0) {
      if (event == Event::Start) state_ = State::Started;
      else if (event == Event::Stop) state_ = State::Stopped;
    }
    return result;
  }
  uint32_t duplicateStarts() const { return duplicateStarts_; }
  uint32_t duplicateStops() const { return duplicateStops_; }
};
}
