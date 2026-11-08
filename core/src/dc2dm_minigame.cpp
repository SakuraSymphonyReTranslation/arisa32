#include "arisa32/dc2dm_minigame.h"
#include "arisa32/dc2dm_reversi.h"
#include "arisa32/dc2dm_suit.h"
#include "arisa32/resource.h"
#include "arisa32/scene.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>

namespace dc4scene {
namespace {
struct Rect {
    int x, y, w, h;
    bool contains(int px, int py) const { return px >= x && py >= y && px < x+w && py < y+h; }
};
constexpr Rect difficulty[] = {{164,240,184,96}, {420,240,184,96}, {676,240,184,96}};
constexpr Rect order[] = {{250,240,184,96}, {592,240,184,96}};
constexpr Rect quit_buttons[] = {{290,240,184,96}, {552,240,184,96}};
int scaled(int value) { return int(std::lround(value * 1.25)); }

void blit(std::vector<uint8_t>& frame, const Image& source, Rect src, int dx, int dy) {
    if (!source.valid()) return;
    const int x0=scaled(dx), y0=scaled(dy), w=scaled(src.w), h=scaled(src.h);
    for (int y=0; y<h; ++y) {
        if (y0+y<0 || y0+y>=kScreenH) continue;
        const int sy=src.y+y*src.h/h;
        if (sy<0 || sy>=source.h) continue;
        for (int x=0; x<w; ++x) {
            if (x0+x<0 || x0+x>=kScreenW) continue;
            const int sx=src.x+x*src.w/w;
            if (sx<0 || sx>=source.w) continue;
            const auto* s=source.rgba.data()+(size_t(sy)*source.w+sx)*4;
            auto* d=frame.data()+(size_t(y0+y)*kScreenW+x0+x)*4;
            for (int c=0;c<3;++c) d[c]=uint8_t((s[c]*s[3]+d[c]*(255-s[3]))/255);
            d[3]=255;
        }
    }
}
std::u16string number(int value, int width=0) {
    auto s=std::to_string(value);
    if (int(s.size())<width) s.insert(0,size_t(width)-s.size(),'0');
    return std::u16string(s.begin(),s.end());
}
void text(const FontRenderer* font, std::vector<uint8_t>& frame, const std::u16string& str,
          int x, int y, int size=22) {
    if (font) font->rasterize_line(str,scaled(x),scaled(y+size-3),&frame,kScreenW,kScreenH,
                                  float(size)*1.25f,0xffffff,false,0,true);
}
}

struct Dc2dmMinigame::Impl {
    Stage stage=Stage::Finished;
    int kind=0, wins=0, losses=0, level=0, selected=0, pressed=-1, face=0;
    int result=0, moves=0, saved_selection=0;
    bool quit=false, animated=false, boot_skippable=false, music_start=false;
    uint32_t random=1, delay=0, console_ticks=0;
    Dc2dmReversi board;
    Dc2dmReversi::Board before_flip{}, visible_board{};
    std::array<int,64> flip_distances{};
    int flip_move=-1, flip_step=0, flip_last=0;
    uint32_t flip_ticks=0;
    Dc2dmReversi::Cell flip_color=Dc2dmReversi::Empty;
    Dc2dmReversi::Cell player=Dc2dmReversi::Black;
    Dc2dmSuit suit;
    Image face_sheet, board_sheet, chip_sheet, button_sheet, result_sheet;
    std::vector<std::u16string> log;
    std::vector<std::u16string> console;
    std::deque<std::u16string> pending_console;
    std::deque<int> sound_effects;

    void console_row() {
        if (console.size()==25) console.erase(console.begin());
        console.push_back(std::move(pending_console.front()));
        pending_console.pop_front();
    }
    void collect_console() {
        auto batch=suit.take_console_batch();
        if (batch.reset) { console.clear(); pending_console.clear(); }
        for (auto& line:batch.lines) pending_console.push_back(std::move(line));
        console_ticks=0;
        if (!animated) while (!pending_console.empty()) console_row();
    }
    void finish_boot() {
        delay=0; music_start=true; pressed=-1; selected=0;
        stage=kind==0 ? Stage::Suit : Stage::Difficulty;
        collect_console();
    }

