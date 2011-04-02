#include "pch.h"

#include <sstream>
#include <dirent.h>
#include <sys/types.h>

#include "http/static_handler.h"
#include "utils/logger.h"

namespace tube {

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
    add_option("error_root", "");
    add_option("allow_index", "true");
    add_option("index_page_css", "");
    add_option("max_cache_entry", "0");
    add_option("max_entry_size", "4096");
}

void
StaticHttpHandler::load_param()
{
    doc_root_ = option("doc_root");
    error_root_ = option("error_root");
    allow_index_ = utils::parse_bool(option("allow_index"));
    index_page_css_ = option("index_page_css");

    io_cache_.set_max_cache_entry(atoi(option("max_cache_entry").c_str()));
    io_cache_.set_max_entry_size(atoi(option("max_entry_size").c_str()));
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
    struct tm gmt;
    gmtime_r(last_modified_time, &gmt); // thread safe
    char time_str[MAX_TIME_LEN];
    strftime(time_str, MAX_TIME_LEN, "%a, %d %b %Y %T GMT", &gmt);
    return std::string(time_str);
}

static std::string
build_etag(const std::string& str)
{
    u32 hash = 0x4F7E0912;
    for (size_t i = 0; i < str.length(); i++) {
        hash += str[i] << (i & 0x0F);
    }
    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

typedef std::vector<std::string> Tokens;

static Tokens
tokenize_string(std::string str, const char* delim)
{
    std::vector<std::string> vec;
    std::string last;
    for (size_t i = 0; i < str.length(); i++) {
        if (strchr(delim, str[i]) != NULL) {
            if (last != "") {
                vec.push_back(last);
                last = "";
            }
        } else {
            last += str[i];
        }
    }
    if (last != "") {
        vec.push_back(last);
    }
    return vec;
}

static Tokens
tokenize_date(std::string date)
{
    return tokenize_string(date, ", -");
}

static Tokens
tokenize_time(std::string time)
{
    return tokenize_string(time, ":");
}

static bool
parse_digits(std::string number, const size_t max_ndigits, int& res)
{
    if (number.length() > max_ndigits) return false;
    res = 0;
    for (size_t i = 0; i < number.length(); i++) {
        if (number[i] < '0' || number[i] > '9')
            return false;
        res = res * 10 + number[i] - '0';
    }
    return true;
}

static bool
parse_month(std::string month, int& res)
{
    static const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
        "Nov", "Dec"
    };
    if (month.length() != 3)
        return false;
    for (res = 0; res < 12; res++) {
        if (month == months[res]) {
            return true;
        }
    }
    return false;
}

static bool
parse_datetime(std::string datetime, struct tm* tm_struct)
{
    Tokens toks = tokenize_date(datetime);

    if (toks.size() !=6 || toks[5] != "GMT") {
        return false;
    }
    // we'll ignore the first token. since mktime will add for us
    if (!parse_digits(toks[1], 2, tm_struct->tm_mday))
        return false;
    if (!parse_month(toks[2], tm_struct->tm_mon))
        return false;
    if (!parse_digits(toks[3], 4, tm_struct->tm_year))
        return false;
    if (tm_struct->tm_year > 100) {
        tm_struct->tm_year -= 1900;
    }
    Tokens time_toks = tokenize_time(toks[4]);
    if (time_toks.size() != 3) {
        return false;
    }
    if (!parse_digits(time_toks[0], 2, tm_struct->tm_hour)) {
        return false;
    }
    if (!parse_digits(time_toks[1], 2, tm_struct->tm_min)) {
        return false;
    }
    if (!parse_digits(time_toks[2], 2, tm_struct->tm_sec)) {
        return false;
    }
    return true;
}

bool
StaticHttpHandler::validate_client_cache(const std::string& path,
                                         struct stat64 stat,
                                         const std::string& etag,
                                         HttpRequest& request)
{
    std::string modified_since =
        request.find_header_value("If-Modified-Since");
    struct tm tm_struct;
    if (parse_datetime(modified_since, &tm_struct)) {
        struct tm gmt;
        gmtime_r(&stat.st_mtime, &gmt);
        time_t orig_mtime = mktime(&gmt);
        time_t modified_time = mktime(&tm_struct);
        if (orig_mtime > modified_time) {
            return false;
        }
        // try to check the etag
        std::string etag = request.find_header_value("If-None-Match");
        if (etag != "" && etag != build_etag(path)) {
            return false;
        }
        return true;
    }
    return false;
}

