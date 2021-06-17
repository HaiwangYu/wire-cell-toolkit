#include "WireCellUtil/Persist.h"
#include "WireCellUtil/String.h"
#include "WireCellUtil/Logging.h"
#include "WireCellUtil/Exceptions.h"

#include "libgojsonnet.h"

#include <cstdlib>  // for getenv, see get_path()

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/filesystem.hpp>

#include <string>
#include <sstream>
#include <fstream>

using spdlog::debug;
using spdlog::error;
using spdlog::info;
using namespace std;
using namespace WireCell;

#define WIRECELL_PATH_VARNAME "WIRECELL_PATH"

static std::string file_extension(const std::string& filename)
{
    auto ind = filename.rfind(".");
    if (ind == string::npos) {
        return "";
    }
    return filename.substr(ind);
}

void WireCell::Persist::dump(const std::string& filename, const Json::Value& jroot, bool pretty)
{
    string ext = file_extension(filename);

    /// default to .json.bz2 regardless of extension.
    std::fstream fp(filename.c_str(), std::ios::binary | std::ios::out);
    boost::iostreams::filtering_stream<boost::iostreams::output> outfilt;
    if (ext == ".bz2") {
        outfilt.push(boost::iostreams::bzip2_compressor());
    }
    outfilt.push(fp);
    if (pretty) {
        Json::StyledWriter jwriter;
        outfilt << jwriter.write(jroot);
    }
    else {
        Json::FastWriter jwriter;
        outfilt << jwriter.write(jroot);
    }
}
// fixme: support pretty option for indentation
std::string WireCell::Persist::dumps(const Json::Value& cfg, bool)
{
    stringstream ss;
    ss << cfg;
    return ss.str();
}

std::string WireCell::Persist::slurp(const std::string& filename)
{
    std::string fname = resolve(filename);
    if (fname.empty()) {
        THROW(IOError() << errmsg{"no such file: " + filename + ". Maybe you need to add to WIRECELL_PATH."});
    }

    std::ifstream fstr(filename);
    std::stringstream buf;
    buf << fstr.rdbuf();
    return buf.str();
}

bool WireCell::Persist::exists(const std::string& filename) { return boost::filesystem::exists(filename); }

static std::vector<std::string> get_path()
{
    std::vector<std::string> ret;
    const char* cpath = std::getenv(WIRECELL_PATH_VARNAME);
    if (!cpath) {
        return ret;
    }
    for (auto path : String::split(cpath)) {
        ret.push_back(path);
    }
    return ret;
}

std::string WireCell::Persist::resolve(const std::string& filename)
{
    if (filename.empty()) {
        return "";
    }
    if (filename[0] == '/') {
        return filename;
    }

    std::vector<boost::filesystem::path> tocheck{
        boost::filesystem::current_path(),
    };
    for (auto pathname : get_path()) {
        tocheck.push_back(boost::filesystem::path(pathname));
    }
    for (auto pobj : tocheck) {
        boost::filesystem::path full = pobj / filename;
        if (boost::filesystem::exists(full)) {
            return boost::filesystem::canonical(full).string();
        }
    }
    return "";
}

Json::Value WireCell::Persist::load(const std::string& filename, const externalvars_t& extvar,
                                    const externalvars_t& extcode)
{
    string ext = file_extension(filename);
    if (ext == ".jsonnet") {  // use libjsonnet++ file interface
        string text = evaluate_jsonnet_file(filename, extvar, extcode);
        return json2object(text);
    }

    std::string fname = resolve(filename);
    if (fname.empty()) {
        THROW(IOError() << errmsg{"no such file: " + filename + ". Maybe you need to add to WIRECELL_PATH."});
    }

    // use jsoncpp file interface
    std::fstream fp(fname.c_str(), std::ios::binary | std::ios::in);
    boost::iostreams::filtering_stream<boost::iostreams::input> infilt;
    if (ext == ".bz2") {
        info("loading compressed json file: {}", fname);
        infilt.push(boost::iostreams::bzip2_decompressor());
    }
    infilt.push(fp);
    std::string text;
    Json::Value jroot;
    infilt >> jroot;
    // return update(jroot, extvar); fixme
    return jroot;
}

Json::Value WireCell::Persist::loads(const std::string& text, const externalvars_t& extvar,
                                     const externalvars_t& extcode)
{
    const std::string jtext = evaluate_jsonnet_text(text, extvar, extcode);
    return json2object(jtext);
}

// bundles few lines into function to avoid some copy-paste
Json::Value WireCell::Persist::json2object(const std::string& text)
{
    Json::Value res;
    stringstream ss(text);
    ss >> res;
    return res;
}