    int hit(int x, int y) const {
        if (x<0 || y<0 || x>=1024 || y>=576) return -1;
        if (quit) {
            for (int i=0;i<2;++i) if (quit_buttons[i].contains(x,y)) return i;
            return -1;
        }
        if (stage==Stage::Boot) return boot_skippable ? 0 : -1;
        if (stage==Stage::Suit && pending_console.empty()) {
            const auto rows=suit.choice_rows();
            for (size_t i=0;i<rows.size();++i)
                if (Rect{0,rows[i]*22,220,22}.contains(x,y)) return int(i);
        } else if (stage==Stage::Difficulty) {
            for (int i=0;i<3;++i) if (difficulty[i].contains(x,y)) return i;
        } else if (stage==Stage::Order || stage==Stage::Replay) {
            for (int i=0;i<2;++i) if (order[i].contains(x,y)) return i;
        } else if (stage==Stage::Playing && x>=192 && x<768) {
            return (y/72)*8+(x-192)/72;
        } else if (stage==Stage::Result && !delay) return 0;
        return -1;
    }
    void append(std::u16string line) {
        log.push_back(std::move(line));
        if (log.size()>35) log.erase(log.begin());
    }
    void after_move(int index, Dc2dmReversi::Cell color) {
        if (index>=0) {
            ++moves;
            append(u"黒:"+number(board.count(Dc2dmReversi::Black),2)+u" 白:"+number(board.count(Dc2dmReversi::White),2));
            for (int y=0;y<8;++y) {
                std::u16string row;
                for (int x=0;x<8;++x) row+=number(int(board.cell(x,y)),2)+u",";
                append(std::move(row));
            }
        }
        if (board.passed_turn()!=Dc2dmReversi::Empty) append(u"> Lewat");
        const int difference=board.count(player)-board.count(Dc2dmReversi::opponent(player));
        face=difference>0 ? 1 : difference<0 ? 2 : 0;
        pressed=-1;
        if (board.finished()) {
            result=board.result_for(player);
            if (result==1) wins=std::min(255,wins+1);
            if (result==2) losses=std::min(255,losses+1);
            face=result==1 ? 1 : result==2 ? 2 : 0;
            append(result==1 ? u"> Anda Menang" : result==2 ? u"> CPU Menang" : u"> Seri");
            stage=Stage::Result; delay=2000; selected=0;
        } else if (board.turn()!=player) {
            append(u"> Giliran CPU");
            stage=Stage::Thinking; delay=1000;
        } else {
            if(index>=0)append(u"> Giliran Anda");
            stage=Stage::Playing; delay=0;
        }
    }
    bool play_move(int index) {
        const auto color=board.turn();
        const auto captured=board.flips(index,color);
        if (captured.empty()) return false;
        before_flip=board.board();
        if (!board.play(index)) return false;
        const int x=index%8, y=index/8;
        sound_effects.push_back((x==0 || x==7) && (y==0 || y==7) ? 399 : 398);
        if (!animated) { after_move(index,color); return true; }
        visible_board=before_flip;
        visible_board[index]=color;
        flip_distances.fill(0);
        flip_move=index; flip_color=color; flip_step=flip_last=0; flip_ticks=0;
        for (int square:captured) {
            const int distance=std::max(std::abs(square%8-x),std::abs(square/8-y));
            flip_distances[square]=distance;
            flip_last=std::max(flip_last,distance);
        }
        pressed=-1; delay=color==player ? 1000 : 0; stage=Stage::Flipping;
        return true;
    }
    void accept(int item) {
        if (item<0) return;
        pressed=-1;
        if (quit) {
            if (item==0) stage=Stage::Finished;
            quit=false; selected=saved_selection;
            return;
        }
        if (stage==Stage::Boot) { if (boot_skippable) finish_boot(); }
        else if (stage==Stage::Suit && pending_console.empty()) {
            if (suit.stage()==Dc2dmSuitStage::Choosing) {
                random=random*214013u+2531011u;
                suit.play(static_cast<Dc2dmSuitHand>(item),static_cast<Dc2dmSuitHand>(((random>>16)&32767)%3));
            } else suit.replay(item==0);
            wins=suit.wins(); losses=suit.losses(); face=suit.face_index(); selected=0;
            collect_console();
            if (suit.stage()==Dc2dmSuitStage::Finished) stage=Stage::Finished;
        } else if (stage==Stage::Difficulty && item<3) {
            level=item; stage=Stage::Order; selected=0;
        } else if (stage==Stage::Order && item<2) {
            player=item==0 ? Dc2dmReversi::Black : Dc2dmReversi::White;
            selected=0;
            after_move(-1,board.turn());
        } else if (stage==Stage::Playing && item<64) {
            const auto color=board.turn();
            if (color==player) { selected=item; play_move(item); }
        } else if (stage==Stage::Result && !delay) {
            stage=Stage::Replay; selected=0;
        } else if (stage==Stage::Replay) {
            if (item==1) stage=Stage::Finished;
            else { board.reset(); moves=face=selected=0; log.clear(); stage=Stage::Difficulty; }
        }
    }
};

Dc2dmMinigame::Dc2dmMinigame() : impl_(std::make_unique<Impl>()) {}
Dc2dmMinigame::~Dc2dmMinigame()=default;

bool Dc2dmMinigame::start(int kind, int wins, int losses, uint32_t seed,
                         const ImageLoader& load, std::string* error,
                         bool animated, bool boot_skippable) {
    impl_=std::make_unique<Impl>();
    auto& p=*impl_;
    if (kind<0 || kind>1) { if(error)*error="Unknown DC2DM minigame"; return false; }
    p.kind=kind; p.wins=std::clamp(wins,0,255); p.losses=std::clamp(losses,0,255); p.random=seed;
    p.animated=animated; p.boot_skippable=boot_skippable;
    const std::string dir=kind==0 ? "MG/JK/" : "MG/REVERSI/";
    if (!load(dir+"FACE",&p.face_sheet,error) ||
        !load("MG/REVERSI/M_BOTAN_OFF",&p.button_sheet,error)) return false;
    if (kind==1 && (!load(dir+"REVERSI_BOARD",&p.board_sheet,error) ||
                   !load(dir+"REVERSI_CHIP",&p.chip_sheet,error) ||
                   !load(dir+"REVERSI_RESULT",&p.result_sheet,error))) return false;
    if (p.face_sheet.w<768 || p.face_sheet.h<288 || p.button_sheet.w<552 || p.button_sheet.h<1152 ||
        (kind==1 && (p.board_sheet.w<576 || p.board_sheet.h<576 || p.chip_sheet.w<216 ||
                    p.chip_sheet.h<72 || p.result_sheet.w<480 || p.result_sheet.h<540))) {
        if (error) *error="Incomplete DC2DM minigame artwork";
        return false;
    }
    p.suit.start(p.wins,p.losses); p.board.set_random_seed(seed);
    if (animated) { p.stage=Stage::Boot; p.delay=4500; }
    else p.finish_boot();
    if (error) error->clear();
    return true;
}

void Dc2dmMinigame::update(uint32_t elapsed_ms) {
    auto& p=*impl_;
    if (!active() || p.quit) return;
    if (p.stage==Stage::Suit && !p.pending_console.empty()) {
        const uint64_t ticks=uint64_t(p.console_ticks)+uint64_t(elapsed_ms)*30;
        size_t rows=std::min<uint64_t>(ticks/1000,p.pending_console.size());
        p.console_ticks=uint32_t(ticks%1000);
        while (rows--) p.console_row();
        return;
    }
    if (p.stage==Stage::Flipping) {
        if (p.flip_color==p.player) p.delay=elapsed_ms>=p.delay ? 0 : p.delay-elapsed_ms;
        const uint64_t ticks=uint64_t(p.flip_ticks)+elapsed_ms;
        if (ticks<=100) { p.flip_ticks=uint32_t(ticks); return; }
        // Native 0x43C4A0 recolors one capture-distance wave after strictly more than 100 ms.
        p.flip_ticks=0;
        ++p.flip_step;
        for (int square=0;square<64;++square)
            if (p.flip_distances[square]==p.flip_step) p.visible_board[square]=p.flip_color;
        p.sound_effects.push_back(398);
        if (p.flip_step==p.flip_last) {
            const uint32_t remaining=p.delay;
            p.after_move(p.flip_move,p.flip_color);
            if (p.stage==Stage::Thinking && p.flip_color==p.player) p.delay=remaining;
        }
        return;
    }
    if (!p.delay && p.stage!=Stage::Thinking) return;
    p.delay=elapsed_ms>=p.delay ? 0 : p.delay-elapsed_ms;
    if (!p.delay && p.stage==Stage::Boot) p.finish_boot();
    if (!p.delay && p.stage==Stage::Thinking) {
        const auto color=p.board.turn();
        const int move=p.board.choose_move_for_level(color,p.level);
        if (move>=0) p.play_move(move);
        else { p.board.pass(); p.after_move(move,color); }
    }
}

void Dc2dmMinigame::pointer(int action, int x, int y) {
    auto& p=*impl_;
    if (!active()) return;
    const int hit=p.hit(x<0 ? -1 : x*4/5,y<0 ? -1 : y*4/5);
    if (action==0) { p.pressed=hit; if(hit>=0)p.selected=hit; }
    else if (action==2) { if(hit>=0)p.selected=hit; }
    else if (action==1) { const int down=p.pressed; p.pressed=-1; if(hit>=0 && hit==down)p.accept(hit); }
    else p.pressed=-1;
}

void Dc2dmMinigame::key(Key key) {
    auto& p=*impl_;
    if (!active()) return;
    p.pressed=-1;
    if (p.stage==Stage::Boot) {
        if (p.boot_skippable && (key==Key::Accept || key==Key::Cancel)) p.finish_boot();
        return;
    }
    if (key==Key::Cancel) {
        if (!p.quit) { p.saved_selection=p.selected; p.selected=1; }
        else p.selected=p.saved_selection;
        p.quit=!p.quit;
        return;
    }
    if (!p.quit && (p.stage==Stage::Flipping || p.stage==Stage::Thinking)) return;
    if (key==Key::Accept) { p.accept(p.selected); return; }
    const int direction=(key==Key::Left || key==Key::Up || key==Key::Previous) ? -1 : 1;
    if (!p.quit && p.stage==Stage::Playing) {
        if (key==Key::Up || key==Key::Down) p.selected=(p.selected+direction*8+64)%64;
        else p.selected=(p.selected+direction+64)%64;
    } else {
        const int count=p.quit ? 2 : p.stage==Stage::Difficulty ? 3 : p.stage==Stage::Suit ? int(p.suit.choice_rows().size()) : 2;
        if (count) p.selected=(p.selected+direction+count)%count;
    }
}

void Dc2dmMinigame::draw(const FontRenderer* font, std::vector<uint8_t>* target) const {
    if (!target) return;
    auto& frame=*target;
    frame.assign(size_t(kScreenW)*kScreenH*4,0);
    for(size_t i=3;i<frame.size();i+=4)frame[i]=255;
    const auto& p=*impl_;
    if (!active()) return;
    if (p.stage==Stage::Boot) {
        text(font,frame,p.kind==0 ? u"Pelatihan Program Laboratorium Amakase, Tugas 4: Game Janken" :
             u"Pelatihan Program Laboratorium Amakase, Tugas 9: Reversi",0,0,16);
        const unsigned tick=(4500-p.delay)/500;
        if (tick%2==0) text(font,frame,u"Memulai...",0,16,16);
    } else if (p.kind==0) {
        const auto& lines=p.console;
        for (size_t i=0;i<lines.size();++i) text(font,frame,lines[i],0,int(i)*22);
        const auto rows=p.suit.choice_rows();
        if (p.pending_console.empty() && p.selected>=0 && p.selected<int(rows.size()))
            text(font,frame,u"≫",0,rows[p.selected]*22);
    } else {
        for(size_t i=0;i<p.log.size();++i) text(font,frame,p.log[i],0,int(i)*16,16);
        blit(frame,p.board_sheet,{0,0,576,576},192,0);
        for (int i=0;i<64;++i) {
            const auto cell=p.stage==Stage::Flipping ? p.visible_board[i] : p.board.cell(i%8,i/8);
            if (cell!=Dc2dmReversi::Empty)
                blit(frame,p.chip_sheet,{(cell==Dc2dmReversi::Black ? 0 : 72),0,72,72},192+(i%8)*72,(i/8)*72);
        }
        if (p.stage==Stage::Playing) {
            for (int move:p.board.legal_moves(p.player))
                blit(frame,p.chip_sheet,{p.player==Dc2dmReversi::Black ? 20 : 92,20,32,32},192+(move%8)*72+20,(move/8)*72+20);
        }
        if(p.stage==Stage::Playing || p.stage==Stage::Thinking || p.stage==Stage::Flipping) {
            const int cursor=p.quit ? p.saved_selection : p.selected;
            blit(frame,p.chip_sheet,{145,0,72,70},192+(cursor%8)*72+2,(cursor/8)*72+2);
        }
        if(p.stage!=Stage::Difficulty && p.stage!=Stage::Order) {
            text(font,frame,p.player==Dc2dmReversi::Black ? u"プレイヤー：黒" : u"プレイヤー：白",768,16);
            text(font,frame,p.player==Dc2dmReversi::Black ? u"ＣＰＵ：白" : u"ＣＰＵ：黒",768,38);
            const auto& counted=p.stage==Stage::Flipping ? p.visible_board : p.board.board();
            text(font,frame,u"黒："+number(int(std::count(counted.begin(),counted.end(),Dc2dmReversi::Black)),2)+u"個",768,60);
            text(font,frame,u"白："+number(int(std::count(counted.begin(),counted.end(),Dc2dmReversi::White)),2)+u"個",768,82);
            text(font,frame,u"手数："+number(p.moves,2),768,104);
        }
    }
    if (p.stage!=Stage::Boot) blit(frame,p.face_sheet,{p.face*256,0,256,288},768,288);
    auto button=[&](Rect r,int source_y,int index) {
        const int state=!p.quit && p.selected==index ? 1 : 0;
        blit(frame,p.button_sheet,{state*184,source_y,184,96},r.x,r.y);
    };
    if(p.stage==Stage::Difficulty)for(int i=0;i<3;++i)button(difficulty[i],288+i*96,i);
    if(p.stage==Stage::Order)for(int i=0;i<2;++i)button(order[i],i*96,i);
    if(p.stage==Stage::Replay)for(int i=0;i<2;++i)button(order[i],576+i*96,i);
    if(p.stage==Stage::Thinking || (p.stage==Stage::Flipping && p.flip_color==p.player))
        blit(frame,p.button_sheet,{0,192,184,96},420,240);
    if(p.stage==Stage::Result)blit(frame,p.result_sheet,{0,(p.result==1 ? 0 : p.result==2 ? 180 : 360),480,180},272,198);
    if(p.quit) {
        blit(frame,p.button_sheet,{0,960,552,192},236,152);
        for(int i=0;i<2;++i) {
            const int state=p.selected==i ? 1 : 0;
            blit(frame,p.button_sheet,{state*184,i==0 ? 864 : 768,184,96},quit_buttons[i].x,quit_buttons[i].y);
        }
    }
}

bool Dc2dmMinigame::active() const { return impl_->stage!=Stage::Finished; }
bool Dc2dmMinigame::confirming_exit() const { return impl_->quit; }
bool Dc2dmMinigame::take_music_start() { const bool start=impl_->music_start; impl_->music_start=false; return start; }
int Dc2dmMinigame::take_sound_effect() {
    if (impl_->sound_effects.empty()) return -1;
    const int sound=impl_->sound_effects.front();
    impl_->sound_effects.pop_front();
    return sound;
}
Dc2dmMinigame::Stage Dc2dmMinigame::stage() const { return impl_->stage; }
int Dc2dmMinigame::kind() const { return impl_->kind; }
int Dc2dmMinigame::wins() const { return impl_->wins; }
int Dc2dmMinigame::losses() const { return impl_->losses; }
const Dc2dmReversi& Dc2dmMinigame::reversi() const { return impl_->board; }

std::string dc2dm_minigame_score_text(const std::string& input,int wins,int losses) {
    std::string result;
    std::u16string decoded;
    const bool unicode=dc4::decode_utf8(input,&decoded);
    for(size_t i=0;i<input.size();) {
        const bool marker=!unicode && i+1<input.size() && uint8_t(input[i])==0x81 && (uint8_t(input[i+1])==0x96 || uint8_t(input[i+1])==0x97);
        const bool utf8=unicode && i+2<input.size() && uint8_t(input[i])==0xef &&
            ((uint8_t(input[i+1])==0xbc && uint8_t(input[i+2])==0x8a) ||
             (uint8_t(input[i+1])==0xbc && uint8_t(input[i+2])==0xa0));
        if(marker || utf8) {
            const bool win=marker ? uint8_t(input[i+1])==0x96 : uint8_t(input[i+2])==0x8a;
            const auto value=std::to_string(std::clamp(win ? wins : losses,0,255));
            for(char c:value) {
                if(utf8) { result.push_back(char(0xef));result.push_back(char(0xbc));result.push_back(char(0x90+c-'0')); }
                else { result.push_back(char(0x82));result.push_back(char(0x4f+c-'0')); }
            }
            i+=marker ? 2 : 3;
        } else {
            const auto c=uint8_t(input[i]);
            const size_t n=!unicode && ((c>=0x81 && c<=0x9f) || (c>=0xe0 && c<=0xfc)) && i+1<input.size() ? 2 : 1;
            result.append(input,i,n); i+=n;
        }
    }
    return result;
}
}
