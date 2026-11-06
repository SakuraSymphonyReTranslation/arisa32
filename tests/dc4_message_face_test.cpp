#include "arisa32/scene.h"
#include "legacy_snapshot_fixture.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
int failures = 0;
void check(bool value, const char* expression, int line) {
    if (!value) { std::cerr << line << ": " << expression << '\n'; ++failures; }
}
#define CHECK(value) check(bool(value), #value, __LINE__)
using Color = std::array<uint8_t, 3>;
const Color red{210,70,40}, blue{30,90,220}, green{20,200,60}, black{0,0,0};
void write(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!out) throw std::runtime_error("cannot write fixture");
}
void u32(std::vector<uint8_t>& bytes, size_t at, uint32_t value) {
    for (int i=0;i<4;++i) bytes[at+i]=uint8_t(value>>(i*8));
}
struct Script {
    std::vector<uint8_t> bytes{1,0,0,0,3,0,0,0,0,0,0xB6,0xAA,0};
    explicit Script(uint8_t version) { bytes[11]=version; }
    void words(uint8_t op, uint16_t a, uint16_t b, uint16_t c=0, uint16_t d=0) {
        bytes.push_back(op);
        for (uint16_t v : {a,b,c,d}) { bytes.push_back(uint8_t(v));bytes.push_back(uint8_t(v>>8)); }
    }
    void string(uint8_t op, const std::string& value) {
        bytes.push_back(op);bytes.insert(bytes.end(),value.begin(),value.end());bytes.push_back(0);
    }
    void slot(uint8_t index, const std::string& value) { bytes.push_back(0x3C);string(index,value); }
    void text(const std::string& value) {
        bytes.push_back(0x6B);
        for (unsigned char c : value) bytes.push_back(uint8_t(c-0x20));
        bytes.push_back(0);
    }
    void save(const fs::path& path) { bytes.push_back(0);write(path,bytes); }
};
std::vector<uint8_t> crx(int x, int y, int w, int h, Color color) {
    std::vector<uint8_t> bytes{'C','R','X','G'};
    for (uint16_t v : {uint16_t(x),uint16_t(y),uint16_t(w),uint16_t(h),uint16_t(1),uint16_t(0),uint16_t(0),uint16_t(0)}) {
        bytes.push_back(uint8_t(v));bytes.push_back(uint8_t(v>>8));
    }
    const int stride=(w*3+3)&~3;
    std::vector<uint8_t> pixels(size_t(stride)*h,0);
    for (int row=0;row<h;++row) for(int col=0;col<w;++col)
        for(int c=0;c<3;++c) pixels[size_t(row)*stride+col*3+c]=color[2-c];
    for (size_t i=0;i<pixels.size();i+=8) {
        bytes.push_back(255);
        bytes.insert(bytes.end(),pixels.begin()+i,pixels.begin()+std::min(i+8,pixels.size()));
    }
    return bytes;
}
void archive(const fs::path& path, int height, const std::vector<uint8_t>& image) {
    std::vector<uint8_t> bytes(48,0);
    std::memcpy(bytes.data(),"CRXB",4);u32(bytes,4,1);u32(bytes,8,1);
    bytes[12]=100;bytes[14]=uint8_t(height);bytes[15]=uint8_t(height>>8);
    u32(bytes,16,48);std::memcpy(bytes.data()+24,"face.CRX",8);
    bytes.insert(bytes.end(),image.begin(),image.end());write(path,bytes);
}
Color pixel(const dc4scene::SceneRunner& scene,int x,int y) {
    const auto* p=scene.frame().data()+(size_t(y)*1280+x)*4;
    return {p[0],p[1],p[2]};
}
void next(dc4scene::SceneRunner& scene,const char* text) {
    CHECK(scene.advance_to_next_text());CHECK(scene.current_line()==text);scene.draw(-1);
}
std::vector<uint8_t> without_faces(std::vector<uint8_t> state) {
    snapshot_fixture::strip_idle_v11_motion(state);
    auto integer=[&](size_t p) { uint32_t v=0;for(int i=0;i<4;++i)v|=uint32_t(state[p+i])<<(8*i);return v; };
    size_t at=24;
    auto string=[&] { at+=4+integer(at); };
    string();at+=16;
    at+=4+integer(at)*12;
    string();const auto choices=integer(at);at+=4;
    for(uint32_t i=0;i<choices;++i) { string();string(); }
    at+=3;string();string();at+=31;
    const auto layers=integer(at);at+=4;
    for(uint32_t i=0;i<layers;++i) { at+=4;string(); }
    CHECK(integer(at)==16);at+=4;
    for(int i=0;i<8;++i) { string();string();at+=33; }
    size_t end=at;
    for(int i=0;i<2;++i) {
        end+=4+integer(end);end+=4+integer(end);end+=33;
    }
    std::vector<uint8_t> blank(82,0);
    for(size_t i : {0u,41u}) {
        const double one=1;std::memcpy(blank.data()+i+8,&one,8);
        u32(blank,i+16,512);u32(blank,i+20,360);u32(blank,i+24,255);
    }
    state.erase(state.begin()+at,state.begin()+end);state.insert(state.begin()+at,blank.begin(),blank.end());
    u32(state,8,9);u32(state,12,uint32_t(state.size()-24));
    uint64_t hash=14695981039346656037ULL;
    for(size_t i=24;i<state.size();++i) hash=(hash^state[i])*1099511628211ULL;
    for(int i=0;i<8;++i) state[16+i]=uint8_t(hash>>(8*i));
    return state;
}
void run(const fs::path& root,uint8_t version) {
    archive(root/"GRP/FACE/portrait.CRM",225,crx(40,10,30,60,red));
    archive(root/"GRP/BST/portrait.CRM",225,crx(40,10,30,60,black));
    archive(root/"GRP/FACE/short.CRM",150,crx(40,10,30,60,green));
    write(root/"GRP/FACE/glasses.CRX",crx(45,20,10,10,blue));
    write(root/"GRP/BG/black.CRX",crx(0,0,16,9,black));
    Script start(version);start.words(0x8D,0,495);start.save(root/"MES/start.mes");
    Script script(version);
    script.text("NONE");
    script.slot(31,"FACE\\face,FACE\\portrait");
    script.slot(32,"FACE\\glasses");script.string(0x64,".");
    script.slot(1,"BG\\black");script.string(0x49,"BG\\black");
    script.words(0x9D,0,160,512,288);
    script.string(0x4C,"");
    script.bytes.insert(script.bytes.end(),{3,0,0});script.text("FACE");
    script.text("NARRATOR");
    script.string(0x64,"-");script.slot(31,"--");script.text("CLEARED");
    script.words(0x8D,20,400,99,99);
    script.slot(31,"FACE\\face,FACE\\short");script.slot(32,"FACE\\glasses");
    script.string(0x64,"");script.text("SHORT");
    script.slot(31,"FACE\\missing");script.slot(32,"-");script.text("MISSING");
    script.slot(31,"FACE\\face,FACE\\portrait");script.text("BEFORE_JUMP");
    script.string(0x54,"LAND");script.slot(31,"FACE\\face,FACE\\short");
    script.string(0x4B,"LAND");script.text("JUMPED");
    script.save(root/"MES/face.mes");
    dc4scene::SceneRunner scene(root.string());
    CHECK(scene.load_sheets((root/"MES/start.mes").string()));
    CHECK(scene.load_script((root/"MES/face.mes").string()));
    next(scene,"NONE");CHECK(pixel(scene,42,506)==black);
    next(scene,"FACE");CHECK(pixel(scene,42,506)==red);CHECK(pixel(scene,46,516)==blue);
    CHECK(scene.sprite(8).visible);CHECK(scene.sprite(9).visible);
    CHECK(scene.sprite(8).cur_x==0);CHECK(scene.sprite(8).cur_y==495);
    CHECK(scene.sprite(8).y==0);CHECK(scene.sprite(8).canvas_h==225);
    const auto actual=pixel(scene,42,506);
    if(actual!=red) std::cerr<<"face pixel "<<int(actual[0])<<','<<int(actual[1])<<','<<int(actual[2])
        <<"; origin "<<scene.sprite(8).cur_x<<','<<scene.sprite(8).cur_y
        <<"; crop center "<<scene.sprite(8).frame_cx<<','<<scene.sprite(8).frame_cy<<'\n';
    CHECK(pixel(scene,42,504)==black);CHECK(pixel(scene,42,565)==black);
    CHECK(scene.camera_scale()==1.6f);
    const auto visible=scene.frame();
    scene.set_message_visible(false);scene.draw(-1);CHECK(pixel(scene,42,506)==black);
    scene.set_message_visible(true);scene.draw(-1);CHECK(scene.frame()==visible);
    std::vector<uint8_t> state,native;std::string error;
    CHECK(scene.capture_state(&state,&error));
    dc4scene::SceneRunner restored(root.string());
    CHECK(restored.restore_state(state,&error));restored.draw(-1);CHECK(restored.frame()==visible);
    CHECK(restored.restore_state(without_faces(state),&error));restored.draw(-1);CHECK(restored.frame()==visible);
    if (!error.empty()) std::cerr<<error<<'\n';
    CHECK(scene.capture_pc_save(&native,&error));
    if (!error.empty()) std::cerr<<error<<'\n';
    CHECK(native.size()==0x56FE4);
    CHECK(restored.restore_pc_save(native,&error));restored.draw(-1);CHECK(restored.frame()==visible);
    if (!error.empty()) std::cerr<<error<<'\n';
    next(scene,"NARRATOR");CHECK(pixel(scene,42,506)==black);CHECK(pixel(scene,46,516)==blue);
    next(restored,"NARRATOR");CHECK(restored.frame()==scene.frame());
    next(scene,"CLEARED");CHECK(pixel(scene,46,516)==black);
    next(scene,"SHORT");CHECK(pixel(scene,62,486)==green);CHECK(pixel(scene,66,496)==blue);
    CHECK(pixel(scene,62,484)==black);
    CHECK(scene.capture_state(&state,&error));CHECK(restored.restore_state(state,&error));
    restored.draw(-1);CHECK(restored.frame()==scene.frame());
    next(scene,"MISSING");CHECK(!scene.sprite(8).visible);CHECK(!scene.sprite(9).visible);
    next(scene,"BEFORE_JUMP");
    CHECK(scene.capture_state(&state,&error));CHECK(restored.restore_state(without_faces(state),&error));
    restored.draw(-1);CHECK(restored.frame()==scene.frame());
    next(scene,"JUMPED");CHECK(!scene.sprite(8).visible);
    CHECK(scene.capture_state(&state,&error));CHECK(restored.restore_state(without_faces(state),&error));
    restored.draw(-1);CHECK(restored.frame()==scene.frame());CHECK(!restored.sprite(8).visible);
}
void original_scene(const fs::path& data) {
    dc4scene::SceneRunner scene(data.string());
    CHECK(scene.load_sheets((data/"MES/start.mes").string()));
    CHECK(scene.load_ui_script((data/"MES/win_size.mes").string()));
    CHECK(scene.load_script((data/"MES/dc4_mid20190330b.mes").string()));
    int dialogue = -1;
    for (int i=0;i<20 && scene.advance_to_next_text();++i) {
        if (scene.sprite(8).visible && scene.sprite(8).image=="FACE\\KSS_A1_A01,FACE\\KSS_A") {
            dialogue = i;
            break;
        }
    }
    CHECK(dialogue>=0);
    CHECK(scene.sprite(8).visible);
    CHECK(scene.sprite(8).image=="FACE\\KSS_A1_A01,FACE\\KSS_A");
    scene.draw(-1);const auto frame=scene.frame();
    const auto& face=scene.sprite(8);
    const int x=face.cur_x+int(face.frame_cx-face.img.w/2.f);
    const int y=face.cur_y+225-face.canvas_h+int(face.frame_cy-face.img.h/2.f);
    int checked=0;
    for(int row=0;row<face.img.h;++row) for(int col=0;col<face.img.w;++col) {
        const auto* p=&face.img.rgba[(size_t(row)*face.img.w+col)*4];
        if(p[3]!=255 || x+col<0 || x+col>=260 || y+row<0 || y+row>=690)continue;
        const Color color{p[0],p[1],p[2]};CHECK(pixel(scene,x+col,y+row)==color);++checked;
    }
    CHECK(checked>1000);
    std::vector<uint8_t> state,native;std::string error;
    CHECK(scene.capture_state(&state,&error));
    dc4scene::SceneRunner restored(data.string());
    CHECK(restored.load_sheets((data/"MES/start.mes").string()));
    CHECK(restored.load_ui_script((data/"MES/win_size.mes").string()));
    CHECK(restored.restore_state(state,&error));restored.draw(-1);CHECK(restored.frame()==frame);
    CHECK(restored.restore_state(without_faces(state),&error));restored.draw(-1);CHECK(restored.frame()==frame);
    CHECK(scene.capture_pc_save(&native,&error));
    CHECK(restored.restore_pc_save(native,&error));
    CHECK(restored.sprite(8).image==scene.sprite(8).image);
    CHECK(restored.sprite(8).cur_y==scene.sprite(8).cur_y);
    CHECK(restored.sprite(8).y==scene.sprite(8).y);
    if(!error.empty())std::cerr<<error<<'\n';
    CHECK(restored.advance_to_next_text());CHECK(!restored.sprite(8).visible);
    CHECK(scene.advance_to_next_text());CHECK(!scene.sprite(8).visible);
    std::cout<<"Kasumi scene, dialogue index "<<dialogue<<": "<<checked<<" opaque portrait pixels checked\n";
}
}
int main(int argc,char** argv) {
    const auto root=fs::temp_directory_path()/("arisa-dc4-message-face-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        run(root/"dc4",0xAA);run(root/"dc4ph",0xAB);
        if(argc>1)original_scene(fs::path(argv[1]));
        fs::remove_all(root);
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n';++failures; }
    std::cout<<(failures?"DC4 message face checks FAILED\n":"DC4 message face checks passed\n");
    return failures?1:0;
}