int
StaticHttpHandler::try_open_file(const std::string& path, HttpRequest& request,
                                 HttpResponse& response)
{
    int file_desc = 0;
    if (request.method() != HTTP_HEAD) {
        file_desc = ::open(path.c_str(), O_RDONLY);
        // cannot open, this is access forbidden
        if (file_desc < 0) {
            respond_error(HttpResponseStatus::kHttpResponseForbidden,
                          request, response);
        }
    }
    return file_desc;
}

static void
send_client_cache_info(HttpResponse& response, time_t* mtime, std::string etag)
{
    response.add_header("Last-Modified", build_last_modified(mtime));
    response.add_header("ETag", etag);
}

static void
send_client_data(HttpResponse& response, const HttpResponseStatus& status,
                 byte* cached_entry, int file_desc, off64_t offset,
                 off64_t length)
{
    if (cached_entry) {
        ::close(file_desc);
        response.write_data(cached_entry + offset, length);
        response.respond(status);
    } else {
        response.respond(status);
        response.write_file(file_desc, offset, length);
    }
}

void
StaticHttpHandler::respond_file_content(const std::string& path,
                                        struct stat64 stat,
                                        HttpRequest& request,
                                        HttpResponse& response)
{
    byte* cached_entry = NULL;
    off64_t file_size = -1;
    std::string range_str;
    std::string etag;
    HttpResponseStatus ret_status = HttpResponseStatus::kHttpResponseOK;
    off64_t offset = 0, length = -1;

    if (request.method() != HTTP_HEAD) {
        cached_entry = io_cache_.access_cache(path, stat.st_mtime,
                                              stat.st_size);
    }

    int file_desc = try_open_file(path, request, response);
    if (file_desc < 0) {
        goto done;
    }

    file_size = stat.st_size;
    etag = build_etag(path);

    if (validate_client_cache(path, stat, etag, request)) {
        response.respond(HttpResponseStatus::kHttpResponseNotModified);
        goto done;
    }

    range_str = request.find_header_value("Range");
    if (range_str != "") {
        // parse range
        parse_range(range_str, offset, length);
        // incomplete range, doesn't have a length
        // we'll transfer all begining from that offset
        if (length < 0) {
            length = file_size - offset;
        }
        // exceeded file size, this is invalid range
        if ((size_t) (offset + length) > file_size) {
            respond_error(
                HttpResponseStatus::kHttpResponseRequestedRangeNotSatisfiable,
                request, response);
            goto done;
        }
        response.add_header("Content-Range",
                            build_range_response(offset, length, file_size));
        ret_status = HttpResponseStatus::kHttpResponsePartialContent;
    } else {
        offset = 0;
        length = file_size;
    }
    response.set_content_length(length);
    send_client_cache_info(response, &stat.st_mtime, etag);
    if (request.method() != HTTP_HEAD) {
        send_client_data(response, ret_status, cached_entry, file_desc, offset,
                         length);
    } else {
        response.respond(HttpResponseStatus::kHttpResponseOK);
    }
done:
    delete [] cached_entry;
}

static void
add_directory_entry(std::stringstream& ss, std::string entry,
                    struct stat64* stat)
{
    if (stat == NULL) {
        ss << "<tr class=\"parent\"><td><a href=\"..\">Parent Directory</a>"
           << "</td>";
    } else if (S_ISDIR(stat->st_mode)) {
        ss << "<tr class=\"directory\">"
           << "<td><a href=\"" << entry << "/\">" << entry << "/</a></td>"
           << "<td>-</td>";
    } else if (S_ISREG(stat->st_mode)) {
        ss << "<tr class=\"regular\">"
           << "<td><a href=\"" << entry << "\">" << entry << "</a></td>"
           << "<td>" << stat->st_size << "</td>";
    }
    if (stat != NULL) {
        char time_str[MAX_TIME_LEN];
        struct tm localtime;
        localtime_r(&(stat->st_mtime), &localtime);
        strftime(time_str, MAX_TIME_LEN, "%F %T", &localtime);
        ss << "<td>" << time_str << "</td>";
    }
    ss << "</tr>" << std::endl;
}

