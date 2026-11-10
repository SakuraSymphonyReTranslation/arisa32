#include "arisa32/log_screen.h"
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
int failures = 0;
void check(bool ok, const char* expression, int line) {
    if (!ok) { std::cerr << line << ": " << expression << '\n'; ++failures; }
}
#define CHECK(x) check(bool(x), #x, __LINE__)
using Color = std::array<uint8_t, 3>;
Color atlas(int x, int y) { return {uint8_t(x % 251), uint8_t(y % 251), uint8_t((x + y) % 251)}; }
void write(const fs::path& path, const std::vector<uint8_t>& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size())))
        throw std::runtime_error("Cannot write log fixture");
}
void crx(const fs::path& path, int w, int h, bool grid, bool corners = false) {
    std::vector<uint8_t> bytes{'C','R','X','G',0,0,0,0};
    auto word = [&](int value) { bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value >> 8)); };
    word(w); word(h); word(1); word(0); word(0); word(0);
    const int stride = (w * 3 + 3) & ~3;
    std::vector<uint8_t> pixels(size_t(stride) * h, 0);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        const Color c = grid ? atlas(x,y) : corners
            ? Color{uint8_t(x < w/2 ? 240 : 20), 50, uint8_t(y < h/2 ? 30 : 220)}
            : Color{10,200,70};
        const size_t at = size_t(y) * stride + x * 3;
        pixels[at] = c[2]; pixels[at+1] = c[1]; pixels[at+2] = c[0];
    }
    for (size_t at = 0; at < pixels.size(); at += 8) {
        bytes.push_back(255);
        bytes.insert(bytes.end(), pixels.begin()+at, pixels.begin()+std::min(at+8,pixels.size()));
    }
    write(path,bytes);
}
Color pixel(const std::vector<uint8_t>& frame, int x, int y, int w = 1280) {
    const size_t at = (size_t(y) * w + x) * 4;
    return {frame.at(at),frame.at(at+1),frame.at(at+2)};
}
void font_capture(const std::string& name, const std::vector<uint8_t>& frame) {
    const char* output=std::getenv("DC2DM_LOG_CAPTURES");
    if (!output || !*output) return;
    fs::create_directories(output);
    std::ofstream out(fs::path(output)/(name+".ppm"),std::ios::binary);
    out<<"P6\n1280 720\n255\n";
    for (size_t at=0;at<frame.size();at+=4) out.write(reinterpret_cast<const char*>(frame.data()+at),3);
    if (!out) throw std::runtime_error("Cannot write backlog font capture");
}
struct Script {
    std::vector<uint8_t> bytes{1,0,0,0,3,0,0,0,0,0,0xB6,0xAA,0};
    void string(uint8_t op, const std::string& s) {
        bytes.push_back(op); bytes.insert(bytes.end(),s.begin(),s.end()); bytes.push_back(0);
    }
    void slot(uint8_t op, uint8_t id, const std::string& s) { bytes.push_back(op); string(id,s); }
    void words(uint8_t op, std::initializer_list<uint16_t> values) {
        bytes.push_back(op);
        for (auto value : values) { bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value>>8)); }
    }
    void text(const std::string& s) {
        bytes.push_back(0x6B); for (unsigned char c : s) bytes.push_back(uint8_t(c-0x20)); bytes.push_back(0);
    }
};
void text_layout() {
    const std::u16string first = u"「いけないイタズラ、しちゃいますよ？　それでも良い";
    const std::u16string second = u"　んですか？」";
    const auto lines = dc4ui::LogScreen::text_lines(first + u"\r\n" + second, false);
    CHECK(lines == (std::vector<std::u16string>{first,second}));
    CHECK(dc4ui::LogScreen::text_lines(u"A\n\nB",false) == (std::vector<std::u16string>{u"A",u"",u"B"}));
    CHECK(dc4ui::LogScreen::text_lines(u"Hello\nworld",true) == (std::vector<std::u16string>{u"Hello world"}));
    CHECK(dc4ui::LogScreen::text_lines(u"Halo\n dunia",true) == (std::vector<std::u16string>{u"Halo dunia"}));
    const std::u16string long_jp(70,u'あ');
    const auto wrapped = dc4ui::LogScreen::text_lines(long_jp,false);
    std::u16string joined;
    for (const auto& line : wrapped) { CHECK(line.size() <= 32); joined += line; }
    CHECK(joined == long_jp);
    CHECK(dc4ui::LogScreen::text_lines(u"",false).empty());
}
void row_art(const fs::path& root) {
    dc4ui::LogScreen log;
    CHECK(log.load(root.string()));
    dc4ui::LogRow row;
    row.name = dc4scene::sjis_to_utf16("\x93\xf1\x94\x54"); // Nino, face cell 3.
    row.thumb.assign(128 * 72 * 4, 255);
    for (size_t i = 0; i < row.thumb.size(); i += 4) { row.thumb[i]=240; row.thumb[i+1]=10; row.thumb[i+2]=30; }
    const dc4scene::FontRenderer no_font;
    std::vector<uint8_t> normal(1280*720*4,255), hover=normal, disabled=normal;
    log.draw({row},0,true,-1,-1,no_font,&normal);
    log.draw({row},0,true,350,120,no_font,&hover);
    log.draw({row},0,false,350,120,no_font,&disabled);
    // Portrait/name remain the same even while a preview is visible.
    for (int y=100;y<220;++y) for (int x=20;x<170;++x)
        CHECK(pixel(normal,x,y)==pixel(hover,x,y));
    CHECK(pixel(hover,40,122)==atlas(473,392));
    CHECK(pixel(hover,184,120)==(Color{240,10,30}));
    CHECK(pixel(hover,311,191)==(Color{240,10,30}));
    CHECK(pixel(hover,183,120)==pixel(normal,183,120));
    CHECK(pixel(hover,312,191)==pixel(normal,312,191));
    CHECK(pixel(disabled,184,120)==pixel(normal,184,120));
    CHECK(pixel(disabled,40,122)==pixel(normal,40,122));
    // Scrolling/hover must select the right row's preview, including narration.
    dc4ui::LogRow narration;
    narration.thumb.assign(128*72*4,255);
    auto scrolled=normal;
    log.draw({row,narration},1,true,350,120,no_font,&scrolled);
    CHECK(pixel(scrolled,184,120)==(Color{255,255,255}));
    CHECK(pixel(scrolled,40,122)==atlas(20,246));
}
void capture(const fs::path& root) {
    crx(root/"GRP/BG/second.CRX",256,144,false,true);
    Script ui;
    ui.slot(0x3D,0,"SYSTEM\\BKLOGBACK"); ui.bytes.push_back(0);
    write(root/"MES/ui.mes",ui.bytes);
    Script script;
    script.slot(0x3C,1,"SYSTEM\\BKLOGBACK"); script.string(0x49,"SYSTEM\\BKLOGBACK"); script.text("FIRST");
    script.slot(0x3C,1,"BG\\second"); script.string(0x49,"BG\\second"); script.text("SECOND\nLINE");
    script.bytes.push_back(0);
    write(root/"MES/scene.mes",script.bytes);
    dc4scene::SceneRunner scene(root.string());
    CHECK(scene.load_sheets((root/"MES/ui.mes").string()));
    scene.set_window_opacity(100);
    CHECK(scene.load_script((root/"MES/scene.mes").string()));
    CHECK(scene.advance_to_next_text()); scene.draw(-1);
    CHECK(scene.advance_to_next_text()); // No draw: frame still belongs to FIRST.
    const auto pc=scene.script_position();
    const auto row=dc4ui::capture_log_row(scene,7);
    CHECK(row.line==7 && row.text==u"SECOND\nLINE" && !row.snapshot.empty());
    CHECK(row.script=="scene.mes");
    CHECK(scene.script_position()==pc && scene.message_visible());
    CHECK(row.thumb.size()==128*72*4);
    CHECK(pixel(row.thumb,0,0,128)==(Color{240,50,30}));
    CHECK(pixel(row.thumb,127,0,128)==(Color{20,50,30}));
    CHECK(pixel(row.thumb,0,71,128)==(Color{240,50,220}));
    CHECK(pixel(row.thumb,127,71,128)==(Color{20,50,220}));
    // A fresh ordinary render contains the green message window, but the
    // preview includes the scene underneath it (and all four canvas corners).
    scene.draw(-1);
    CHECK(pixel(scene.frame(),1270,710)==(Color{10,200,70}));
    CHECK(scene.capture_thumbnail(0,72).empty());
}
void voice_associations(const fs::path& root) {
    for (uint8_t version : {0xAA, 0xAB}) {
        Script script; script.bytes[11] = version;
        script.string(0x4D,"VOICE\\NIN001"); script.text("NINO");
        script.bytes.insert(script.bytes.end(),{0x06,0,0}); script.text("NARRATION");
        script.bytes.insert(script.bytes.end(),{0x06,0,0,0x6A});
        for (unsigned char ch : std::string("Ichito")) script.bytes.push_back(uint8_t(ch-0x20));
        script.bytes.push_back(0); script.text("MC");
        script.bytes.insert(script.bytes.end(),{0x06,0,0});
        script.string(0x4D,"VOICE\\NIN002"); script.text("NINO AGAIN"); script.bytes.push_back(0);
        write(root/"MES/voices.mes",script.bytes);
        dc4scene::SceneRunner scene(root.string());
        CHECK(scene.load_script((root/"MES/voices.mes").string()));
        std::vector<dc4ui::LogRow> rows;
        for (int i=0;i<4;++i) { CHECK(scene.advance_to_next_text()); rows.push_back(dc4ui::capture_log_row(scene,i)); }
        CHECK(rows[0].voice=="VOICE\\NIN001" && rows[3].voice=="VOICE\\NIN002");
        CHECK(rows[1].voice.empty() && rows[2].voice.empty() && rows[2].name==u"Ichito");
        CHECK(scene.restore_state(rows[2].snapshot) && scene.last_voice().empty());
        dc4ui::LogScreen log; CHECK(log.load(root.string()));
        const dc4scene::FontRenderer font;
        std::vector<uint8_t> frame(1280*720*4,255);
        log.draw(rows,0,false,-1,-1,font,&frame);
        CHECK(pixel(frame,328,150)==atlas(362,12));
        CHECK(pixel(frame,328,600)==atlas(362,12));
        CHECK(pixel(frame,328,300)!=atlas(362,12));
        CHECK(pixel(frame,328,450)!=atlas(362,12));
    }
}
void dc2dm(const fs::path& root, const char* font_path) {
    write(root/"MES/main.mes",{1,0,0,0,3,0,0,0,0,0,0x72,0x9D,0,0});
    Script palette;
    palette.bytes[10]=0x72; palette.bytes[11]=0x9D;
    palette.words(0x6E,{15,200,160,220});
    palette.words(0x6E,{7,230,231,232});
    palette.words(0x6E,{15,999,12,34});
    palette.bytes.push_back(0);
    write(root/"MES/LOG.mes",palette.bytes);
    crx(root/"GRP/SYSTEM/LOG_BACK.CRX",1024,576,false);
    crx(root/"GRP/SYSTEM/LOG_CHIP.CRX",1024,596,true);
    dc4ui::LogScreen log;
    CHECK(log.load(root.string()));
    CHECK(log.visible_rows()==4 && log.row_pitch()==166);
    CHECK(log.last_scroll(4)==0 && log.last_scroll(7)==3);
    CHECK(log.back_hit(1180,650) && !log.back_hit(1150,650));
    CHECK(!log.checkbox_hit(400,50));
    CHECK(log.jump_hit(60,100,2,8)==2);
    CHECK(log.jump_hit(60,385,2,8)==-1);
    CHECK(log.jump_hit(60,100,0,0)==-1);
    CHECK(log.voice_hit(500,50,2,8)==2);
    CHECK(log.voice_hit(500,215,2,8)==3);
    CHECK(log.voice_hit(500,380,2,8)==4);
    CHECK(log.voice_hit(500,550,2,8)==5);
    CHECK(log.voice_hit(500,550,2,5)==-1);
    CHECK(log.voice_hit(1100,50,2,8)==-1);
    CHECK(log.scroll_button(160,180,4,12)==-4);
    CHECK(log.scroll_button(160,660,4,12)==4);
    CHECK(log.scroll_button(1200,120,4,12)==-1);
    CHECK(log.scroll_button(1200,565,4,12)==1);
    CHECK(log.scroll_button(160,180,0,12)==0);
    CHECK(log.scroll_button(160,660,8,12)==0);
    CHECK(log.scroll_button(1200,565,0,4)==0);
    CHECK(log.thumb_scroll(0,7)==0 && log.thumb_scroll(720,7)==3);
    CHECK(!log.thumb_hit(1200,300,0,4) && log.thumb_hit(1200,300,0,7));
    CHECK(log.thumb_scroll(140,12)==0 && log.thumb_scroll(515,12)==8);
    CHECK(log.thumb_scroll(330,12)==5);
    CHECK(log.thumb_scroll(349,10)==4);
    CHECK(log.thumb_scroll(720,0)==0);
    dc4ui::LogRow row; row.voice="voice"; row.name=u"Otome"; row.script="mar_0114_a1.mes";
    const dc4scene::FontRenderer no_font;
    std::vector<uint8_t> frame(1280*720*4,0);
    const auto at = [&](int x, int y) { return pixel(frame,(x*5+3)/4,(y*5+3)/4); };
    log.draw({row},0,true,-1,-1,no_font,&frame);
    CHECK(at(1023,575)==(Color{10,200,70}));
    CHECK(at(370,40)==atlas(398,15));
    CHECK(at(250,40)==atlas(178,143));
    CHECK(at(210,39)==atlas(86,122));
    CHECK(at(28,66)==atlas(0,204));
    CHECK(at(936,504)==atlas(0,0));
    log.draw({row},0,true,60,100,no_font,&frame);
    CHECK(at(28,66)==atlas(0,252));
    struct FaceCase { const char16_t* name; const char* scene; int id; };
    const FaceCase cases[] = {
        {u"音姫","mar",0}, {u"Otome","SUc61_any.MES",19}, {u"Otome","suc60_1110_f1_otm.mes",19},
        {u"Otome","SUC60_1110_F1_OTM.mes",0}, {u"Otome","suc60_1110_f1_otm_extra.mes",0},
        {u"Yume","mar",1}, {u"Koko","mar",2}, {u"Nanaka","mar",3}, {u"Anzu","mar",4},
        {u"Minatsu","mar",5}, {u"Erika","mar",6}, {u"Wataru","mar",7}, {u"Akane","mar",8},
        {u"Maya","mar",9}, {u"Mayuki","mar",10}, {u"Suginami","mar",11}, {u"Sakura","mar",12},
        {u"Mizukoshi","mar",13}, {u"Maika","mar",13}, {u"Yuki","suc75",14},
        {u"由姫","suc75",14}, {u"Yuto","mar",15}, {u"勇斗","mar",15},
        {u"Hiver","mar",16}, {u"Yvel","mar",16}, {u"Yvers","mar",16},
        {u"Yoshiyuki","mar",17}, {u"Rikka","mar",18}, {u"Yuuki","suc60",20},
        {u"桜姫","suc65",21}, {u"Yuuki","MES\\SUC69_0120.MES",21},
        {u"Yuuki","suc71",22}, {u"Yuuki","suc74_0201_a1",22},
        {u"Yuuki","suc74_0325_b1",22}, {u"Yuuki","suc74_0401",23},
        {u"Yuuki","suc75",23}, {u"Harimao","mar",24}, {u"はりまお","mar",24}
    };
    for (const auto& entry : cases) {
        row.name=entry.name; row.script=entry.scene;
        log.draw({row},0,false,-1,-1,no_font,&frame);
        const int sx=entry.id==24 ? 864 : 168+entry.id%6*116;
        const int sy=entry.id==24 ? 480 : 132+entry.id/6*116;
        CHECK(at(250,40)==atlas(sx+10,sy+11));
    }
    row.name=u"Unknown";
    log.draw({row},0,false,-1,-1,no_font,&frame);
    CHECK(at(250,40)==atlas(278,15));
    row.name=u"";
    log.draw({row},0,false,-1,-1,no_font,&frame);
    CHECK(at(250,40)==atlas(278,15));
    CHECK(at(210,31)==atlas(86,122));
    row.name=u"Otome"; row.script="mar";
    auto later=row; later.script="suc75";
    log.draw({row,later,row,later},0,false,-1,-1,no_font,&frame);
    CHECK(at(250,40)==atlas(178,143));
    CHECK(at(250,173)==atlas(294,491));
    CHECK(at(250,306)==atlas(178,143));
    CHECK(at(250,439)==atlas(294,491));
    std::vector<dc4ui::LogRow> history(8,row);
    log.draw(history,4,false,-1,-1,no_font,&frame);
    CHECK(at(948,412)==atlas(908,72));
    CHECK(at(120,136)==atlas(156,0));
    CHECK(at(120,512)==(Color{10,200,70}));
    log.draw(history,0,false,-1,-1,no_font,&frame);
    CHECK(at(948,112)==atlas(908,72));
    CHECK(at(120,136)==(Color{10,200,70}));
    CHECK(at(120,512)==atlas(84,0));
    const auto latin=dc4ui::LogScreen::text_lines(std::u16string(60,u'A'),true,true);
    CHECK(latin.size()==2 && latin[0].size()==50 && latin[1].size()==10);
    const auto japanese=dc4ui::LogScreen::text_lines(std::u16string(30,u'あ'),false,true);
    CHECK(japanese.size()==2 && japanese[0].size()==24 && japanese[1].size()==6);
    dc4scene::FontRenderer font;
    CHECK(font.load(font_path,0,22,false));
    row.text=u"MMMM";
    history.assign(4,row);
    const auto count_color = [&](int top, Color color) {
        int count=0;
        for (int y=top*5/4;y<(top+32)*5/4;++y)
            for (int x=364*5/4;x<410*5/4;++x) count+=pixel(frame,x,y)==color;
        return count;
    };
    log.draw(history,0,false,-1,-1,font,&frame);
    CHECK(count_color(41,{200,160,220})>20);
    CHECK(count_color(440,{230,231,232})>20);
    log.draw(history,0,false,500,50,font,&frame);
    CHECK(count_color(41,{230,231,232})>20);
    CHECK(count_color(440,{200,160,220})>20);
    const auto selected=frame;
    log.draw(history,0,false,-1,-1,font,&frame);
    CHECK(frame==selected);
    log.draw(std::vector<dc4ui::LogRow>(8,row),4,false,-1,-1,font,&frame);
    CHECK(count_color(41,{230,231,232})>20);
    CHECK(count_color(440,{200,160,220})>20);
    CHECK(log.load(root.string()));
    const std::u16string line=u"ijltMWgjpqy"+std::u16string(39,u'M');
    CHECK(line.size()==50);
    row.text=line+line+line+line;
    CHECK(dc4ui::LogScreen::text_lines(row.text,true,true)==std::vector<std::u16string>(4,line));
    history.assign(4,row);
    auto artwork=frame;
    log.draw(history,0,false,-1,-1,no_font,&artwork);
    log.draw(history,0,false,-1,-1,font,&frame);
    int line_ink[4][4]{};
    bool contained=true;
    for (int y=0;y<720;++y) for (int x=0;x<1280;++x) {
        if (pixel(frame,x,y)==pixel(artwork,x,y)) continue;
        bool in_text=false;
        for (int slot=0;slot<4;++slot) for (int line_index=0;line_index<4;++line_index) {
            const int top=(41+slot*133+line_index*24)*5/4;
            if (x>=453 && x<1090 && y>=top && y<top+30) {
                in_text=true; ++line_ink[slot][line_index];
            }
        }
        contained&=in_text;
    }
    CHECK(contained);
    for (const auto& slot : line_ink) for (int count : slot) CHECK(count>100);
    CHECK(count_color(41,{200,160,220})>20);
    CHECK(count_color(440,{230,231,232})>20);
    font_capture("dc2dm-ms-gothic-four-rows",frame);
    log.draw(history,0,false,500,50,font,&frame);
    CHECK(count_color(41,{230,231,232})>20);
    CHECK(count_color(440,{200,160,220})>20);
    font_capture("dc2dm-ms-gothic-selected-first",frame);
    const auto legacy_control = [&](const std::u16string& value, bool latin_text,
                                    const dc4scene::FontRenderer& control_font) {
        auto control=row; control.text=value;
        auto background=frame;
        log.draw({control},0,false,500,50,no_font,&background);
        std::vector<uint8_t> native(1024u*576u*4u,0), expected=background;
        for (int y=0;y<576;++y) for (int x=0;x<1024;++x) {
            const size_t source=(size_t((y*5+3)/4)*1280+(x*5+3)/4)*4;
            std::copy_n(background.data()+source,4,native.data()+(size_t(y)*1024+x)*4);
        }
        int top=41;
        for (const auto& text_line : dc4ui::LogScreen::text_lines(value,latin_text,true)) {
            control_font.rasterize_line(text_line,364,top+(latin_text?18:22),&native,1024,576,
                                       22,0xE6E7E8,false,0,true,-1,latin_text?17:22);
            top+=latin_text?24:32;
        }
        for (int y=0;y<720;++y) for (int x=0;x<1280;++x)
            std::copy_n(native.data()+(size_t(y*4/5)*1024+x*4/5)*4,4,expected.data()+(size_t(y)*1280+x)*4);
        log.draw({control},0,false,500,50,control_font,&frame);
        CHECK(frame==expected);
    };
    legacy_control(u"テスト。あいうえお",false,font);
    dc4scene::FontRenderer ui_font;
    CHECK(ui_font.load(font_path,1,22,false)); CHECK(!ui_font.is_ms_gothic());
    legacy_control(u"Unchanged other font gjpqy",true,ui_font);
    log.set_latin_mode(true);
    log.draw({},0,false,-1,-1,no_font,&artwork);
    log.draw({},0,false,-1,-1,font,&frame);
    bool empty_visible=false, empty_contained=true;
    for (int y=0;y<720;++y) for (int x=0;x<1280;++x) {
        if (pixel(frame,x,y)==pixel(artwork,x,y)) continue;
        empty_visible=true;
        empty_contained&=x>=453 && x<800 && y>=50 && y<80;
    }
    CHECK(empty_visible && empty_contained);
    font_capture("dc2dm-ms-gothic-empty",frame);
    log.set_latin_mode(false);
    row.name=u"Unknown"; row.text.clear(); row.voice.clear();
    auto blank=frame;
    log.draw({row},0,false,-1,-1,no_font,&blank);
    log.draw({row},0,false,-1,-1,font,&frame);
    int left=1280, right=-1, top=720, bottom=-1;
    for (int y=0;y<720;++y) for (int x=0;x<1280;++x) {
        if (pixel(frame,x,y)==pixel(blank,x,y)) continue;
        left=std::min(left,x); right=std::max(right,x);
        top=std::min(top,y); bottom=std::max(bottom,y);
    }
    CHECK(left>=322 && left<=329 && right<391);
    CHECK(top>=54 && bottom<=74 && bottom-top<=18);
    font_capture("dc2dm-ms-gothic-fallback-name",frame);
    row.name=std::u16string(24,u'W'); row.voice="voice";
    log.draw({row},0,false,-1,-1,no_font,&blank);
    log.draw({row},0,false,-1,-1,font,&frame);
    bool name_visible=false, voice_intact=true;
    for (int y=0;y<720;++y) for (int x=0;x<1280;++x) {
        const bool changed=pixel(frame,x,y)!=pixel(blank,x,y);
        if (x>=260 && x<290 && y>=47 && y<77) voice_intact&=!changed;
        else name_visible|=changed;
    }
    CHECK(name_visible && voice_intact);
    font_capture("dc2dm-ms-gothic-voiced-long-name",frame);
    CHECK(!log.load((root/"missing").string()) && !log.loaded());
}
void modern_selection_unchanged(const fs::path& root, const char* font_path) {
    dc4scene::FontRenderer font;
    CHECK(font.load(font_path,0,22,false));
    dc4ui::LogScreen log;
    CHECK(log.load(root.string()));
    dc4ui::LogRow row; row.text=u"MMMM";
    const std::vector<dc4ui::LogRow> rows(4,row);
    std::vector<uint8_t> frame(1280u*720u*4u,0);
    log.draw(rows,0,false,-1,-1,font,&frame);
    const auto original=frame;
    log.select_at(500,215,0,rows.size());
    log.select_top(0,rows.size());
    log.draw(rows,0,false,-1,-1,font,&frame);
    CHECK(frame==original);
}
void dc2dm_selection(const fs::path& root, const char* font_path) {
    dc4scene::FontRenderer font;
    CHECK(font.load(font_path,0,22,false));
    dc4ui::LogScreen log;
    CHECK(log.load(root.string()));
    dc4ui::LogRow row;
    row.name=u"Otome"; row.script="mar"; row.text=u"MMMM";
    std::vector<uint8_t> frame(1280u*720u*4u,0);
    const auto check_selection = [&](int count, int selected) {
        for (int slot=0;slot<count;++slot) {
            int selected_ink=0, regular_ink=0;
            const int top=(41+slot*133)*5/4;
            for (int y=top;y<top+40;++y) for (int x=455;x<513;++x) {
                selected_ink+=pixel(frame,x,y)==(Color{230,231,232});
                regular_ink+=pixel(frame,x,y)==(Color{200,160,220});
            }
            CHECK(slot==selected ? selected_ink>20 && regular_ink==0
                                 : regular_ink>20 && selected_ink==0);
        }
    };
    const int row_y[]{50,215,380,550};
    for (int count=1;count<=4;++count) {
        const std::vector<dc4ui::LogRow> rows(size_t(count),row);
        for (int selected=0;selected<count;++selected) {
            log.select_at(500,row_y[selected],0,rows.size());
            log.draw(rows,0,false,-1,-1,font,&frame);
            check_selection(count,selected);
        }
        log.select_top(0,rows.size());
        log.draw(rows,0,false,-1,-1,font,&frame);
        check_selection(count,0);
        const auto first_selected=frame;
        log.select_at(500,710,0,rows.size());
        log.draw(rows,0,false,-1,-1,font,&frame);
        CHECK(frame==first_selected);
        if (count<4) {
            log.select_at(500,row_y[count],0,rows.size());
            log.draw(rows,0,false,-1,-1,font,&frame);
            CHECK(frame==first_selected);
        }
    }
    std::vector<dc4ui::LogRow> rows(8,row);
    log.select_at(500,550,4,rows.size());
    log.draw(rows,4,false,-1,-1,font,&frame);
    check_selection(4,3);
    log.select_top(4,rows.size());
    log.draw(rows,4,false,-1,-1,font,&frame);
    check_selection(4,0);
    log.select_at(500,215,4,rows.size());
    log.draw(rows,4,false,-1,-1,font,&frame);
    check_selection(4,1);
    const auto second_selected=frame;
    for (const auto point : {std::array<int,2>{-1,-1}, {500,190}, {1200,50}}) {
        log.select_at(point[0],point[1],4,rows.size());
        log.draw(rows,4,false,-1,-1,font,&frame);
        CHECK(frame==second_selected);
    }
    log.select_top(0,0);
    log.select_at(500,50,0,0);
    log.draw({},0,false,-1,-1,font,&frame);
    rows.assign(2,row);
    log.select_top(0,rows.size());
    log.draw(rows,0,false,-1,-1,font,&frame);
    check_selection(2,0);
}

