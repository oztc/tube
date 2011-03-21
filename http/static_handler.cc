#include "pch.h"

#include <sstream>
#include <dirent.h>
#include <sys/types.h>

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

#define MAX_TIME_LEN 128

static std::string
build_last_modified(const time_t* last_modified_time)
{
    struct tm* t = gmtime(last_modified_time);
    char time_str[MAX_TIME_LEN];
    strftime(time_str, MAX_TIME_LEN, "%a, %d %b %Y %T GMT", t);
    return std::string(time_str);
}

void
StaticHttpHandler::respond_zero_copy(const std::string& path,
                                     struct stat64 stat,
                                     HttpRequest& request,
                                     HttpResponse& response)
{
    off64_t file_size = stat.st_size;
    int file_desc = 0;

    if (request.method() != HTTP_HEAD) {
        file_desc = ::open(path.c_str(), O_RDONLY);
        // cannot open, this is access forbidden
        if (file_desc < 0) {
            respond_error(HttpResponseStatus::kHttpResponseForbidden,
                          request, response);
            return;
        }
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
                HttpResponseStatus::kHttpResponseRequestedrangenotsatisfiable,
                request, response);
            return;
        }
        // sound everything is good
        response.set_content_length(length);
        response.add_header("Content-Range",
                            build_range_response(offset, length, file_size));
        response.add_header("Last-Modified",
                            build_last_modified(&stat.st_mtime));
        response.respond(HttpResponseStatus::kHttpResponsePartialContent);
        if (request.method() != HTTP_HEAD) {
            response.write_file(file_desc, offset, length);
        }
    } else {
        response.set_content_length(file_size);
        response.add_header("Last-Modified",
                            build_last_modified(&stat.st_mtime));
        response.respond(HttpResponseStatus::kHttpResponseOK);
        if (request.method() != HTTP_HEAD) {
            response.write_file(file_desc, 0, file_size);
        }
    }
}

void
StaticHttpHandler::respond_directory_list(const std::string& path,
                                          const std::string& href_path,
                                          HttpRequest& request,
                                          HttpResponse& response)
{
    std::stringstream ss;
    ss << "<html><head><title>Directory List " << href_path << "</title>";
    // TOOD: add custom css if needed
    ss << "</head><body>" << std::endl;
    DIR* dirp = opendir(path.c_str());
    if (!dirp) {
        respond_error(HttpResponseStatus::kHttpResponseForbidden,
                      request, response);
        return;
    }
    dirent* ent = NULL;
    while ((ent = readdir(dirp))) {
        std::string ent_name = ent->d_name;
        std::string ent_path = path + "/" + ent->d_name;
        if (ent_name == ".") {
            continue;
        }
        if (href_path == "/" && ent_name == "..") {
            continue; // skip .. on root directory
        }
        struct stat64 buf;
        if (::stat64(ent_path.c_str(), &buf) < 0) {
            continue;
        }
        if (S_ISREG(buf.st_mode)) {
            ss << "<div class=\"regular\">"
               << "<a href=\"" << ent_name << "\">" << ent_name << "</a>"
               << "</div>" << std::endl;
        } else if (S_ISDIR(buf.st_mode)) {
            ss << "<div class=\"directory\">"
               << "<a href=\"" << ent_name << "/\">" << ent_name << "</a>"
               << "</div>" << std::endl;
        }
    }
    closedir(dirp);
    ss << "</body></html>" << std::endl;
    response.add_header("Content-Type", "text/html");
    if (request.method() != HTTP_HEAD) {
        response.write_string(ss.str());
    } else {
        response.set_content_length(ss.str().length());
    }
    response.respond(HttpResponseStatus::kHttpResponseOK);
}

void
StaticHttpHandler::respond_error(const HttpResponseStatus& error,
                                 HttpRequest& request,
                                 HttpResponse& response)
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

    if (request.method() != HTTP_GET && request.method() != HTTP_POST
        && request.method() != HTTP_HEAD) {
        respond_error(HttpResponseStatus::kHttpResponseBadRequest, request,
                      response);
    }

    struct stat64 buf;
    std::string filepath = doc_root_ + filename;
    if (::stat64(filepath.c_str(), &buf) < 0) {
        respond_error(HttpResponseStatus::kHttpResponseNotFound,
                      request, response);
        return;
    }
    if (S_ISREG(buf.st_mode)) {
        respond_zero_copy(filepath, buf, request, response);
    } else if (S_ISDIR(buf.st_mode)) {
        respond_directory_list(filepath, filename, request, response);
    } else {
        respond_error(HttpResponseStatus::kHttpResponseForbidden,
                      request, response);
    }
}

}
