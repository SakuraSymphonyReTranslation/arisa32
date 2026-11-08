#include "arisa32/scene.h"
#include "arisa32/resource.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace fs = std::filesystem;
namespace {
int failures = 0;
void check(bool ok, const char* expression, int line) {
    if (!ok) { std::cerr << line << ": " << expression << '\n'; ++failures; }
}
#define CHECK(value) check(bool(value), #value, __LINE__)
enum class Profile { Dm, Dc2pc, Dc4 };
struct Script {
    Profile profile;
    std::vector<uint8_t> bytes;
    explicit Script(Profile p) : profile(p), bytes{1,0,0,0,3,0,0,0,0,0,0xB6,0xAA,0} {
        if (p == Profile::Dm) { bytes[10]=0x72; bytes[11]=0x9D; }
        if (p == Profile::Dc2pc) bytes={1,0,0,0,2,0,0,0,0x69,0x99};
    }
    uint8_t speaker() const { return profile==Profile::Dm?0x4E:profile==Profile::Dc2pc?0x4B:0x6A; }
    uint8_t dialogue() const { return speaker()+1; }
    void text(uint8_t op, const std::string& text) {
        bytes.push_back(op);
        for (unsigned char ch:text) bytes.push_back(uint8_t((ch==' '?0x7F:ch)-0x20));
        bytes.push_back(0);
    }
    void string(uint8_t op, const std::string& text) {
        bytes.push_back(op); bytes.insert(bytes.end(),text.begin(),text.end()); bytes.push_back(0);
    }
    void words(uint8_t op, uint16_t value=0) {
        bytes.push_back(op); bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value>>8));
    }
    void write(const fs::path& path) const {
        fs::create_directories(path.parent_path());
        std::ofstream out(path,std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
        if (!out) throw std::runtime_error("Cannot write isolated font fixture");
    }
};
struct Frame { std::vector<uint8_t> pixels, background; };
struct Bounds { int left=1280,top=720,right=-1,bottom=-1,pixels=0; };
Bounds bounds(const Frame& frame,int left,int top,int right,int bottom,int minimum_ink=0) {
    Bounds result;
    for (int y=top;y<bottom;++y) for (int x=left;x<right;++x) {
        const size_t at=(size_t(y)*1280+x)*4;
        if (std::equal(frame.pixels.begin()+at,frame.pixels.begin()+at+3,frame.background.begin()+at)) continue;
        if (*std::min_element(frame.pixels.begin()+at,frame.pixels.begin()+at+3)<minimum_ink) continue;
        result.left=std::min(result.left,x); result.right=std::max(result.right,x);
        result.top=std::min(result.top,y); result.bottom=std::max(result.bottom,y); ++result.pixels;
    }
    return result;
}
std::vector<Bounds> text_bands(const Frame& frame,int top=560) {
    std::vector<Bounds> bands;
    for (int y=top;y<710;++y) {
        const auto row=bounds(frame,380,y,1148,y+1,180);
        if (row.pixels==0) continue;
        if (bands.empty() || bands.back().bottom+1<y) bands.push_back(row);
        else {
            auto& band=bands.back();
            band.left=std::min(band.left,row.left); band.right=std::max(band.right,row.right);
            band.bottom=y; band.pixels+=row.pixels;
        }
    }
    return bands;
}
void ppm(const fs::path& path,const std::vector<uint8_t>& pixels) {
    std::ofstream out(path,std::ios::binary); out<<"P6\n1280 720\n255\n";
    std::vector<uint8_t> rgb;
    rgb.reserve(pixels.size()/4*3);
    for (size_t at=0;at<pixels.size();at+=4) rgb.insert(rgb.end(),pixels.begin()+at,pixels.begin()+at+3);
    out.write(reinterpret_cast<const char*>(rgb.data()),std::streamsize(rgb.size()));
    if (!out) throw std::runtime_error("Cannot write font capture");
}
struct Fixture {
    fs::path root, data, ui, captures;
    Profile profile;
    dc4scene::FontRenderer& font;
    std::string name;
    std::ofstream* metrics;
    bool japanese;
    std::unique_ptr<dc4scene::SceneRunner> load(const Script& script) {
        const auto path=root/"MES/__arisa32_font_fixture__.mes"; script.write(path);
        dc4::configure_resources(root.string(),data==root?std::string{}:data.string(),"utf-8");
        dc4::set_language(japanese?dc4::Language::Japanese:dc4::Language::Indonesian);
        auto scene=std::make_unique<dc4scene::SceneRunner>(root.string());
        scene->set_font(&font);
        CHECK(scene->load_ui_script(ui.string()));
        CHECK(scene->load_script(path.string()));
        scene->set_wait_marker(false);
        CHECK(scene->advance_to_next_text());
        return scene;
    }
    Frame frame(dc4scene::SceneRunner& scene,int reveal=-1) {
        scene.set_font(&font); scene.draw(reveal); auto pixels=scene.frame();
        scene.set_font(nullptr); scene.draw(reveal); auto background=scene.frame();
        scene.set_font(&font); scene.draw(reveal); CHECK(scene.frame()==pixels);
        return {std::move(pixels),std::move(background)};
    }
    Frame dialogue(const std::string& text,const std::string& speaker,int reveal=-1) {
        Script script(profile); script.text(script.speaker(),speaker); script.text(script.dialogue(),text); script.bytes.push_back(0);
        auto scene=load(script); return frame(*scene,reveal);
    }
    void capture(const std::string& label,const Frame& frame) {
        CHECK(bounds(frame,0,0,1280,720).pixels>0);
        if (captures.empty()) return;
        ppm(captures/(name+"-"+label+".ppm"),frame.pixels);
        for (const auto& area:std::vector<std::pair<std::string,Bounds>>{
                {"all",bounds(frame,0,0,1280,720)}, {"body",bounds(frame,270,563,1280,720)},
                {"name",bounds(frame,250,500,800,563)}, {"choice",bounds(frame,0,100,1280,500)}}) {
            const auto& b=area.second;
            *metrics<<name<<','<<label<<','<<area.first<<','<<b.pixels<<','<<b.left<<','<<b.top<<','<<b.right<<','<<b.bottom<<'\n';
        }
    }
};
void dialogue_checks(Fixture& fixture,bool japanese,bool proportional) {
    const std::string name=japanese?u8"義之":"Yoshiyuki";
    const std::string prose=japanese?u8"「今日も教室でパンフ作りですね」\n「明日も一緒に頑張りましょう」":
        "Oleh_suara_jam_weker_yang_berbunyi_di_samping_\nbantal,_kesadaranku_mulai_terbangun_perlahan.";
    fixture.capture("dialogue",fixture.dialogue(prose,name));
    fixture.capture("descenders",fixture.dialogue("WiWi WWWW iiiii gjpqy 0123456789",name));
    const std::string long_text=japanese?u8"今日はみんなと一緒に教室でパンフレットを作りながら、卒業してからの毎日について話していました。":
        "Walaupun_waktu_berjalan_perlahan,_Yoshiyuki_memutuskan_untuk_membicarakan_rencana_pernikahan_bersama_Otome_di_ruang_kelas.";
    const auto long_frame=fixture.dialogue(long_text,name); fixture.capture("long-dialogue",long_frame);
    if (fixture.profile==Profile::Dm) {
        const auto ink=bounds(long_frame,380,563,1280,720);
        CHECK(ink.right<1148 && ink.bottom<710);
    }
    CHECK(fixture.dialogue("iiiWWW",name,3).pixels==fixture.dialogue("iii",name).pixels);
    CHECK(fixture.dialogue("@wWW@miiii",name,2).pixels==fixture.dialogue("@wWW",name).pixels);
    CHECK(fixture.dialogue("@siii@mWWW",name,3).pixels==fixture.dialogue("@siii",name).pixels);
    CHECK(fixture.dialogue(u8"｛よしゆき／義之｝さん",name,2).pixels==
          fixture.dialogue(u8"｛よしゆき／義之｝",name).pixels);
    fixture.capture("reveal",fixture.dialogue("iiiWWW",name,3));
    fixture.capture("styles",fixture.dialogue("@crWWW@cw_iii_@wWW@m_@sOtome@m",name));
    fixture.capture("ruby",fixture.dialogue(japanese?u8"｛よしゆき／義之｝と｛おとめ／音姫｝":u8"Nama_｛よしゆき／義之｝_dan_Otome",name));
    if (fixture.profile==Profile::Dm && !japanese) {
        std::string prefix;
        while (fixture.font.text_width(std::u16string(prefix.begin(),prefix.end())+u"dunia ",22,false,!proportional,proportional?0:-1)<=600)
            prefix+="dunia ";
        const auto automatic=fixture.dialogue(prefix+"dunia memutih",name);
        fixture.capture("word-wrap",automatic);
        CHECK(automatic.pixels==fixture.dialogue(prefix+"\ndunia memutih",name).pixels);
        const auto narrow=fixture.dialogue(std::string(90,'i'),name);
        fixture.capture("narrow",narrow);
        if (proportional) CHECK(bounds(narrow,380,605,1280,710).pixels==0);
    }
    if (proportional) {
        const auto large=fixture.dialogue("@lAgjpqy\n@lAgjpqy",name);
        fixture.capture("large-multiline",large);
        CHECK(bounds(large,380,710,1148,720).pixels==0);
        const auto bands=text_bands(large);
        CHECK(bands.size()==2);
        if (bands.size()==2) {
            CHECK(bands[0].bottom+1<bands[1].top);
            CHECK(std::abs((bands[0].bottom-bands[0].top)-(bands[1].bottom-bands[1].top))<=1);
            CHECK(bands[1].bottom<710);
        }
        const auto ruby=fixture.dialogue(u8"@lAgjpqy\n@l｛よしゆき／義之｝Agjpqy",name);
        fixture.capture("large-ruby-multiline",ruby);
        CHECK(bounds(ruby,380,710,1148,720).pixels==0);
        CHECK(text_bands(ruby).size()>=3);
        const std::string ruby_line=u8"@l｛よしゆき／義之｝Agjpqy";
        const auto four=fixture.dialogue(ruby_line+"\n"+ruby_line+"\n"+ruby_line+"\n"+ruby_line,"");
        fixture.capture("large-ruby-four-lines",four);
        CHECK(bounds(four,380,710,1148,720).pixels==0);
        const auto four_bands=text_bands(four,500);
        CHECK(four_bands.size()==8);
        if (four_bands.size()==8) {
            CHECK(std::abs((four_bands[1].bottom-four_bands[1].top)-
                           (four_bands[7].bottom-four_bands[7].top))<=1);
            CHECK(four_bands[7].bottom<710);
        }
    }
}
void choice_checks(Fixture& fixture,bool japanese) {
    Script script(fixture.profile);
    script.text(script.speaker(),japanese?u8"義之":"Yoshiyuki");
    script.text(script.dialogue(),japanese?u8"「杏に何て言おう？」":"Apa_yang_sebaiknya_aku_katakan_pada_Anzu?");
    script.words(fixture.profile==Profile::Dm?0x07:fixture.profile==Profile::Dc2pc?0x08:0x09,1);
    const uint8_t target=fixture.profile==Profile::Dm?0x43:fixture.profile==Profile::Dc2pc?0x44:0x5C;
    const uint8_t label=fixture.profile==Profile::Dm?0x34:fixture.profile==Profile::Dc2pc?0x35:0x4B;
    const std::vector<std::string> choices=japanese?
        std::vector<std::string>{u8"○素直にお祝いする",u8"○少し冗談を言ってみる",u8"○明日もみんなで教室に集まってパンフレットを作ろうと誘う"}:
        std::vector<std::string>{u8"○_Memberi_selamat_dengan_jujur",u8"○_Mencoba_sedikit_bercanda",u8"○_Walaupun_terlambat,_mengucapkan_selamat_ulang_tahun_dan_mendoakan_kebahagiaannya"};
    for (size_t i=0;i<choices.size();++i) {
        script.string(target,"branch"+std::to_string(i)); script.text(script.dialogue()+1,choices[i]);
    }
    script.words(fixture.profile==Profile::Dc4?0x0A:0x09);
    for (size_t i=0;i<choices.size();++i) {
        script.string(label,"branch"+std::to_string(i)); script.text(script.dialogue(),"BRANCH"+std::to_string(i));
    }
    script.bytes.push_back(0);
    auto scene=fixture.load(script);
    CHECK(!scene->advance_to_next_text()); CHECK(scene->waiting_for_choice()); CHECK(scene->choices().size()==3);
    if (!scene->waiting_for_choice() || scene->choices().size()!=3) return;
    std::vector<uint8_t> saved; CHECK(scene->capture_state(&saved));
    scene->set_pointer(-1,-1,false); const auto normal=fixture.frame(*scene);
    fixture.capture("choice-normal",normal);
    if (fixture.profile==Profile::Dm) {
        const auto ink=bounds(normal,0,100,1280,500);
        CHECK(ink.left>=380 && ink.right<1070 && ink.top>=188 && ink.bottom<=407);
    }
    for (int i=0;i<3;++i) {
        CHECK(scene->restore_state(saved));
        int hit_x=-1,hit_y=-1;
        for (int y=0;y<500 && hit_y<0;y+=5) for (int x=250;x<1150;x+=5)
            if (scene->choice_at(x,y)==i) {hit_x=x+8;hit_y=y+8;break;}
        CHECK(hit_y>=0); if(hit_y<0)continue;
        scene->set_pointer(hit_x,hit_y,false); const auto hovered=fixture.frame(*scene);
        if (i==2) fixture.capture("choice-long-hover",hovered);
        scene->set_pointer(hit_x,hit_y,true); const auto pressed=fixture.frame(*scene);
        if (i==2) fixture.capture("choice-long-pressed",pressed);
        if (fixture.profile==Profile::Dm && fixture.data!=fixture.root) CHECK(hovered.pixels!=pressed.pixels);
        CHECK(scene->choose(size_t(i))); CHECK(scene->advance_to_next_text());
        CHECK(scene->current_line()=="BRANCH"+std::to_string(i));
    }
}
void font_identity_checks(const fs::path& root,const char* gothic_path,const char* pretendard_path) {
    dc4scene::FontRenderer font;
    CHECK(!font.is_pretendard_jp_bold());
    CHECK(font.load(pretendard_path,0,22,false));
    CHECK(font.is_pretendard_jp_bold());
    const auto renamed=root/"imported/default.ttf";
    fs::create_directories(renamed.parent_path());
    fs::copy_file(pretendard_path,renamed);
    CHECK(font.load(renamed.string(),0,22,false));
    CHECK(font.is_pretendard_jp_bold());
    CHECK(font.load(gothic_path,0,22,false));
    CHECK(!font.is_pretendard_jp_bold());
    CHECK(font.load(renamed.string(),0,22,false));
    CHECK(font.is_pretendard_jp_bold());
    CHECK(!font.load((root/"missing.ttf").string(),0,22,false));
    CHECK(!font.is_pretendard_jp_bold());
}
}
int main(int argc,char** argv) {
    if (argc<3) {std::cerr<<"Usage: dc2dm_font_test MSGOTHIC.TTC PretendardJP-Bold.ttf [captures] [DC2DM AdvData]\n";return 1;}
    const auto root=fs::temp_directory_path()/("arisa32-dc2dm-font-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        font_identity_checks(root,argv[1],argv[2]);
        const fs::path captures=argc>3?argv[3]:"";
        if (!captures.empty()) fs::create_directories(captures);
        std::ofstream metrics;
        if (!captures.empty()) {metrics.open(captures/"metrics.csv");metrics<<"case,capture,region,ink_pixels,left,top,right,bottom\n";}
        for (const auto profile:{Profile::Dm,Profile::Dc2pc,Profile::Dc4}) {
            const std::string profile_name=profile==Profile::Dm?"dc2dm":profile==Profile::Dc2pc?"dc2pc":"dc4";
            for (int font_kind=0;font_kind<(profile==Profile::Dm?3:2);++font_kind) {
                const bool pretendard=font_kind==1, native=font_kind==2;
#ifndef _WIN32
                if (native) continue;
#endif
                dc4scene::FontRenderer font;
                CHECK(font.load(argv[pretendard?2:1],0,22,native));
                for (bool japanese:{false,true}) {
                    dc4::set_language(japanese?dc4::Language::Japanese:dc4::Language::Indonesian);
                    const std::string name=profile_name+"-"+(pretendard?"pretendard":native?"msgothic-gdi":"msgothic-stb")+"-"+(japanese?"jp":"latin");
                    const auto fixture_root=root/name;
                    Script ui(profile); ui.bytes.push_back(0); ui.write(fixture_root/"MES/start.mes");
                    fs::path data=fixture_root,ui_path=fixture_root/"MES/start.mes";
                    if (profile==Profile::Dm && argc>4) data=argv[4];
                    Fixture fixture{fixture_root,data,ui_path,captures,profile,font,name,&metrics,japanese};
                    dialogue_checks(fixture,japanese,profile==Profile::Dm && pretendard);
                    choice_checks(fixture,japanese);
                    std::cout<<"Checked "<<name<<'\n';
                }
            }
        }
        dc4::configure_resources({});
        fs::remove_all(root);
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';++failures;}
    std::cout<<"DC2DM font checks: "<<failures<<" failure(s)\n";
    return failures?1:0;
}
