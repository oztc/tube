#include "pch.h"

#include <sstream>

#include "http/static_handler.h"
#include "utils/logger.h"

namespace pipeserv {

// remove all . and .. directory access
static std::string
string_remove_dots(const std::string& path)
{
    std::string res;
    for (size_t i = 0; i < path.length(); i++) {
        res += path[i];
        if (path[i] == '/') {
            size_t j = path.find('/', i + 1);
            if (j == std::string::npos) {
                j = path.length();
            }
            std::string ent = path.substr(i + 1, j - i - 1);
            if (ent != "." && ent != "..") {
                res.append(ent);
            }
            i = j - 1;
        }
    }
    return res;
}

std::string
StaticHttpHandler::remove_path_dots(const std::string& path)
{
    return string_remove_dots(path);
}

StaticHttpHandler::StaticHttpHandler()
{
    // setting up the default configuration parameters
    add_option("doc_root", "/var/www");
}

void
StaticHttpHandler::load_param()
{
    doc_root_ = option("doc_root");
}

// currently we only support single range
static void
parse_range(std::string range_desc, off64_t& offset, off64_t& length)
{
    offset = 0;
    length = -1;
    int end_byte = 0;

    size_t i = 0;
    while (i < range_desc.length()) {
        i++;
        if (range_desc[i - 1] == '=') {
            break;
        }
    }

    while (i < range_desc.length()) {
        char ch = range_desc[i];
        if (ch >= '0' && ch <= '9') {
            offset = 10 * offset + (ch - '0');
        } else if (ch == '-') {
            break;
        } else {
            return;
        }
        i++;
    }

    while (i < range_desc.length()) {
        char ch = range_desc[i];
        if (ch >= '0' && ch <= '9') {
            end_byte = 10 * end_byte + (ch - '0');
            length = end_byte - offset + 1;
        } else {
            return;
        }
        i++;
    }
}

static std::string
build_range_response(off64_t offset, off64_t length, off64_t file_size)
{
    std::stringstream ss;
    ss << "bytes " << offset << '-' << offset + length - 1 << '/' << file_size;
    return ss.str();
}

void
StaticHttpHandler::respond_zero_copy(const std::string& path,
                                     HttpRequest& request,
                                     HttpResponse& response)
{
    struct stat64 buf;
    u64 file_size = 0;
    if (::stat64(path.c_str(), &buf) < 0) {
        respond_error(request, response,
                      HttpResponseStatus::kHttpResponseNotFound);
        return;
    }
    file_size = buf.st_size;

    int file_desc = ::open(path.c_str(), O_RDONLY);
    // cannot open, this is access forbidden
    if (file_desc < 0) {
        respond_error(request, response,
                      HttpResponseStatus::kHttpResponseForbidden);
        return;
    }

    if (request.has_header("Range")) {
        // parse range
        off64_t offset, length;
        parse_range(request.find_header_value("Range"), offset, length);
        // incomplete range, doesn't have a length
        // we'll transfer all begining from that offset
        if (length < 0) {
            length = file_size - offset;
        }
        // exceeded file size, this is invalid range
        if ((size_t) (offset + length) > file_size) {
            respond_error(
                request, response,
                HttpResponseStatus::kHttpResponseRequestedrangenotsatisfiable);
            return;
        }
        // sound everything is good
        response.set_content_length(length);
        response.add_header("Content-Range",
                            build_range_response(offset, length, file_size));
        response.respond(HttpResponseStatus::kHttpResponsePartialContent);
        response.write_file(file_desc, offset, length);
    } else {
        response.set_content_length(file_size);
        response.respond(HttpResponseStatus::kHttpResponseOK);
        response.write_file(file_desc, 0, file_size);
    }
}

//void
//StaticHttpHandler::respond_directory_list(const std::string& path,
//                                          const std::string& href_path,
//                                          HttpRequest& request,
//                                          HttpResponse& response)
//{
//}

void
StaticHttpHandler::respond_error(HttpRequest& request,
                                 HttpResponse& response,
                                 const HttpResponseStatus& error)
{
    // TODO: custom error page
    response.write_string(error.reason);
    response.respond(error);
}

void
StaticHttpHandler::handle_request(HttpRequest& request, HttpResponse& response)
{
    std::string filename = HttpRequest::url_decode(request.path());
    filename = remove_path_dots(filename);
    LOG(DEBUG, "%s requested", filename.c_str());
    respond_zero_copy(doc_root_ + filename, request, response);
}

}
