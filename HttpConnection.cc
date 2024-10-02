// Jolie Davison jdavi@cs.washington.edu Copyright 2024 Jolie Davison

#include <stdint.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <map>
#include <string>
#include <vector>

#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpConnection.h"

using std::map;
using std::string;
using std::vector;

namespace hw4 {

static const char* kHeaderEnd = "\r\n\r\n";
static const int kHeaderEndLen = 4;

bool HttpConnection::GetNextRequest(HttpRequest* const request) {
  // Use WrappedRead from HttpUtils.cc to read bytes from the files into
  // private buffer_ variable. Keep reading until:
  // 1. The connection drops
  // 2. You see a "\r\n\r\n" indicating the end of the request header.
  //
  // Hint: Try and read in a large amount of bytes each time you call
  // WrappedRead.
  //
  // After reading complete request header, use ParseRequest() to parse into
  // an HttpRequest and save to the output parameter request.
  //
  // Important note: Clients may send back-to-back requests on the same socket.
  // This means WrappedRead may also end up reading more than one request.
  // Make sure to save anything you read after "\r\n\r\n" in buffer_ for the
  // next time the caller invokes GetNextRequest()!

  // STEP 1:
  unsigned char buf[2048];
  size_t request_bytes = 0;
  while ((request_bytes = buffer_.find(kHeaderEnd)) == string::npos) {
    int bytes_read = WrappedRead(fd_, buf, sizeof(buf));
    // Returns false if fatal error
    if (bytes_read == -1) {
      return false;
    }
    buffer_.append(reinterpret_cast<char*>(buf), bytes_read);
  }
  string request_str = buffer_.substr(0, request_bytes + kHeaderEndLen);
  *request = HttpConnection::ParseRequest(request_str);
  // Save the remaining data for next request
  buffer_ = buffer_.substr(request_bytes + strlen(kHeaderEnd));

  return true;  // you may need to change this return value
}

bool HttpConnection::WriteResponse(const HttpResponse& response) const {
  string str = response.GenerateResponseString();
  int res = WrappedWrite(fd_,
                         reinterpret_cast<const unsigned char*>(str.c_str()),
                         str.length());
  if (res != static_cast<int>(str.length()))
    return false;
  return true;
}

HttpRequest HttpConnection::ParseRequest(const string& request) const {
  HttpRequest req("/");  // by default, get "/".

  // Plan for STEP 2:
  // 1. Split the request into different lines (split on "\r\n").
  // 2. Extract the URI from the first line and store it in req.URI.
  // 3. For the rest of the lines in the request, track the header name and
  //    value and store them in req.headers_ (e.g. HttpRequest::AddHeader).
  //
  // Hint: Take a look at HttpRequest.h for details about the HTTP header
  // format that you need to parse.
  //
  // You'll probably want to look up boost functions for:
  // - Splitting a string into lines on a "\r\n" delimiter
  // - Trimming whitespace from the end of a string
  // - Converting a string to lowercase.
  //
  // Note: If a header is malformed, skip that line.

  // STEP 2:
  vector<string> lines;
  // Split into different lines on "\r\n"
  boost::algorithm::split(lines, request,
                          boost::is_any_of("\r\n"),
                          boost::token_compress_on);

  if (!lines.empty()) {
    // Split first line on spaces to extract URI
    vector<string> first_line_vec;
    string first_line_str = lines[0];
    boost::algorithm::split(first_line_vec, first_line_str,
                            boost::is_any_of(" "),
                            boost::token_compress_on);

    // Extract URI
    req.set_uri(first_line_vec[1]);

    for (size_t i = 1; i < lines.size(); i++) {
      string line = lines[i];

      // If header is malformed, skip it
      std::size_t headername_split = line.find(':');
      if (headername_split == string::npos) {
          continue;
      }

      // Set headername and headerval
      string headername = line.substr(0, headername_split);
      string headerval = line.substr(headername_split + 1);

      // Trim whitespace from the headername and headerval
      boost::algorithm::trim(headername);
      boost::algorithm::trim(headerval);

      // Convert header name to lowercase
      boost::algorithm::to_lower(headername);
      boost::algorithm::to_lower(headerval);

      // Add the header to req
      req.AddHeader(headername, headerval);
    }
  }

  return req;
}

}  // namespace hw4
