// Jolie Davison jdavi@cs.washington.edu Copyright 2024 Jolie Davison

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

#include "./FileReader.h"
#include "./HttpConnection.h"
#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpServer.h"
#include "./libhw3/QueryProcessor.h"

using std::cerr;
using std::cout;
using std::endl;
using std::list;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;

namespace hw4 {
///////////////////////////////////////////////////////////////////////////////
// Constants, internal helper functions
///////////////////////////////////////////////////////////////////////////////
static const char* kThreegleStr =
  "<html><head><title>333gle</title></head>\n"
  "<body>\n"
  "<center style=\"font-size:500%;\">\n"
  "<span style=\"position:relative;bottom:-0.33em;color:orange;\">3</span>"
    "<span style=\"color:red;\">3</span>"
    "<span style=\"color:gold;\">3</span>"
    "<span style=\"color:blue;\">g</span>"
    "<span style=\"color:green;\">l</span>"
    "<span style=\"color:red;\">e</span>\n"
  "</center>\n"
  "<p>\n"
  "<div style=\"height:20px;\"></div>\n"
  "<center>\n"
  "<form action=\"/query\" method=\"get\">\n"
  "<input type=\"text\" size=30 name=\"terms\" />\n"
  "<input type=\"submit\" value=\"Search\" />\n"
  "</form>\n"
  "</center><p>\n";

// static
const int HttpServer::kNumThreads = 100;

// This is the function that threads are dispatched into
// in order to process new client connections.
static void HttpServer_ThrFn(ThreadPool::Task* t);

// Given a request, produce a response.
static HttpResponse ProcessRequest(const HttpRequest& req,
                            const string& base_dir,
                            const list<string>& indices);

// Process a file request.
static HttpResponse ProcessFileRequest(const string& uri,
                                const string& base_dir);

// Process a query request.
static HttpResponse ProcessQueryRequest(const string& uri,
                                 const list<string>& indices);


///////////////////////////////////////////////////////////////////////////////
// HttpServer
///////////////////////////////////////////////////////////////////////////////
bool HttpServer::Run(void) {
  // Create the server listening socket.
  int listen_fd;
  cout << "  creating and binding the listening socket..." << endl;
  if (!socket_.BindAndListen(AF_INET6, &listen_fd)) {
    cerr << endl << "Couldn't bind to the listening socket." << endl;
    return false;
  }

  // Spin, accepting connections and dispatching them.  Use a
  // threadpool to dispatch connections into their own thread.
  cout << "  accepting connections..." << endl << endl;
  ThreadPool tp(kNumThreads);
  while (1) {
    HttpServerTask* hst = new HttpServerTask(HttpServer_ThrFn);
    hst->base_dir = static_file_dir_path_;
    hst->indices = &indices_;
    if (!socket_.Accept(&hst->client_fd,
                    &hst->c_addr,
                    &hst->c_port,
                    &hst->c_dns,
                    &hst->s_addr,
                    &hst->s_dns)) {
      // The accept failed for some reason, so quit out of the server.
      // (Will happen when kill command is used to shut down the server.)
      break;
    }
    // The accept succeeded; dispatch it.
    tp.Dispatch(hst);
  }
  return true;
}

static void HttpServer_ThrFn(ThreadPool::Task* t) {
  // Cast back our HttpServerTask structure with all of our new
  // client's information in it.
  unique_ptr<HttpServerTask> hst(static_cast<HttpServerTask*>(t));
  cout << "  client " << hst->c_dns << ":" << hst->c_port << " "
       << "(IP address " << hst->c_addr << ")" << " connected." << endl;

  // Read in the next request, process it, and write the response.

  // Use the HttpConnection class to read and process the next
  // request from our current client, then write out our response.  If
  // the client sends a "Connection: close\r\n" header, then shut down
  // the connection -- we're done.
  //
  // Hint: the client can make multiple requests on our single connection,
  // so we should keep the connection open between requests rather than
  // creating/destroying the same connection repeatedly.

  // STEP 1:
  HttpConnection hc = HttpConnection(hst->client_fd);
  bool done = false;
  while (!done) {
    HttpRequest result;
    if (!hc.GetNextRequest(&result)) {
      cerr << "Could not get next request" << endl;
      break;
    }
    HttpResponse response = ProcessRequest(result,
                                          hst->base_dir,
                                          *hst->indices);

    if (!hc.WriteResponse(response)) {
      cerr << "Could not write response" << endl;
      break;
    }

    if (result.GetHeaderValue("connection") == "close") {
      cerr << "Client is closing connection..." << endl;
      done = true;
    }
  }
  hc.~HttpConnection();
}

static HttpResponse ProcessRequest(const HttpRequest& req,
                            const string& base_dir,
                            const list<string>& indices) {
  // Is the user asking for a static file?
  if (req.uri().substr(0, 8) == "/static/") {
    return ProcessFileRequest(req.uri(), base_dir);
  }

  // The user must be asking for a query.
  return ProcessQueryRequest(req.uri(), indices);
}

static HttpResponse ProcessFileRequest(const string& uri,
                                const string& base_dir) {
  // The response we'll build up.
  HttpResponse ret;

  // Steps to follow:
  // 1. Use the URLParser class to figure out what file name
  //    the user is asking for. Note that we identify a request
  //    as a file request if the URI starts with '/static/'
  //
  // 2. Use the FileReader class to read the file into memory
  //
  // 3. Copy the file content into the ret.body
  //
  // 4. Depending on the file name suffix, set the response
  //    Content-type header as appropriate, e.g.,:
  //      --> for ".html" or ".htm", set to "text/html"
  //      --> for ".jpeg" or ".jpg", set to "image/jpeg"
  //      --> for ".png", set to "image/png"
  //      etc.
  //    You should support the file types mentioned above,
  //    as well as ".txt", ".js", ".css", ".xml", ".gif",
  //    and any other extensions to get bikeapalooza
  //    to match the solution server.
  //
  // be sure to set the response code, protocol, and message
  // in the HttpResponse as well.
  string file_name = "";

  // STEP 2:

  // Extract the file name from the uri. The base_dir is in the form
  // "./base_dir" so we want to not include the "./"" or the following
  // "/" in the uri for our count
  size_t args_pos = uri.find("?");
  if (args_pos != string::npos) {
    file_name = uri.substr(8, args_pos - 8);
  } else {
    file_name = uri.substr(8);
  }
  // if (!IsPathSafe(base_dir, file_name)) {
  //   cerr << "Error: File is not within document subdirectory tree" << endl;
  //   return ret;
  // }
  FileReader freader(base_dir, file_name);
  string file_contents;

  if (freader.ReadFile(&file_contents)) {
    // We found the file
    ret.set_protocol("HTTP/1.1");
    ret.set_response_code(200);
    ret.set_message("OK");
    ret.AppendToBody(file_contents);

    // Set response content header
    string suffix = file_name.substr(file_name.find_last_of(".") + 1);
    string suffix_header = "";
    if (suffix.find("htm") == 0 || suffix.find("html") == 0) {
      suffix_header = "text/html";
    } else if (suffix.find("jpeg") == 0 || suffix.find("jpg") == 0) {
      suffix_header = "image/jpeg";
    } else if (suffix.find("png") == 0) {
      suffix_header = "image/png";
    } else if (suffix.find("txt") == 0) {
      suffix_header = "text/plain";
    } else if (suffix.find("js") == 0) {
      suffix_header = "text/javascript";
    } else if (suffix.find("css") == 0) {
      suffix_header = "text/css";
    } else if (suffix.find("xml") == 0) {
      suffix_header = "application/xml";
    } else if (suffix.find("gif") == 0) {
      suffix_header = "image/gif";
    } else {
      suffix_header = "text/plain";
    }
    ret.set_content_type(suffix_header);
    return ret;
  } else {
    // If you couldn't find the file, return an HTTP 404 error.
    ret.set_protocol("HTTP/1.1");
    ret.set_response_code(404);
    ret.set_message("Not Found");
    ret.AppendToBody("<html><body>Couldn't find file \""
                     + EscapeHtml(file_name)
                     + "\"</body></html>\n");
    return ret;
  }
}

static HttpResponse ProcessQueryRequest(const string& uri,
                                 const list<string>& indices) {
  // The response we're building up.
  HttpResponse ret;

  // Your job here is to figure out how to present the user with
  // the same query interface as our solution_binaries/http333d server.
  // A couple of notes:
  //
  // 1. The 333gle logo and search box/button should be present on the site.
  //
  // 2. If the user had previously typed in a search query, you also need
  //    to display the search results.
  //
  // 3. you'll want to use the URLParser to parse the uri and extract
  //    search terms from a typed-in search query.  convert them
  //    to lower case.
  //
  // 4. Initialize and use hw3::QueryProcessor to process queries with the
  //    search indices.
  //
  // 5. With your results, try figuring out how to hyperlink results to file
  //    contents, like in solution_binaries/http333d. (Hint: Look into HTML
  //    tags!)

  // STEP 3:
  // Show title and search bar on screen
  stringstream ss;
  ss << kThreegleStr;

  // Parse the uri
  URLParser parsed_uri;
  parsed_uri.Parse(uri);
  // Extract the args and check if there are any
  std::map<std::string, std::string> args = parsed_uri.args();
  if (!args["terms"].empty()) {
    // Extract the terms into a string and set to lower
    string terms_str = args["terms"];
    boost::algorithm::to_lower(terms_str);

    // Split terms on "+" and store in vector
    vector<string> terms_vec;
    string temp_str;
    stringstream temp_ss(terms_str);
    while (std::getline(temp_ss, temp_str, ' ')) {
        // store term string in the vector
        terms_vec.push_back(temp_str);
    }

    hw3::QueryProcessor queryP(indices, true);
    vector<hw3::QueryProcessor::QueryResult> queryR =
                    queryP.ProcessQuery(terms_vec);

    if (queryR.empty()) {
      ss << "<div>No results found for <b>" <<
            EscapeHtml(terms_str) << "</b></div>";
    } else {
      ss << "<div>" << queryR.size() << " results found for <b>" <<
            EscapeHtml(terms_str) << "</b></div><br>";
    }
    for (const hw3::QueryProcessor::QueryResult &document : queryR) {
      if (document.document_name.find("http://") == 0
          || document.document_name.find("https://") == 0) {
        ss << "<div><li><a href=\"" << document.document_name
            << "\" target=\"_blank\">" << document.document_name
            << "</a> [" << document.rank << "]</li></div>";
      } else {
        ss << "<div><li><a href=\"/static/" << document.document_name
            << "\">" << document.document_name << "</a> ["
            << document.rank << "]</li></div>";
      }
    }
  }

  ret.set_protocol("HTTP/1.1");
  ret.set_response_code(200);
  ret.set_message("OK");
  ret.AppendToBody(ss.str());
  ret.set_content_type("text/html");

  return ret;
}

}  // namespace hw4
