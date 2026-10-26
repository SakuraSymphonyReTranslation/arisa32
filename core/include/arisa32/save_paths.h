#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

namespace dc4 {
inline std::string save_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){return char(std::tolower(c));});
    return value;
}
inline std::string native_save_prefix(uint32_t version) {
    return version==0x9969 ? "DC2PC" : version==0xAAB6 ? "DC4" : version==0xABB6 ? "DC4PHDL" : "";
}
// DC2PC has quick 00, latest-copy 01, then three regular rows on page 1
// and five rows per later page, with an eight-record page stride.
inline int native_save_number(uint32_t version,int logical) {
    if(version!=0x9969)return logical+2;
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
    if(native && version==0x9969) {
        const int physical=logical;
        if(physical==0)return 0;
        if(physical>=2 && physical<=4)logical=physical-1;
        else if(physical>=8 && physical%8<=4)logical=4+(physical/8-1)*5+physical%8;
        else return -1;
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
inline std::filesystem::path latest_save(const std::filesystem::path& directory, uint32_t version) {
    std::array<std::filesystem::path,101> native{},legacy{};
    std::error_code ec;
    for(std::filesystem::directory_iterator i(directory,ec),end;!ec&&i!=end;i.increment(ec)) {
        if(!i->is_regular_file(ec))continue;
        const auto filename=i->path().filename().string();
        const int slot=save_slot_number(filename,version);
        if(slot<1 && !(version==0x9969 && save_lower(filename)=="dc2pc01.dat"))continue;
        if(slot<1) {native[0]=i->path();continue;}
        (save_lower(i->path().extension().string())==".dat"?native:legacy)[slot]=i->path();
    }
    std::filesystem::path result;auto latest=std::filesystem::file_time_type::min();
    for(int i=0;i<=100;++i) {const auto& path=native[i].empty()?legacy[i]:native[i];if(path.empty())continue;
        const auto time=std::filesystem::last_write_time(path,ec);
        if(!ec&&(result.empty()||time>latest)){result=path;latest=time;}ec.clear();}
    return result;
}
} // namespace dc4
