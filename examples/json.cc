#include <stdio.h>

#include <iostream>
#include <memory>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "gflags/gflags.h"
#include "glog/logging.h"

DEFINE_string(json_file, "", "Path to a JSON file.");

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  std::string json_in;
  CHECK(base::ReadFileToString(base::FilePath(FLAGS_json_file), &json_in));

  std::unique_ptr<base::Value> value = base::JSONReader::Read(json_in);

  std::string json_out;
  base::JSONWriter::Write(*value, &json_out);

  std::cout << json_out << std::endl;

  return 0;
}
