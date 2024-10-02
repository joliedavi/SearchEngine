// Jolie Davison jdavi@cs.washington.edu Copyright 2024 Jolie Davison

#include <stdio.h>
#include <string>
#include <cstdlib>
#include <iostream>
#include <sstream>

extern "C" {
  #include "libhw2/FileParser.h"
}

#include "./HttpUtils.h"
#include "./FileReader.h"

using std::string;

namespace hw4 {

bool FileReader::ReadFile(string* const contents) {
  string full_file = basedir_ + "/" + fname_;

  // Read the file into memory, and store the file contents in the
  // output parameter "contents."  Be careful to handle binary data
  // correctly; i.e., you probably want to use the two-argument
  // constructor to std::string (the one that includes a length as a
  // second argument).
  //
  // You might find ::ReadFileToString() from HW2 useful here. Remember that
  // this function uses malloc to allocate memory, so you'll need to use
  // free() to free up that memory after copying to the string output
  // parameter.
  //
  // Alternatively, you can use a unique_ptr with a malloc/free
  // deleter to automatically manage this for you; see the comment in
  // HttpUtils.h above the MallocDeleter class for details.

  // STEP 1:

  // Returns false if the file exists above the basedir in the file
  // system hierarchy
  if (!IsPathSafe(basedir_, full_file)) {
    return false;
  }
  int size = 0;
  char* result = ReadFileToString(full_file.c_str(), &size);
  // Returns false if unable to read string
  if (result == nullptr) {
    return false;
  }

  *contents = std::string(result, size);
  free(result);

  return true;
}

}  // namespace hw4
