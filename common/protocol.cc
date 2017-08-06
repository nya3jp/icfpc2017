#include "common/protocol.h"

#include <vector>

#include "base/logging.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"

namespace common {

std::unique_ptr<base::Value> ReadMessage(FILE* fp) {
  size_t size;
  {
    char buf[16];
    for (int i = 0; i < 10; ++i) {
      size_t result = fread(&buf[i], sizeof(char), 1, fp);
      PCHECK(result == 1) << "fread";
      if (buf[i] == ':') {
        buf[i] = '\0';
        CHECK(base::StringToSizeT(buf, &size));
        break;
      }
    }
  }

  std::vector<char> buf(size, 'x');
  CHECK_EQ(size, fread(buf.data(), sizeof(char), size, fp));
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
