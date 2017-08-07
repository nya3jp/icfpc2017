#include "common/protocol.h"

#include <vector>

#include <sys/epoll.h>

#include "base/files/scoped_file.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"

DEFINE_bool(logprotocol, false, "Output message for debugging.");
DEFINE_bool(logreadprotocol, false, "Output message for debugging.");
DEFINE_bool(logwriteprotocol, false, "Output message for debugging.");

namespace common {

namespace {

class FdWaiter {
 public:
  FdWaiter() = default;
  ~FdWaiter() = default;

  void Initialize(
      int fd, const base::TimeDelta& timeout,
      const base::TimeTicks& start_time) {
    if (!timeout.is_zero()) {
      CHECK(!start_time.is_null());
      end_time_ = start_time + timeout;
    }
    fd_ = fd;

    epoll_fd_.reset(epoll_create1(0));
    PCHECK(epoll_fd_.is_valid());

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    PCHECK(epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, fd, &ev) == 0);
  }

  bool Wait() {
    const int kMaxEvents = 1;
    struct epoll_event events[kMaxEvents];

    while (true) {
      int timeout_ms;
      if (end_time_.is_null()) {
        timeout_ms = -1;
      } else {
        timeout_ms = (end_time_ - base::TimeTicks::Now()).InMilliseconds();
        if (timeout_ms <= 0) {
          // Timed out.
          return false;
        }
      }

      int num_fds =
          epoll_wait(epoll_fd_.get(), events, kMaxEvents, timeout_ms);
      if (num_fds == -1) {
        if (errno == EINTR)
          continue;
        NOTREACHED();
        return false;
      }
      if (num_fds == 0) {
        // EPOLL Timeout. Will return false in the above check in the next
        // iteration.
        continue;
      }

      CHECK_EQ(num_fds, 1);
      CHECK_EQ(events[0].data.fd, fd_);
      return true;
    }
  }

 private:
  base::ScopedFD epoll_fd_;
  base::TimeTicks end_time_;
  int fd_ = -1;
  DISALLOW_COPY_AND_ASSIGN(FdWaiter);
};

void WritePingInternal(
    FILE* fp, base::StringPiece field_name, const std::string& name) {
  DLOG(INFO) << "Sending name: " << name;
  base::DictionaryValue ping;
  ping.SetString(field_name, name);
  WriteMessage(fp, ping);
}

base::Optional<std::string> ReadPingInternal(
    FILE* fp, base::StringPiece field_name) {
  auto message = base::DictionaryValue::From(ReadMessage(fp));
  std::string result;
  if (!message || !message->GetString(field_name, &result))
    return base::nullopt;
  DLOG(INFO) << "Received name: " << result;
  return result;
}

}  // namespace

std::unique_ptr<base::Value> ReadMessage(FILE* fp,
                                         const base::TimeDelta& timeout,
                                         const base::TimeTicks& start_time) {
  int fd = fileno(fp);
  FdWaiter waiter;
  waiter.Initialize(fd, timeout, start_time);

  size_t size;
  {
    char buf[16];
    for (int pos = 0;; ++pos) {
      if (!waiter.Wait()) {
        DLOG(INFO) << "Timeout during reading the size of the message";
        return nullptr;
      }
      ssize_t result = HANDLE_EINTR(read(fd, &buf[pos], 1));
      if (result == 0) {
        DLOG(ERROR) << "Unexpected EOF";
        return nullptr;
      }
      if (buf[pos] == ':') {
        buf[pos] = '\0';
        CHECK(base::StringToSizeT(buf, &size));
        break;
      }
      if (pos >= 10) {
        DLOG(ERROR) << "Unexpected message format.";
        return nullptr;
      }
    }
  }
  CHECK(size >= 0) << "Invalid JSON message header";

  std::vector<char> buf(size, 'x');
  for (size_t filled = 0; filled < size; ) {
    if (!waiter.Wait()) {
      DLOG(INFO) << "Timeout during reading the body of the message";
      return nullptr;
    }
    ssize_t result =
        HANDLE_EINTR(read(fd, buf.data() + filled, size - filled));
    PCHECK(result >= 0);
    if (result == 0) {
      DLOG(ERROR) << "Unexpected EOF";
      return nullptr;
    }
    filled += result;
  }
  if (FLAGS_logprotocol || FLAGS_logreadprotocol)
    LOG(INFO) << "read: " << std::string(buf.data(), buf.size());
  return base::JSONReader::Read(base::StringPiece(buf.data(), buf.size()));
}

std::unique_ptr<base::Value> ReadMessage(FILE* fp) {
  return ReadMessage(fp, base::TimeDelta(), base::TimeTicks());
}

void WriteMessage(FILE* fp, const base::Value& value) {
  std::string text;
  CHECK(base::JSONWriter::Write(value, &text));
  if (FLAGS_logprotocol || FLAGS_logwriteprotocol)
    LOG(INFO) << "write: " << text;
  fprintf(fp, "%d:", static_cast<int>(text.size()));
  CHECK_EQ(text.size(), fwrite(text.c_str(), 1, text.size(), fp));
  fflush(fp);
}

void WritePing(FILE* fp, const std::string& name) {
  WritePingInternal(fp, "me", name);
}

base::Optional<std::string> ReadPing(FILE* fp) {
  return ReadPingInternal(fp, "me");
}

void WritePong(FILE* fp, const std::string& name) {
  WritePingInternal(fp, "you", name);
}

base::Optional<std::string> ReadPong(FILE* fp) {
  return ReadPingInternal(fp, "you");
}

}  // namespace common
