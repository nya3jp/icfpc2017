#include "common/protocol.h"

#include <vector>

#include <sys/epoll.h>

#include "base/logging.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"

namespace common {

bool WaitForFdReadable(int fd, const base::TimeDelta& timeout, const base::TimeTicks& start_time) {
  int timeout_ms = -1;

  if (!timeout.is_zero()) {
    base::TimeDelta elapsed = start_time - base::TimeTicks::Now();
    if (elapsed > timeout)
      return false;
    timeout_ms = (timeout - elapsed).InMilliseconds();
  }

  int epoll_fd = epoll_create1(0);
  CHECK_NE(epoll_fd, -1);

  struct epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = fd;
  epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);

  const int kMaxEvents = 1;
  struct epoll_event events[kMaxEvents];

  for (;;) {
    int num_fds = epoll_wait(epoll_fd, events, kMaxEvents, timeout_ms);
    if (num_fds == -1) {
      if (errno == EINTR)
        continue;
      NOTREACHED();
    }
    if (num_fds == 0 || (!timeout.is_zero() && base::TimeTicks::Now() > start_time + timeout)) {
      // timeout
      close(epoll_fd);
      return false;
    }
    CHECK_EQ(num_fds, 1);
    CHECK_EQ(events[0].data.fd, fd);
    close(epoll_fd);
    return true;
  }
}

std::unique_ptr<base::Value> ReadMessage(FILE* fp,
                                         const base::TimeDelta& timeout,
                                         const base::TimeTicks& start_time) {
  int fd = fileno(fp);

  int size = -1;
  {
    char buf[16];
    int pos = 0;
    for (;;) {
      ssize_t result = read(fd, &buf[pos], 1);

      if (result == -1) {
        if (errno == EINTR)
          continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          if (!WaitForFdReadable(fd, timeout, start_time))
            return NULL;
          continue;
        }
        return NULL;
      }

      PCHECK(result == 1) << "read";
      if (buf[pos] == ':') {
        buf[pos] = '\0';
        CHECK(base::StringToInt(buf, &size));
        break;
      }

      if (pos >= 16)
        return NULL;
      ++pos;
    }
  }
  CHECK(size >= 0) << "Invalid JSON message header";

  std::vector<char> buf(size, 'x');
  size_t filled = 0;
  while (filled < size) {
    ssize_t result = read(fd, buf.data() + filled, size - filled);

    if (result == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        if (!WaitForFdReadable(fd, timeout, start_time))
          return NULL;
        continue;
      }
      if (errno == EINTR)
        continue;
      return NULL;
    }

    filled += result;
  }
  return base::JSONReader::Read(base::StringPiece(buf.data(), buf.size()));
}

void WriteMessage(FILE* fp, const base::Value& value) {
  std::string text;
  CHECK(base::JSONWriter::Write(value, &text));
  fprintf(fp, "%d:", static_cast<int>(text.size()));
  CHECK_EQ(text.size(), fwrite(text.c_str(), 1, text.size(), fp));
  fflush(fp);
}

}  // namespace common
