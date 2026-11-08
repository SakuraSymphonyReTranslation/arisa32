#include "arisa32/system_menu.h"
#include "arisa32/crx.h"
#include "arisa32/names.h"
#include "arisa32/resource.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
using Menu = dc4ui::SystemMenu;
using Page = dc4ui::SystemMenuPage;
namespace fs = std::filesystem;
int failures = 0, checks = 0;
void check(bool ok, const char* expression, int line) {
    ++checks;
    if (!ok) { ++failures; std::cerr << line << ": " << expression << '\n'; }
}
#define CHECK(x) check(bool(x), #x, __LINE__)
std::vector<uint8_t> read(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(file),{}};
}
void write(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path,std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
    CHECK(bool(file));
}
struct Script {
    std::vector<uint8_t> bytes{0,0,0,0,0x69,0x99};
    void param(int op,int a,int b) { bytes.insert(bytes.end(),{uint8_t(op),uint8_t(a),uint8_t(b)}); }
    void words(int op,int a,int b,int c,int d) {
        bytes.push_back(uint8_t(op));
        for (int value : {a,b,c,d}) { bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value>>8)); }
    }
    void str(int op,const std::string& value) {
        bytes.push_back(uint8_t(op)); bytes.insert(bytes.end(),value.begin(),value.end()); bytes.push_back(0);
    }
    void label(const std::string& value) { str(0x35,value); }
    void save(const fs::path& root,const std::string& name) {
        auto result=bytes; result.push_back(0); write(root/"MES"/(name+".mes"),result);
    }
};
struct Motion { int id, width, height, x, y, target_x, target_y; };
constexpr Motion motions[] = {
    {3,800,1200,0,-600,0,0}, {4,800,1200,0,0,0,-600},
    {5,1600,600,0,0,-800,0}, {6,1600,600,-800,0,0,0},
    {7,800,837,0,0,0,-237}, {8,800,1800,0,-1200,0,0},
    {9,800,846,0,0,0,-246}, {10,800,1056,0,0,0,-456},
    {11,800,933,0,-333,0,0}, {12,800,1066,0,0,0,-466}
};
std::array<uint8_t,3> color(int id,int x,int y) {
    return {uint8_t((x+y)%2 ? 255 : 0),uint8_t((y+id*17)%251),uint8_t((x+id*31)%251)};
}
void picture(const fs::path& path,int id,int width,int height) {
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0};
    const auto word=[&](int value) { bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value>>8)); };
    word(width); word(height); word(1); word(0); word(0); word(0);
    const int stride=(width*3+3)&~3;
    std::vector<uint8_t> pixels(size_t(stride)*height);
    for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
        const auto rgb=color(id,x,y); const size_t at=size_t(y)*stride+x*3;
        pixels[at]=rgb[2]; pixels[at+1]=rgb[1]; pixels[at+2]=rgb[0];
    }
    for (size_t i=0;i<pixels.size();i+=8) {
        bytes.push_back(255); bytes.insert(bytes.end(),pixels.begin()+i,pixels.begin()+std::min(i+8,pixels.size()));
    }
    write(path,bytes);
}
void fixtures(const fs::path& root) {
    Script main; main.param(0x09,0,0); main.save(root,"main"); main.save(root,"title_00");
    Script gallery; gallery.label("menu"); gallery.str(0x30,"BG/test15");
    for (int id : {0,3,4,5,6,7,8,9,10,11,12,13}) {
        gallery.words(0x4F,20+id*40,20,55+id*40,80); gallery.str(0x3A,"cg"+std::to_string(id));
    }
    gallery.param(0x09,0,0);
    for (int id=0;id<16;++id) {
        gallery.label("cg"+std::to_string(id)); gallery.words(0x5F,97,id,1,0); gallery.param(0x02,1,0);
        gallery.str(0x30,"*"+std::to_string(id)); gallery.words(0x5C,3,2,id,0);
        gallery.str(0x3A,id>=3 && id<=12 ? "motion"+std::to_string(id) : "still"); gallery.param(0x1E,1,1);
    }
    gallery.str(0x3A,"cg0");
    gallery.label("still"); gallery.param(0x0D,0,0); gallery.str(0x3B,"page_chk");
    gallery.param(0x12,0,1); gallery.str(0x3A,"cg_next");
    for (const auto& motion : motions) {
        gallery.label("motion"+std::to_string(motion.id));
        if (motion.x || motion.y) gallery.words(0x62,motion.x,motion.y,1,0);
        gallery.param(0x0D,0,0); gallery.str(0x3B,"page_chk"); gallery.param(0x12,0,1);
        gallery.words(0x62,motion.target_x,motion.target_y,0,0); gallery.param(0x12,0,1); gallery.str(0x3A,"cg_next");
    }
    gallery.label("page_chk"); gallery.words(0x5D,3,0,3,2); gallery.str(0x3A,"menu");
    gallery.label("cg_next");
    for (int id=0;id<16;++id) {
        gallery.words(0x5F,3,2,id,0); gallery.param(0x02,1,0);
        gallery.str(0x3A,"cg"+std::to_string(id==12 ? 0 : id==15 ? 13 : id+1)); gallery.param(0x1E,1,1);
    }
    gallery.save(root,"cg_otome");
    std::vector<uint8_t> names(772); names[0]=6;
    const auto number=[&](int value) { for (int i=0;i<4;++i) names.push_back(uint8_t(value>>(8*i))); };
    number(0); number(0); number(16);
    for (int id=0;id<16;++id) {
        const auto name="BG/test"+std::to_string(id); const size_t at=names.size(); names.resize(at+128);
        std::copy(name.begin(),name.end(),names.begin()+at);
        const auto found=std::find_if(std::begin(motions),std::end(motions),[&](const auto& m){return m.id==id;});
        picture(root/"GRP"/(name+".crx"),id,found==std::end(motions)?800:found->width,found==std::end(motions)?600:found->height);
    }
    number(0); write(root/"DAT/names.dat",names);
}
bool has(Menu& menu,const std::string& tag) {
    for (const auto& control : menu.controls()) if (control.enabled && control.tag==tag) return true;
    return false;
}
void tap(Menu& menu) { menu.pointer_down(640,360); menu.pointer_up(640,360); }
std::vector<uint8_t> frame(Menu& menu,int width=1280,int height=720) {
    std::vector<uint8_t> pixels; CHECK(menu.draw_gallery(width,height,&pixels)); return pixels;
}
uint8_t pixel(const std::vector<uint8_t>& pixels,int width,int x,int y,int channel) {
    const size_t at=(size_t(y)*width+x)*4+channel;
    return at<pixels.size() ? pixels[at] : 0;
}
void sample_original(const std::vector<uint8_t>& pixels,int output_width,int output_height,
                     const std::vector<uint8_t>& original,int source_width,int source_height,int offset_x,int offset_y) {
    const int viewport_width=output_height*4/3, left=(output_width-viewport_width)/2;
    bool bars=true, samples=true;
    for (int y : {0,output_height/2,output_height-1}) for (int x : {0,left-1,left+viewport_width,output_width-1})
        for (int c=0;c<3;++c) bars &= pixel(pixels,output_width,x,y,c)==0;
    for (int y : {0,23,output_height/2,output_height-1}) for (int x : {0,31,viewport_width/2,viewport_width-1}) {
        const double sx=std::clamp((x+0.5)*800/viewport_width-0.5-offset_x,0.0,double(source_width-1));
        const double sy=std::clamp((y+0.5)*600/output_height-0.5-offset_y,0.0,double(source_height-1));
        const int x0=int(sx), y0=int(sy), x1=std::min(x0+1,source_width-1), y1=std::min(y0+1,source_height-1);
        for (int c=0;c<3;++c) {
            const double top=original[(size_t(y0)*source_width+x0)*4+c]*(1-(sx-x0))+original[(size_t(y0)*source_width+x1)*4+c]*(sx-x0);
            const double bottom=original[(size_t(y1)*source_width+x0)*4+c]*(1-(sx-x0))+original[(size_t(y1)*source_width+x1)*4+c]*(sx-x0);
            samples &= std::abs(int(pixel(pixels,output_width,left+x,y,c))-int(std::lround(top*(1-(sy-y0))+bottom*(sy-y0))))<=1;
        }
    }
    CHECK(bars); CHECK(samples);
}
std::vector<uint8_t> decode(const fs::path& path,dc4::CrxInfo* info) {
    const auto bytes=read(path); std::vector<uint8_t> rgba; std::string error;
    CHECK(dc4::crx_decode(bytes.data(),bytes.size(),&rgba,info,&error)); return rgba;
}
void capture(Menu& menu,const std::string& name) {
    const char* folder=std::getenv("DC2PC_GALLERY_CAPTURES"); if (!folder) return;
    fs::create_directories(folder); const auto pixels=frame(menu);
    std::ofstream file(fs::path(folder)/(name+".ppm"),std::ios::binary); file<<"P6\n1280 720\n255\n";
    for (size_t i=0;i<pixels.size();i+=4) file.write(reinterpret_cast<const char*>(pixels.data()+i),3);
}
void fade_samples(const std::vector<uint8_t>& actual,const std::vector<uint8_t>& old,
                  const std::vector<uint8_t>& target,int weight,int ticks) {
    bool matches=true;
    for (int y : {0,90,359,719}) for (int x : {0,159,160,321,900,1119,1120,1279}) for (int c=0;c<3;++c) {
        const size_t at=(size_t(y)*1280+x)*4+c;
        int value=old[at];
        for (int tick=0;tick<ticks;++tick) value=((16-weight)*value+weight*target[at])/16;
        matches &= actual[at]==value;
    }
    CHECK(matches);
}
void synthetic(const fs::path& root) {
    fixtures(root); Menu menu; std::string error; CHECK(menu.load(root.string(),&error)); CHECK(error.empty());
    menu.set_variable(99,10,0);
    for (int id=0;id<16;++id) if (id!=2) menu.set_variable(97,id,1);
    menu.open(Page::Gallery); CHECK(has(menu,"cg0"));
    std::vector<uint8_t> unused{1,2,3}; CHECK(!menu.draw_gallery(1280,720,&unused));
    menu.activate("cg0"); CHECK(menu.gallery_preview_active()); CHECK(menu.variable(99,100)==0);
    const auto initial=frame(menu); menu.pointer_down(640,360); menu.cancel_pointer(); menu.pointer_up(640,360); CHECK(frame(menu)==initial);
    tap(menu); CHECK(menu.variable(99,100)==1); tap(menu); CHECK(menu.variable(99,100)==3);
    CHECK(menu.variable(3,2)==3); menu.back(); CHECK(!menu.gallery_preview_active()); CHECK(menu.variable(3,0)==3);
    menu.activate("cg0"); const auto hd=frame(menu,3200,1800); bool exact=true;
    for (int y=0;y<600;++y) for (int x=0;x<800;++x) {
        const auto expected=color(0,x,y);
        for (int c=0;c<3;++c) exact &= pixel(hd,3200,400+3*x+1,3*y+1,c)==expected[c];
    }
    CHECK(exact); CHECK(frame(menu)==initial); CHECK(frame(menu,3200,1800)==hd);
    CHECK(!menu.draw_gallery(0,720,&unused)); CHECK(!menu.draw_gallery(1280,0,&unused));
    CHECK(!menu.draw_gallery(1920,1200,&unused)); CHECK(!menu.draw_gallery(7680,4320,&unused)); CHECK(!menu.draw_gallery(1280,720,nullptr));
    for (const auto& motion : motions) for (int speed=0;speed<4;++speed) {
        menu.back(); menu.set_variable(99,12,speed); menu.activate("cg"+std::to_string(motion.id));
        CHECK(menu.variable(99,100)==motion.id); dc4::CrxInfo info;
        const auto original=decode(root/"GRP/BG"/("test"+std::to_string(motion.id)+".crx"),&info);
        const auto first=frame(menu); sample_original(first,1280,720,original,info.width,info.height,motion.x,motion.y);
        if (speed==1 && (motion.id==3 || motion.id==4)) capture(menu,"synthetic-cg"+std::to_string(motion.id)+"-initial");
        menu.update(100000); CHECK(frame(menu)==first); tap(menu); CHECK(frame(menu)==first);
        menu.update(16); CHECK(frame(menu)==first); menu.update(1);
        const int step=2<<speed;
        const auto move=[&](int start,int end){return start+std::clamp(end-start,-step,step);};
        sample_original(frame(menu),1280,720,original,info.width,info.height,move(motion.x,motion.target_x),move(motion.y,motion.target_y));
        const auto moving=frame(menu); tap(menu); CHECK(menu.variable(99,100)==motion.id); CHECK(frame(menu)==moving);
        if (speed==1 && (motion.id==3 || motion.id==4)) {
            menu.update(1258); capture(menu,"synthetic-cg"+std::to_string(motion.id)+"-moving");
        }
        menu.update(100000); const auto end=frame(menu);
        if (speed==1 && (motion.id==3 || motion.id==4)) capture(menu,"synthetic-cg"+std::to_string(motion.id)+"-endpoint");
        sample_original(end,1280,720,original,info.width,info.height,motion.target_x,motion.target_y);
        sample_original(frame(menu,1920,1080),1920,1080,original,info.width,info.height,motion.target_x,motion.target_y);
        CHECK(frame(menu)==end); tap(menu); CHECK(menu.variable(99,100)==(motion.id==12?0:motion.id+1));
    }
    menu.back(); menu.activate("cg13"); CHECK(menu.variable(99,100)==13);
    tap(menu); CHECK(menu.variable(99,100)==14); tap(menu); CHECK(menu.variable(99,100)==15); tap(menu); CHECK(menu.variable(99,100)==13);
    menu.scroll(-1); CHECK(menu.variable(99,100)==15); menu.back();
    dc4scene::FontRenderer font; std::vector<uint8_t> menu_pixels; menu.draw(-1,-1,false,font,&menu_pixels);
    for (int speed=0;speed<4;++speed) {
        menu.set_variable(99,10,1); menu.set_variable(99,12,speed); menu.draw(-1,-1,false,font,&menu_pixels);
        menu.activate("cg0"); const int weights[]{3,6,8,10}; const int duration=640>>speed;
        fade_samples(frame(menu),menu_pixels,initial,weights[speed],1);
        menu.update(39); fade_samples(frame(menu),menu_pixels,initial,weights[speed],1);
        menu.update(1); fade_samples(frame(menu),menu_pixels,initial,weights[speed],2);
        menu.update(duration-41); fade_samples(frame(menu),menu_pixels,initial,weights[speed],duration/40);
        menu.update(1); CHECK(frame(menu)==initial); menu.back();
    }
    menu.set_variable(99,12,0); menu.activate("cg0"); menu.update(640);
    tap(menu); CHECK(menu.variable(99,100)==1); tap(menu); CHECK(menu.variable(99,100)==1); tap(menu); CHECK(menu.variable(99,100)==3);
    menu.back(); menu.update(100000); CHECK(!menu.gallery_preview_active());
    menu.set_variable(99,10,0); menu.activate("cg3"); tap(menu); menu.update(170); menu.back(); menu.activate("cg3");
    dc4::CrxInfo info; const auto portrait=decode(root/"GRP/BG/test3.crx",&info);
    sample_original(frame(menu),1280,720,portrait,info.width,info.height,0,-600);
    Menu sparse; CHECK(sparse.load(root.string(),&error)); sparse.set_variable(99,10,0); sparse.open(Page::Gallery);
    sparse.activate("cg0"); CHECK(!sparse.gallery_preview_active());
    sparse.set_variable(97,1,1); sparse.activate("cg0"); CHECK(sparse.variable(99,100)==1);
    tap(sparse); CHECK(sparse.variable(99,100)==1); sparse.back(); sparse.set_variable(97,1,0);
    sparse.set_variable(97,3,1); sparse.activate("cg3"); const auto single=frame(sparse);
    tap(sparse); sparse.update(100000); CHECK(frame(sparse)!=single); tap(sparse);
    CHECK(sparse.variable(99,100)==3); CHECK(frame(sparse)==single); sparse.back(); sparse.set_variable(97,3,0);
    sparse.activate("cg3"); CHECK(!sparse.gallery_preview_active()); CHECK(sparse.error().empty());
    menu.back(); picture(root/"GRP/BG/test0.crx",15,800,600); CHECK(menu.load(root.string(),&error));
    menu.set_variable(99,10,0); menu.set_variable(97,0,1); menu.open(Page::Gallery); menu.activate("cg0");
    CHECK(frame(menu)!=initial);
}
struct Route { const char* name; int first,last; };
constexpr Route routes[] = {
    {"otome",0,98},{"yume",99,229},{"nanaka",230,328},{"minatu",329,413},
    {"anzu",414,511},{"koko",512,653},{"aisia",654,749},{"erika",750,837},
    {"akane",838,928},{"maya",929,1003},{"mayuki",1004,1108},{"mahiru",1109,1222},
    {"other",1223,1251},{"other",1252,1267},{"other",1268,1307},{"other",1308,1335}
};
struct Portrait { const char* route; int first,last,start,end; };
// Consecutive IDs share the same crop in the original DC2PC MES helpers.
constexpr Portrait portraits[] = {
    {"akane",889,889,0,-456},{"akane",892,892,-600,0},{"akane",893,894,0,-600},
    {"anzu",441,444,-600,0},{"anzu",445,453,0,-600},{"erika",796,796,-333,0},
    {"koko",533,534,0,-600},{"koko",626,629,-600,0},{"mahiru",1138,1141,0,-600},
    {"maya",956,958,0,-237},{"minatu",389,390,-1200,0},{"nanaka",237,239,-600,0},
    {"nanaka",247,257,-600,0},{"nanaka",303,303,0,-246},{"other",1265,1266,0,-600},
    {"other",1267,1267,0,-466},{"otome",5,5,-600,0},{"otome",48,53,-600,0},
    {"yume",99,102,0,-600},{"yume",118,118,-600,0},{"yume",135,137,0,-600}
};
bool is_portrait(int id) {
    return std::any_of(std::begin(portraits),std::end(portraits),[&](const auto& item){return id>=item.first && id<=item.last;});
}
void next(Menu& menu) {
    const int id=menu.variable(99,100);
    tap(menu);
    if (is_portrait(id)) { CHECK(menu.variable(99,100)==id); menu.update(100000); tap(menu); }
}
void installed(const std::string& data) {
    dc4::GameNames names; CHECK(dc4::decode_game_names(read(fs::path(data)/"DAT/names.dat"),&names));
    CHECK(names.cg.size()==1336);
    for (const auto& route : routes) {
        Menu menu; std::string error; CHECK(menu.load(data,&error)); menu.set_variable(99,10,0);
        for (int id : {route.first,route.first+1,route.last}) menu.set_variable(97,id,1);
        menu.open(Page::Gallery); menu.activate(route.name);
        if (std::string(route.name)=="other") menu.set_variable(3,0,route.first==1223?0:route.first==1252?1:route.first==1268?2:3);
        menu.activate("cg"+std::to_string(route.first)); CHECK(menu.gallery_preview_active()); CHECK(menu.variable(99,100)==route.first);
        next(menu); CHECK(menu.variable(99,100)==route.first+1); next(menu); CHECK(menu.variable(99,100)==route.last);
        menu.back(); CHECK(!menu.gallery_preview_active()); CHECK(menu.variable(3,2)==route.last-(std::string(route.name)=="other"?1223:route.first));
        menu.activate("cg"+std::to_string(route.last)); CHECK(menu.variable(99,100)==route.last);
        next(menu); CHECK(menu.variable(99,100)==route.first); CHECK(menu.error().empty());
        std::cout<<"Route "<<route.name<<' '<<route.first<<".."<<route.last<<" checked\n";
    }
    int count=0;
    for (const auto& group : portraits) {
        Menu menu; std::string error; CHECK(menu.load(data,&error)); menu.set_variable(99,10,0); menu.set_variable(99,12,1);
        for (int id=group.first;id<=group.last;++id) menu.set_variable(97,id,1);
        menu.open(Page::Gallery); menu.activate(group.route);
        if (std::string(group.route)=="other") menu.set_variable(3,0,1);
        for (int id=group.first;id<=group.last;++id) {
            menu.activate("cg"+std::to_string(id)); CHECK(menu.variable(99,100)==id);
            std::string asset=names.cg[size_t(id)]; std::replace(asset.begin(),asset.end(),'\\','/');
            dc4::CrxInfo info; const auto original=decode(dc4::resolve_asset(data+"/GRP/"+asset+".crx"),&info);
            CHECK(info.width==800 && info.height>600);
            sample_original(frame(menu),1280,720,original,info.width,info.height,0,group.start);
            tap(menu); menu.update(1275); CHECK(menu.variable(99,100)==id);
            const int midpoint=group.start+std::clamp(group.end-group.start,-300,300);
            sample_original(frame(menu),1280,720,original,info.width,info.height,0,midpoint);
            menu.update(100000); sample_original(frame(menu),1280,720,original,info.width,info.height,0,group.end);
            menu.back(); ++count;
        }
    }
    CHECK(count==66); std::cout<<"Original portraits checked: "<<count<<'\n';
}
}
int main(int argc,char** argv) {
    const auto parent=fs::canonical(fs::temp_directory_path());
    const auto scratch=parent/("dc2pc-gallery-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    synthetic(scratch);
    if (const char* data=argc>1?argv[1]:std::getenv("DC2PC_TEST_DATA")) installed(data);
    else std::cout<<"Original DC2PC assets skipped; set DC2PC_TEST_DATA to include them.\n";
    if (scratch.parent_path()==parent) fs::remove_all(scratch);
    std::cout<<"DC2PC gallery: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