static void init_parser(JsonnetVm* parser, const Persist::externalvars_t& extvar,
                        const Persist::externalvars_t& extcode)
{
    parser = jsonnet_make();
    for (auto path : get_path()) {
        jsonnet_jpath_add(parser, const_cast<char*>(path.c_str()));
    }
    for (auto& vv : extvar) {
        jsonnet_ext_var(parser, const_cast<char*>(vv.first.c_str()), const_cast<char*>(vv.second.c_str()));
    }
    for (auto& vv : extcode) {
        jsonnet_ext_code(parser, const_cast<char*>(vv.first.c_str()), const_cast<char*>(vv.second.c_str()));
    }
}
std::string WireCell::Persist::evaluate_jsonnet_file(const std::string& filename, const externalvars_t& extvar,
                                                     const externalvars_t& extcode)
{
    std::string fname = resolve(filename);
    if (fname.empty()) {
        THROW(IOError() << errmsg{"no such file: " + filename + ", maybe you need to add to WIRECELL_PATH."});
    }

    JsonnetVm* parser;
    init_parser(parser, extvar, extcode);

    int err;
    std::string output(jsonnet_evaluate_file(parser, const_cast<char*>(fname.c_str()), &err));
    jsonnet_destroy(parser);
    if (err!=0) {
        error(output);
        THROW(ValueError() << errmsg{output});
    }
    return output;
}
std::string WireCell::Persist::evaluate_jsonnet_text(const std::string& text, const externalvars_t& extvar,
                                                     const externalvars_t& extcode)
{
    JsonnetVm* parser;
    init_parser(parser, extvar, extcode);

    int err;
    std::string output(jsonnet_evaluate_snippet(parser, "<stdin>", const_cast<char*>(text.c_str()), &err));
    jsonnet_destroy(parser);
    if (err!=0) {
        error(output);
        THROW(ValueError() << errmsg{output});
    }
    return output;
}

WireCell::Persist::Parser::Parser(const std::vector<std::string>& load_paths, const externalvars_t& extvar,
                                  const externalvars_t& extcode, const externalvars_t& tlavar,
                                  const externalvars_t& tlacode)
{
    m_jsonnet = jsonnet_make();

    // Loading: 1) cwd, 2) passed in paths 3) environment
    m_load_paths.push_back(boost::filesystem::current_path());
    for (auto path : load_paths) {
        debug("search path: {}", path);
        m_load_paths.push_back(boost::filesystem::path(path));
    }
    for (auto path : get_path()) {
        debug("search path: {}", path);
        m_load_paths.push_back(boost::filesystem::path(path));
    }
    // load paths into jsonnet backwards to counteract its reverse ordering
    for (auto pit = m_load_paths.rbegin(); pit != m_load_paths.rend(); ++pit) {
        jsonnet_jpath_add(m_jsonnet, const_cast<char*>(boost::filesystem::canonical(*pit).string().c_str()));
    }

    // external variables
    for (auto& vv : extvar) {
        jsonnet_ext_var(m_jsonnet, const_cast<char*>(vv.first.c_str()), const_cast<char*>(vv.second.c_str()));
    }
    // external code
    for (auto& vv : extcode) {
        jsonnet_ext_code(m_jsonnet, const_cast<char*>(vv.first.c_str()), const_cast<char*>(vv.second.c_str()));
    }

    // top level argument string variables
    for (auto& vv : tlavar) {
        debug("tla: {} = \"{}\"", vv.first.c_str(), vv.second.c_str());
        jsonnet_tla_var(m_jsonnet, const_cast<char*>(vv.first.c_str()), const_cast<char*>(vv.second.c_str()));
    }
    // top level argument code variables
    for (auto& vv : tlacode) {
        jsonnet_tla_code(m_jsonnet, const_cast<char*>(vv.first.c_str()), const_cast<char*>(vv.second.c_str()));
    }
}

///
/// Below is a reimplimenatiaon of the above but in class form.....
////

std::string WireCell::Persist::Parser::resolve(const std::string& filename)
{
    if (filename.empty()) {
        return "";
    }
    if (filename[0] == '/') {
        return filename;
    }

    for (auto pobj : m_load_paths) {
        boost::filesystem::path full = pobj / filename;
        if (boost::filesystem::exists(full)) {
            return boost::filesystem::canonical(full).string();
        }
    }
    return "";
}

Json::Value WireCell::Persist::Parser::load(const std::string& filename)
{
    std::string fname = resolve(filename);
    if (fname.empty()) {
        THROW(IOError() << errmsg{"no such file: " + filename + ". Maybe you need to add to WIRECELL_PATH."});
    }
    string ext = file_extension(filename);

    if (ext == ".jsonnet" or ext.empty()) {  // use libjsonnet++ file interface
        int err;
        std::string output(jsonnet_evaluate_file(m_jsonnet, const_cast<char*>(fname.c_str()), &err));
        if (err!=0) {
            error(output);
            THROW(ValueError() << errmsg{output});
        }
        return json2object(output);
    }

    // also support JSON, possibly compressed

    // use jsoncpp file interface
    std::fstream fp(fname.c_str(), std::ios::binary | std::ios::in);
    boost::iostreams::filtering_stream<boost::iostreams::input> infilt;
    if (ext == ".bz2") {
        info("loading compressed json file: {}", fname);
        infilt.push(boost::iostreams::bzip2_decompressor());
    }
    infilt.push(fp);
    std::string text;
    Json::Value jroot;
    infilt >> jroot;
    // return update(jroot, extvar); fixme
    return jroot;
}

Json::Value WireCell::Persist::Parser::loads(const std::string& text)
{
    int err;
    std::string output(jsonnet_evaluate_snippet(m_jsonnet, "<stdin>", const_cast<char*>(text.c_str()), &err));
    if (err!=0) {
        error(output);
        THROW(ValueError() << errmsg{output});
    }
    return json2object(output);
}