void
StaticHttpHandler::respond_directory_list(const std::string& path,
                                          const std::string& href_path,
                                          HttpRequest& request,
                                          HttpResponse& response)
{
    DIR* dirp = opendir(path.c_str());
    if (!dirp) {
        respond_error(HttpResponseStatus::kHttpResponseForbidden,
                      request, response);
        return;
    }
    std::stringstream ss;
    ss << "<html><head><title>Directory List " << href_path << "</title>"
       << std::endl;
    if (index_page_css_ != "") {
        ss << "<link rel=\"stylesheet\" type=\"text/css\" href=\""
           << index_page_css_ << "\"/>" << std::endl;
    }
    ss << "</head><body>" << std::endl
       << "<h1>Index of " << href_path << "</h1>" << std::endl
       << "<table>" << std::endl;

    if (href_path != "/") {
        add_directory_entry(ss, "..", NULL);
    }
    dirent* ent = NULL;
    ss << "<table>";
    while ((ent = readdir(dirp))) {
        std::string ent_name = ent->d_name;
        std::string ent_path = path + "/" + ent->d_name;
        struct stat64 buf;
        if (ent_name == "." || ent_name == "..") {
            continue;
        }
        if (::stat64(ent_path.c_str(), &buf) < 0) {
            continue;
        }
        add_directory_entry(ss, ent_name, &buf);
    }
    closedir(dirp);
    ss << "</table></body></html>" << std::endl;
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
    LOG(WARNING, "%d with %s", error.status_code, request.uri().c_str());
    response.add_header("Content-Type", "text/html");

    std::stringstream ss;
    std::string path;
    int file_desc = -1;
    struct stat64 buf;

    if (error_root_ == "") {
        goto default_resp;
    }
    ss << error_root_ << "/" << error.status_code << ".html";
    path = ss.str();
    file_desc = ::open(path.c_str(), O_RDONLY);
    if (file_desc < 0) {
        goto default_resp;
    }
    if (fstat64(file_desc, &buf) < 0) {
        ::close(file_desc);
        goto default_resp;
    }
    response.set_content_length(buf.st_size);
    response.respond(error);
    response.write_file(file_desc, 0, -1);
    return;
default_resp:
    ss.clear();
    ss << "<html><head><title>" << error.reason << "</title></head><body>"
       << "<h1>" << error.status_code << " - " << error.reason << "</h1>"
       << "</body></html>" << std::endl;
    response.write_string(ss.str());
    response.respond(error);
    return;
}

void
StaticHttpHandler::handle_request(HttpRequest& request, HttpResponse& response)
{
    std::string filename = HttpRequest::url_decode(request.path());
    filename = remove_path_dots(filename);

    if (request.method() != HTTP_GET && request.method() != HTTP_POST
        && request.method() != HTTP_HEAD) {
        respond_error(HttpResponseStatus::kHttpResponseBadRequest, request,
                      response);
    }

    struct stat64 buf;
    std::string filepath = doc_root_ + filename;
    if (::stat64(filepath.c_str(), &buf) < 0) {
        LOG(DEBUG, "Cannot stat file %s", filepath.c_str());
        respond_error(HttpResponseStatus::kHttpResponseNotFound,
                      request, response);
        return;
    }

    if (S_ISREG(buf.st_mode)) {
        respond_file_content(filepath, buf, request, response);
    } else if (S_ISDIR(buf.st_mode)) {
        if (allow_index_) {
            respond_directory_list(filepath, filename, request, response);
        } else {
            respond_error(HttpResponseStatus::kHttpResponseForbidden,
                          request, response);
        }
    } else {
        respond_error(HttpResponseStatus::kHttpResponseForbidden,
                      request, response);
    }
}

}