void dc2dm_pretendard(const fs::path& root, const char* gothic_path, const char* pretendard_path) {
    dc4scene::FontRenderer font, gothic;
    CHECK(font.load(pretendard_path,0,22,false) && font.is_pretendard_jp_bold());
    CHECK(gothic.load(gothic_path,0,22,false) && gothic.is_ms_gothic());
    const auto wrap = [&](const std::u16string& text, bool latin = true) {
        return dc4ui::LogScreen::text_lines(text,latin,true,&font);
    };
    CHECK(font.glyph_advance(u'W',22)==18 && font.glyph_advance(u'i',22)==5);
    CHECK(wrap(std::u16string(60,u'W'))==(std::vector<std::u16string>{
        std::u16string(28,u'W'),std::u16string(28,u'W'),std::u16string(4,u'W')}));
    CHECK(wrap(std::u16string(150,u'i'))==(std::vector<std::u16string>{
        std::u16string(101,u'i'),std::u16string(49,u'i')}));
    const std::u16string words=u"Wide WWW words and narrow iii letters retain each word while wrapping. ";
    const std::u16string prose=words+words+words;
    std::u16string joined;
    for (const auto& line : wrap(prose)) { CHECK(font.text_width(line,22)<=506); joined+=line; }
    CHECK(joined==prose);
    CHECK(dc4ui::LogScreen::text_lines(prose,true,true,&gothic)==
          dc4ui::LogScreen::text_lines(prose,true,true));
    CHECK(dc4ui::LogScreen::text_lines(prose,true,false,&font)==
          dc4ui::LogScreen::text_lines(prose,true,false));

    dc4ui::LogScreen log;
    CHECK(log.load(root.string()));
    const dc4scene::FontRenderer no_font;
    dc4ui::LogRow row; row.name=u"Otome"; row.script="mar";
    std::u16string line=u"ijltMWgjpqy";
    while (font.text_width(line+u"W",22)<=506) line+=u'W';
    row.text=line+u"\n"+line+u"\n"+line+u"\n"+line;
    CHECK(wrap(row.text).size()==4);
    std::vector<dc4ui::LogRow> history(4,row);
    std::vector<uint8_t> frame(1280u*720u*4u,0),artwork=frame;
    log.select_top(0,history.size());
    log.draw(history,0,false,-1,-1,no_font,&artwork);
    log.draw(history,0,false,-1,-1,font,&frame);
    int ink[4][4]{},bright_top[4][4],bright_bottom[4][4];
    for (int slot=0;slot<4;++slot) for (int n=0;n<4;++n) {
        bright_top[slot][n]=720; bright_bottom[slot][n]=-1;
    }
    bool contained=true;
    for (int y=0;y<720;++y) for (int x=0;x<1280;++x) {
        if (pixel(frame,x,y)==pixel(artwork,x,y)) continue;
        bool in_text=false;
        for (int slot=0;slot<4;++slot) for (int n=0;n<4;++n) {
            const int top=(41+slot*133+n*24)*5/4;
            if (x<453 || x>=1094 || y<top || y>=top+30) continue;
            in_text=true; ++ink[slot][n];
            if (pixel(frame,x,y)==(slot==0 ? Color{230,231,232} : Color{200,160,220})) {
                bright_top[slot][n]=std::min(bright_top[slot][n],y);
                bright_bottom[slot][n]=std::max(bright_bottom[slot][n],y);
            }
        }
        contained&=in_text;
    }
    CHECK(contained);
    for (int slot=0;slot<4;++slot) for (int n=0;n<4;++n) {
        CHECK(ink[slot][n]>500);
        CHECK(bright_bottom[slot][n]-bright_top[slot][n]+1>=20);
    }
    font_capture("dc2dm-pretendard-four-rows",frame);

    const auto changed_bounds = [&] {
        std::array<int,4> bounds{1280,720,-1,-1};
        for (int y=0;y<720;++y) for (int x=0;x<1280;++x) {
            if (pixel(frame,x,y)==pixel(artwork,x,y)) continue;
            bounds[0]=std::min(bounds[0],x); bounds[1]=std::min(bounds[1],y);
            bounds[2]=std::max(bounds[2],x); bounds[3]=std::max(bounds[3],y);
        }
        return bounds;
    };
    row.text.clear();
    int narrow_width=0;
    for (const auto& name : {u"iiiiiii",u"WWWWWWW",u"Unknown",u"Yoshiyuki Jr."}) {
        row.name=name;
        log.draw({row},0,false,-1,-1,no_font,&artwork);
        log.draw({row},0,false,-1,-1,font,&frame);
        const auto bounds=changed_bounds();
        CHECK(bounds[0]>=250 && bounds[2]<455 && bounds[1]>=50 && bounds[3]<80);
        CHECK(std::abs(bounds[0]+bounds[2]-720)<=10);
        if (row.name==u"iiiiiii") narrow_width=bounds[2]-bounds[0]+1;
        if (row.name==u"WWWWWWW") CHECK(bounds[2]-bounds[0]+1>3*narrow_width);
    }
    font_capture("dc2dm-pretendard-fallback-name",frame);
    row.name=std::u16string(24,u'W'); row.voice="voice";
    log.draw({row},0,false,-1,-1,no_font,&artwork);
    log.draw({row},0,false,-1,-1,font,&frame);
    const auto long_name=changed_bounds();
    CHECK(long_name[0]>=250 && long_name[2]<455 && long_name[1]>=50 && long_name[3]<80);
    CHECK(long_name[2]-long_name[0]>150);
    bool voice_intact=true;
    for (int y=47;y<77;++y) for (int x=260;x<290;++x)
        voice_intact&=pixel(frame,x,y)==pixel(artwork,x,y);
    CHECK(voice_intact);
    font_capture("dc2dm-pretendard-voiced-long-name",frame);

    row.name=u"Otome"; row.voice.clear();
    const std::vector<std::u16string> japanese{
        std::u16string(23,u'あ'),std::u16string(23,u'い'),std::u16string(23,u'う')};
    row.text=japanese[0]+u"\n"+japanese[1]+u"\n"+japanese[2];
    CHECK(wrap(row.text,false)==japanese);
    CHECK(wrap(std::u16string(24,u'あ'),false).size()==2);
    log.draw({row},0,false,-1,-1,no_font,&artwork);
    log.draw({row},0,false,-1,-1,font,&frame);
    const auto jp=changed_bounds();
    CHECK(jp[0]>=453 && jp[2]<1094 && jp[1]>=49 && jp[3]<181);
    for (int n=0;n<3;++n) {
        int count=0;
        for (int y=(41+n*32)*5/4;y<(73+n*32)*5/4;++y)
            for (int x=453;x<1094;++x) count+=pixel(frame,x,y)!=pixel(artwork,x,y);
        CHECK(count>500);
    }
    font_capture("dc2dm-pretendard-japanese",frame);
    log.set_latin_mode(true);
    log.draw({},0,false,-1,-1,no_font,&artwork);
    log.draw({},0,false,-1,-1,font,&frame);
    const auto empty=changed_bounds();
    CHECK(empty[0]>=453 && empty[2]>600 && empty[2]<1094 && empty[1]>=50 && empty[3]<82);
    font_capture("dc2dm-pretendard-empty",frame);
}
}
int main(int argc, char** argv) {
    if (argc<3) { std::cerr<<"Pass MSGOTHIC.TTC and PretendardJP-Bold.ttf for backlog text checks.\n"; return 1; }
    const auto parent=fs::canonical(fs::temp_directory_path());
    const auto root=parent/("dc4-log-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        if (!fs::create_directory(root)) throw std::runtime_error("Cannot create isolated log fixture");
        crx(root/"GRP/SYSTEM/BKLOGBACK.CRX",1280,720,false);
        crx(root/"GRP/SYSTEM/BKLOGCHIP.CRX",1224,854,true);
        text_layout(); row_art(root); capture(root); voice_associations(root);
        modern_selection_unchanged(root,argv[1]); dc2dm(root,argv[1]);
        dc2dm_selection(root,argv[1]);
        dc2dm_pretendard(root,argv[1],argv[2]);
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; ++failures; }
    std::error_code ec;
    if (fs::weakly_canonical(root,ec)==root && !ec && root.parent_path()==parent)
        fs::remove_all(root,ec);
    std::cout<<(failures?"Log regressions FAILED\n":"Log regressions passed\n");
    return failures ? 1 : 0;
}

