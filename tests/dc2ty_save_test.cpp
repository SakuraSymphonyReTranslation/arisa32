#include "arisa32/scene.h"
#include "arisa32/save_paths.h"
#include "arisa32/audio.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
int failures=0;
void check(bool ok,const char* expression,int line) {
    if (!ok) { std::cerr<<line<<": "<<expression<<'\n';++failures; }
}
#define CHECK(value) check(bool(value),#value,__LINE__)
void put32(std::vector<uint8_t>& b,size_t at,uint32_t value) {
    for (int i=0;i<4;++i)b.at(at+i)=uint8_t(value>>(8*i));
}
uint32_t get32(const std::vector<uint8_t>& b,size_t at) {
    uint32_t value=0;for (int i=0;i<4;++i)value|=uint32_t(b.at(at+i))<<(8*i);return value;
}
void write(const fs::path& path,const std::vector<uint8_t>& b) {
    fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);
    out.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size()));
    if (!out)throw std::runtime_error("cannot write save fixture");
}
std::vector<uint8_t> read(const fs::path& path) {
    std::ifstream in(path,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};
}
std::string name(const std::vector<uint8_t>& b,size_t at) {
    return std::string(reinterpret_cast<const char*>(b.data()+at));
}
struct Script {
    std::vector<uint8_t> code{0x69,0x5F};
    std::vector<uint32_t> labels;
    void pair(uint8_t op,uint8_t a=0,uint8_t b=0) { code.insert(code.end(),{op,a,b}); }
    void marker(uint8_t op=3) {const auto id=labels.size();labels.push_back(uint32_t(code.size()));pair(op,uint8_t(id),uint8_t(id>>8));}
    void words(uint8_t op,uint16_t a,uint16_t b,uint16_t c,uint16_t d) {
        code.push_back(op);for (const auto n:{a,b,c,d}) {code.push_back(uint8_t(n));code.push_back(uint8_t(n>>8));}
    }
    void str(uint8_t op,const std::string& value,uint8_t slot=0) {
        code.push_back(op);if(op>=0x29&&op<=0x2C)code.push_back(slot);
        for (unsigned char c:value)code.push_back(op>=0x4A&&op<=0x4D?uint8_t(c-0x20):c);
        code.push_back(0);
    }
    std::vector<uint8_t> bytes() const {
        std::vector<uint8_t> b(4+labels.size()*4);put32(b,0,uint32_t(labels.size()));
        for (size_t i=0;i<labels.size();++i)put32(b,4+i*4,labels[i]+3);
        b.insert(b.end(),code.begin(),code.end());b.push_back(0);return b;
    }
};
void image(const fs::path& path,int w,int h,std::array<uint8_t,3> color) {
    std::vector<uint8_t> b{'C','R','X','G',0,0,0,0};
    for (int n:{w,h,1,0,0,0}) {b.push_back(uint8_t(n));b.push_back(uint8_t(n>>8));}
    std::vector<uint8_t> pixels;for(int i=0;i<w*h;++i)pixels.insert(pixels.end(),{color[2],color[1],color[0]});
    for(size_t i=0;i<pixels.size();i+=8) {b.push_back(255);b.insert(b.end(),pixels.begin()+i,pixels.begin()+std::min(i+8,pixels.size()));}
    write(path,b);
}
std::vector<uint8_t> snapshot(dc4scene::SceneRunner& scene) {
    std::vector<uint8_t> bytes;std::string error;
    if(!scene.capture_state(&bytes,&error))throw std::runtime_error(error);return bytes;
}
std::vector<uint8_t> native(dc4scene::SceneRunner& scene) {
    std::vector<uint8_t> bytes;std::string error;
    if(!scene.capture_pc_save(&bytes,&error))throw std::runtime_error(error);return bytes;
}
void restore(dc4scene::SceneRunner& scene,const std::vector<uint8_t>& bytes) {
    std::string error;if(!scene.restore_pc_save(bytes,&error))throw std::runtime_error(error);
}
void next(dc4scene::SceneRunner& scene,const char* line) {
    CHECK(scene.advance_to_next_text());CHECK(scene.current_line()==line);scene.update(1000);
}
void synthetic(const fs::path& root) {
    image(root/"GRP/bg/room.CRX",64,36,{30,50,80});
    image(root/"GRP/bst/body.CRX",32,24,{190,20,30});
    image(root/"GRP/bst/face.CRX",8,8,{20,200,30});
    image(root/"GRP/system/chips.CRX",64,64,{20,30,220});
    Script s;
    s.str(0x30,"bg\\room");s.words(0x64,120,70,0,0);s.words(0x65,150,10,12,16);
    s.str(0x2B,"bst\\body");s.str(0x2C,"bst\\face");s.str(0x31,"system\\chips");
    s.words(0x53,80,90,4,6);s.words(0x52,20,24,6,99);s.pair(0x0D,5);
    s.marker();s.str(0x4B,"Arisa");s.str(0x4C,"FIRST");s.pair(7);
    s.marker();s.str(0x4B,"Yume");s.str(0x4C,"SECOND");s.pair(7);
    s.marker(4);s.pair(8,1);s.str(0x44,"left");s.str(0x4D,"LEFT");s.str(0x44,"right");s.str(0x4D,"RIGHT");s.pair(9);
    const auto choice_resume=s.code.size();
    s.str(0x35,"left");s.str(0x4C,"LEFTBRANCH");s.str(0x35,"right");s.str(0x4C,"RIGHTBRANCH");
    write(root/"MES/hyd_0611_a1_avn.mes",s.bytes());
    Script ui;ui.marker();write(root/"MES/start.mes",ui.bytes());write(root/"MES/main.mes",ui.bytes());
    dc4scene::SceneRunner scene(root.string());
    CHECK(scene.load_ui_script((root/"MES/start.mes").string()));
    CHECK(scene.load_script((root/"MES/hyd_0611_a1_avn.mes").string()));next(scene,"FIRST");
    scene.set_variable(9,20,3);scene.set_variable(9,40,12);scene.set_variable(100,7,1);
    auto first=native(scene);
    CHECK(first.size()==0x2944&&get32(first,0)==1);
    CHECK(name(first,4)=="hyd_0611_a1_avn"&&name(first,0x84)=="bg\\room");
    CHECK(name(first,0x104)=="bst\\body"&&name(first,0x384)=="bst\\face");
    CHECK(name(first,0x584)=="system\\chips");
    CHECK(name(first,0x28DC)==std::string("\x8E\x6E\x82\xDC\x82\xE8\x82\xCC\x8E\x6E\x82\xDC\x82\xE8"));
    CHECK(get32(first,0x810)==s.labels[1]&&get32(first,0x814)==0&&get32(first,0x81C)==7);
    CHECK(first[0x1388]==3&&first[0x1389]>=1&&first[0x1389]<=12&&first[0x138A]>=1&&first[0x138A]<=31);
    CHECK(get32(first,0x15D4)==0&&get32(first,0x15D8)==1);
    CHECK(get32(first,0x1D5C)==80&&get32(first,0x1D6C)==76&&get32(first,0x1D70)==84);
    CHECK(get32(first,0x1D74)==16&&get32(first,0x1D78)==18);
    scene.set_message_visible(false);scene.draw(-1);const auto frame=scene.frame();
    const auto exact=snapshot(scene);CHECK(get32(exact,8)==12);
    dc4scene::SceneRunner imported(root.string());restore(imported,first);
    imported.set_message_visible(false);imported.draw(-1);CHECK(imported.frame()==frame);
    CHECK(imported.current_line()=="FIRST"&&imported.speaker()=="Arisa");
    CHECK(imported.variable(9,40)==12&&imported.sprite(0).scale==1.5f);
    CHECK(imported.sprite(0).cur_x==120&&imported.sprite(0).cur_y==70);
    CHECK(imported.restore_state(exact));imported.draw(-1);CHECK(imported.frame()==frame);
    CHECK(snapshot(imported)==exact);
    const auto file=root/"saves/dc2ty02.dat",latest=root/"saves/DC2TY01.DAT";
    write(latest,first);CHECK(scene.save_state(file.string()));
    CHECK(read(file)==read(latest)&&fs::exists(file.string()+".arisa")&&fs::exists(latest.string()+".arisa"));
    const auto companion=read(file.string()+".arisa");CHECK(companion.size()>40&&std::string(companion.begin(),companion.begin()+7)=="ARISAPC");
    next(scene,"SECOND");CHECK(scene.load_state(file.string()));CHECK(snapshot(scene)==exact);
    auto foreign=first;put32(foreign,0x814,1);put32(foreign,0x810,s.labels[2]);foreign[0xFF0+9*100+40]=27;
    write(file,foreign);scene.set_variable(100,7,4);CHECK(scene.load_state(file.string()));
    CHECK(scene.current_line()=="SECOND"&&scene.variable(9,40)==27&&scene.variable(100,7)==4);
    const auto latest_before=read(latest);CHECK(scene.save_state((root/"saves/dc2ty00.dat").string()));CHECK(read(latest)==latest_before);
    CHECK(!scene.advance_to_next_text()&&scene.waiting_for_choice());const auto choice=native(scene);
    CHECK(get32(choice,0x810)==choice_resume&&get32(choice,0x814)==2&&get32(choice,0x81C)==9);
    restore(imported,choice);CHECK(imported.waiting_for_choice()&&imported.choices().size()==2);
    CHECK(imported.current_line()=="SECOND");CHECK(imported.choose(1));next(imported,"RIGHTBRANCH");
    const auto stable=snapshot(scene);
    for(int damage=0;damage<12;++damage) {
        auto broken=first;
        if(damage==0)broken.pop_back();
        if(damage==1)put32(broken,0,2);
        if(damage==2)std::fill(broken.begin()+4,broken.begin()+132,'A');
        if(damage==3)put32(broken,0x814,65535);
        if(damage==4)put32(broken,0x15D8,61);
        if(damage==5)put32(broken,0x1D74,0xFFFFFFFF);
        if(damage==6)put32(broken,0x1D5C,0x7FFFFFFF);
        if(damage==7)put32(broken,0x80C,0x1FFFF);
        if(damage==8)put32(broken,0x15D4,1);
        if(damage==9)put32(broken,0x14B8,1);
        if(damage==10)put32(broken,0x1D68,1);
        if(damage==11)put32(broken,0x14C0,1);
        std::string error;CHECK(!scene.restore_pc_save(broken,&error));CHECK(!error.empty());CHECK(snapshot(scene)==stable);
    }
    std::vector<uint8_t> names(1112);put32(names,0,6);put32(names,772,1);put32(names,968,1);
    const std::string music="BGM\\TRACK",effect="SE\\LOOP";
    std::copy(music.begin(),music.end(),names.begin()+776);std::copy(effect.begin(),effect.end(),names.begin()+972);
    write(root/"GRP/NAMES.DAT",names);
    std::vector<uint8_t> pcm(44);std::copy_n("XPCM",4,pcm.begin());put32(pcm,4,16);
    pcm[12]=1;pcm[14]=1;put32(pcm,16,44100);put32(pcm,20,44100);pcm[24]=1;pcm[26]=8;
    for(size_t i=28;i<pcm.size();++i)pcm[i]=uint8_t(129+i-28);
    write(root/"BGM/TRACK.pcm",pcm);write(root/"SE/LOOP.pcm",pcm);
    dc4::AudioEngine mixer;dc4scene::SceneRunner sounding(root.string());sounding.set_audio(&mixer,nullptr);
    auto with_audio=first;put32(with_audio,0x804,0);put32(with_audio,0x808,0);put32(with_audio,0x80C,0x10000);
    restore(sounding,with_audio);CHECK(mixer.channel_playing(0)&&mixer.channel_playing(2));
    const auto audio_native=native(sounding);
    CHECK(get32(audio_native,0x804)==0&&get32(audio_native,0x808)==0&&get32(audio_native,0x80C)==0x10000);
    std::array<int16_t,18> samples{};mixer.render_pcm(samples.data(),5);
    const auto audio_exact=snapshot(sounding);const auto audio_file=root/"audio/dc2ty02.dat";
    CHECK(sounding.save_state(audio_file.string()));mixer.render_pcm(samples.data(),9);
    CHECK(sounding.load_state(audio_file.string()));
    const auto restored_audio=mixer.capture_snapshot();
    CHECK(restored_audio.channels[0].playing&&restored_audio.channels[2].playing);
    CHECK(restored_audio.channels[0].position_frames==5&&restored_audio.channels[2].position_frames==5);
    CHECK(snapshot(sounding)==audio_exact);
    auto translated=s;translated.code.insert(translated.code.begin()+s.labels[1],{0x4C,0x21,0});
    for(size_t i=1;i<translated.labels.size();++i)translated.labels[i]+=3;
    write(root/"MES/hyd_0611_a1_avn.mes",translated.bytes());
    restore(imported,foreign);CHECK(imported.current_line()=="SECOND");
    CHECK(get32(native(imported),0x810)==translated.labels[2]);
}
void originals(const fs::path& data,const fs::path& output) {
    for(const auto filename:{"dc2ty00.dat","dc2ty01.dat","dc2ty02.dat"}) {
        dc4scene::SceneRunner scene(data.string());const auto original=read(data/"Savegame"/filename);
        if(original.empty())continue;
        restore(scene,original);const auto line=scene.current_line();
        scene.set_message_visible(false);scene.draw(-1);const auto frame=scene.frame();
        const auto bytes=native(scene);restore(scene,bytes);scene.set_message_visible(false);scene.draw(-1);
        CHECK(scene.current_line()==line&&scene.frame()==frame);
        CHECK(get32(bytes,0x814)==get32(original,0x814)&&get32(bytes,0x808)==get32(original,0x808));
        const auto script=read(data/"MES"/scene.script_name());
        CHECK(script.at(4+get32(script,0)*4+get32(bytes,0x810)-3)==get32(bytes,0x81C));
        if(!output.empty()) {write(output/filename,bytes);CHECK(scene.save_state((output/(std::string("roundtrip-")+filename)).string()));}
        std::cout<<filename<<": "<<scene.script_name()<<", marker "<<get32(bytes,0x814)<<'\n';
    }
    dc4scene::SceneRunner scene(data.string());CHECK(scene.load_script((data/"MES/hyd_0611_a2_com.mes").string()));
    uint16_t marker=0,dialogue=0;scene.set_observer([&](const dc4::MesCommand& c,const std::string&) {
        if(c.op==3)marker=uint16_t(c.p1|uint16_t(c.p2)<<8);if(c.op==0x4C)dialogue=marker;
    });
    for(int i=0;i<300&&dialogue<85&&!scene.scene_ended();++i) {
        if(scene.waiting_for_time())scene.finish_script_wait();if(!scene.pending_movie().empty())scene.finish_movie();
        scene.advance_to_next_text();scene.update(1000);
    }
    CHECK(dialogue==85);scene.set_message_visible(false);scene.draw(-1);const auto frame=scene.frame();
    const auto bytes=native(scene);restore(scene,bytes);scene.set_message_visible(false);scene.draw(-1);CHECK(scene.frame()==frame);
    const auto script=read(data/"MES"/scene.script_name());
    CHECK(script.at(4+get32(script,0)*4+get32(bytes,0x810)-3)==7);
    CHECK(scene.sprite(0).image=="bst\\yme_1d_013_b"&&scene.sprite(0).canvas_w==1260);
    if(!output.empty())write(output/"marker85.dat",bytes);
}
}
int main(int argc,char** argv) {
    const auto root=fs::temp_directory_path()/("arisa32-dc2ty-save-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        synthetic(root);
        const char* data=argc>1?argv[1]:std::getenv("DC2TY_DATA");
        if(data&&*data)originals(data,argc>2?fs::path(argv[2]):fs::path{});
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';++failures;}
    fs::remove_all(root);std::cout<<"DC2TY save failures: "<<failures<<'\n';return failures?1:0;
}
