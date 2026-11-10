#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace dc4 {
inline std::string save_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){return char(std::tolower(c));});
    return value;
}
inline std::string native_save_prefix(uint32_t version) {
    return version==0x5F69 ? "dc2ty" : version==0x9969 ? "DC2PC" : version==0x9D72 ? "DC2DM" : version==0xAAB6 ? "DC4" : version==0xABB6 ? "DC4PHDL" : "";
}
// DC2PC/TY have quick 00, latest-copy 01, then three regular rows on page 1
// and five rows per later page, with an eight-record page stride.
inline int native_save_number(uint32_t version,int logical) {
    if(version==0x9D72)return logical==0 ? 1 : logical+3;
    if(version!=0x9969 && version!=0x5F69)return logical+2;
    if(logical==0)return 0;
    return logical<=3?logical+1:8+((logical-4)/5)*8+(logical-4)%5;
}
// Logical zero is quicksave, 1..100 are ordinary slots. Reserved records,
// companions and system.dat never count as a regular slot.
inline int save_slot_number(const std::string& filename, uint32_t version) {
    const auto name=save_lower(filename), prefix=save_lower(native_save_prefix(version));
    if(name=="quicksave.dc4sav") return 0;
    const bool legacy=name.rfind("slot",0)==0 && name.size()>11 && name.substr(name.size()-7)==".dc4sav";
    const bool native=!prefix.empty() && name.rfind(prefix,0)==0 && name.size()>=prefix.size()+6 && name.substr(name.size()-4)==".dat";
    if(!legacy && !native) return -1;
    const auto digits=name.substr(legacy?4:prefix.size(),name.size()-(legacy?11:prefix.size()+4));
    if(digits.empty() || digits.size()>3 || !std::all_of(digits.begin(),digits.end(),[](unsigned char c){return std::isdigit(c)!=0;})) return -1;
    int logical=std::stoi(digits);
    if(native && (version==0x9969 || version==0x5F69)) {
        const int physical=logical;
        if(physical==0)return 0;
        if(physical>=2 && physical<=4)logical=physical-1;
        else if(physical>=8 && physical%8<=4)logical=4+(physical/8-1)*5+physical%8;
        else return -1;
    } else if(native && version==0x9D72) {
        if(logical==1)return 0;
        if(logical<4)return -1;
        logical-=3;
    } else if(native)logical-=2;
    return logical>=0 && logical<=100 ? logical : -1;
}
inline std::filesystem::path save_target(const std::filesystem::path& directory, uint32_t version, int logical) {
    const auto prefix=native_save_prefix(version);
    std::string filename;
    if(prefix.empty()) filename=logical==0?"quicksave.dc4sav":"slot"+std::to_string(logical)+".dc4sav";
    else {std::ostringstream out;out<<prefix<<std::setw(2)<<std::setfill('0')<<native_save_number(version,logical)<<".dat";filename=out.str();}
    std::error_code ec;
    for(std::filesystem::directory_iterator i(directory,ec),end;!ec&&i!=end;i.increment(ec))
        if(i->is_regular_file(ec)&&save_lower(i->path().filename().string())==save_lower(filename))return i->path();
    return directory/filename;
}
inline std::filesystem::path resolve_save(const std::filesystem::path& directory, uint32_t version, int logical) {
    const auto target=save_target(directory,version,logical);std::error_code ec;
    if(std::filesystem::is_regular_file(target,ec))return target;
    const auto legacy=directory/(logical==0?"quicksave.dc4sav":"slot"+std::to_string(logical)+".dc4sav");
    return std::filesystem::is_regular_file(legacy,ec)?legacy:target;
}
inline int64_t dc2ty_save_minute(const std::filesystem::path& path, std::filesystem::file_time_type modified) {
    const auto fallback = std::chrono::duration_cast<std::chrono::minutes>(
        (modified - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now()).time_since_epoch()).count();
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if (in.tellg() != 0x2944) return fallback;
    std::array<unsigned char,4> header{};
    in.seekg(0); in.read(reinterpret_cast<char*>(header.data()),header.size());
    if (header != std::array<unsigned char,4>{1,0,0,0}) return fallback;
    std::array<unsigned char,5> fields{};
    in.seekg(0x1389); in.read(reinterpret_cast<char*>(fields.data()),fields.size());
    if (!in) return fallback;
    std::tm date{};
    date.tm_mon = int(fields[0])-1; date.tm_mday = fields[1];
    date.tm_hour = fields[2]; date.tm_min = fields[3]; date.tm_year = 100+fields[4]; date.tm_isdst = -1;
    const auto expected = date;
    const auto stamp = std::mktime(&date);
    if (stamp == std::time_t(-1) || date.tm_year != expected.tm_year || date.tm_mon != expected.tm_mon ||
        date.tm_mday != expected.tm_mday || date.tm_hour != expected.tm_hour || date.tm_min != expected.tm_min) return fallback;
    return int64_t(stamp)/60;
}
inline std::filesystem::path latest_save(const std::filesystem::path& directory, uint32_t version) {
    std::array<std::filesystem::path,101> native{},legacy{};
    std::error_code ec;
    for(std::filesystem::directory_iterator i(directory,ec),end;!ec&&i!=end;i.increment(ec)) {
        if(!i->is_regular_file(ec))continue;
        const auto filename=i->path().filename().string();
        const int slot=save_slot_number(filename,version);
        if(slot<1 && !(version==0x9969 && save_lower(filename)=="dc2pc01.dat") &&
           !(version==0x5F69 && save_lower(filename)=="dc2ty01.dat") &&
           !(version==0x9D72 && save_lower(filename)=="dc2dm03.dat"))continue;
        if(slot<1) {native[0]=i->path();continue;}
        (save_lower(i->path().extension().string())==".dat"?native:legacy)[slot]=i->path();
    }
    std::filesystem::path result;auto latest=std::filesystem::file_time_type::min();
    int64_t latest_minute=0;
    for(int i=0;i<=100;++i) {const auto& path=native[i].empty()?legacy[i]:native[i];if(path.empty())continue;
        const auto time=std::filesystem::last_write_time(path,ec);
        if(ec){ec.clear();continue;}
        const auto minute=version==0x5F69?dc2ty_save_minute(path,time):0;
        const bool newer=version==0x5F69 && minute!=latest_minute?minute>latest_minute:time>latest;
        if(result.empty()||newer){result=path;latest=time;latest_minute=minute;}}
    return result;
}
inline std::filesystem::path auto_save_target(const std::filesystem::path& directory, uint32_t version) {
    if(version==0x9D72) {
        std::error_code ec;
        for(std::filesystem::directory_iterator i(directory,ec),end;!ec&&i!=end;i.increment(ec))
            if(i->is_regular_file(ec)&&save_lower(i->path().filename().string())=="dc2dm02.dat")return i->path();
        return directory/"DC2DM02.dat";
    }
    return directory/"autosave.dc4sav";
}
inline std::filesystem::path auto_save(const std::filesystem::path& directory, uint32_t version) {
    const auto native=auto_save_target(directory,version), legacy=directory/"autosave.dc4sav";
    std::error_code ec;
    return std::filesystem::is_regular_file(native,ec) || !std::filesystem::is_regular_file(legacy,ec) ? native : legacy;
}
} // namespace dc4
