#include "arisa32/mesvm.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

void restore(dc4::MesVm& vm, const dc4::MesFile& file) {
    dc4::MesVmSnapshot saved;
    std::string error;
    require(vm.capture_snapshot(&saved, &error), error);
    vm.reset();
    require(vm.bind(file), vm.error());
    require(vm.restore_snapshot(saved, &error), error);
}

void service_regressions() {
    const struct { std::array<int, 4> points; int route; } cases[] = {
        {{0,0,0,0},0}, {{4,0,0,0},0}, {{0,4,0,0},1},
        {{0,0,4,0},2}, {{0,0,0,4},3}, {{1,1,1,1},0},
        {{2,2,0,0},0}, {{2,0,2,0},0}, {{2,0,0,2},0},
        {{0,2,2,0},1}, {{0,2,0,2},1}, {{0,0,2,2},2},
        {{1,0,3,0},2}, {{0,1,0,3},3}, {{0,128,255,254},2}
    };
    for (uint32_t version : {0xAAB6u, 0xABB6u}) {
        dc4::MesFile file;
        file.version = version;
        file.code = {0xB6, uint8_t(version >> 8), 0, 0x1E, 2, 0, 0};
        for (const auto& test : cases) {
            dc4::MesVm vm(file);
            vm.write_variable(3, 0, 99);
            vm.write_variable(3, 1, 77);
            vm.write_variable(5, 4, 255); // Later heroine points are outside this ranking.
            for (uint16_t i = 0; i < 4; ++i) vm.write_variable(5, i, test.points[i]);
            restore(vm, file);
            dc4::MesCommand command;
            while (vm.next(&command)) {}
            require(vm.error().empty() && vm.ended(), "route service did not finish");
            require(vm.read_variable(3, 0) == test.route, "route service returned the wrong group");
            require(vm.read_variable(3, 1) == 77 && vm.read_variable(5, 4) == 255,
                    "route service changed unrelated variables");
            for (uint16_t i = 0; i < 4; ++i)
                require(vm.read_variable(5, i) == test.points[i], "route service changed map points");
            restore(vm, file);
            require(vm.read_variable(3, 0) == test.route, "saved route result was lost");
        }
    }
    // ClassicDM shares the service ID, but its subcommands are game-specific.
    dc4::MesFile legacy;
    legacy.dialect = dc4::MesDialect::ClassicDM;
    legacy.version = 0x9D72;
    legacy.code = {0x72,0x9D,0,0x1B,2,0,0};
    dc4::MesVm vm(legacy);
    vm.write_variable(3, 0, 99);
    vm.write_variable(5, 2, 4);
    dc4::MesCommand command;
    require(vm.next(&command) && command.op == 0x1B, "legacy service was consumed");
    require(vm.read_variable(3, 0) == 99, "DC4 ranking leaked into ClassicDM");
    dc4::MesFile other;
    other.code = {0,0,0,0x1E,2,0,0};
    require(vm.bind(other), vm.error());
    require(vm.next(&command) && command.op == 0x1E, "unverified modern service was consumed");
    require(vm.read_variable(3, 0) == 99, "DC4 ranking leaked into another modern version");
    other.version = 0xAAB6;
    other.code = {0xB6,0xAA,0,0x1E,9,0,0};
    require(vm.bind(other), vm.error());
    require(vm.next(&command) && command.p1 == 9, "credits service was consumed");
    require(vm.read_variable(3, 0) == 99, "credits service changed route variables");
}

void fresh_progress() {
    for (uint32_t version : {0xAAB6u, 0xABB6u}) {
        dc4::MesFile file;
        file.version = version;
        file.code = {0xB6, uint8_t(version >> 8), 0, 0};
        dc4::MesVm vm(file);
        for (uint16_t i = 100; i < 150; ++i)
            require(vm.read_variable(100, i) == 0, "fresh game starts with completed endings");
        vm.write_variable(100, 101, 1);
        restore(vm, file);
        require(vm.read_variable(100, 101) == 1 && vm.read_variable(100, 100) == 0,
                "snapshot changed earned or locked progression");
    }
}

std::string lower(std::string name) {
    for (char& c : name) c = char(std::tolower(static_cast<unsigned char>(c)));
    return name;
}

struct Scripts {
    std::filesystem::path root;
    std::map<std::string, dc4::MesFile> files;
    const dc4::MesFile& get(const std::string& name) {
        auto found = files.find(name);
        if (found != files.end()) return found->second;
        dc4::MesFile file;
        std::string error;
        require(file.load_file((root / (name + ".mes")).string(), &error), error);
        return files.emplace(name, std::move(file)).first->second;
    }
};

const std::array<std::string,4> maps = {
    "dc4_kyo20190208c1_m0", "dc4_kyo20190209b_m0",
    "dc4_kyo20190211b_m0", "dc4_kyo20190212c_m0"
};
const std::array<std::string,4> groups = {
    "dc4_mia20190301a", "dc4_mib20190301a",
    "dc4_mic20190301a", "dc4_mid20190301a1"
};

void map_path(Scripts& scripts, const std::array<int,4>& picks, bool unlocked, bool saved) {
    dc4::MesVm vm;
    std::string name = maps[0];
    require(vm.bind(scripts.get(name)), vm.error());
    vm.write_variable(100, 112, unlocked ? 1 : 0);
    std::array<int,4> points{};
    size_t selections = 0;
    std::string map_background;
    for (size_t steps = 0; steps < 100000; ++steps) {
        dc4::MesCommand command;
        if (vm.next(&command)) {
            if(command.op==0x49 && lower(command.str).rfind("bg\\map_",0)==0)
                map_background=lower(command.str);
            if (!vm.transferring()) continue;
            const auto next = lower(command.str);
            const auto group = std::find(groups.begin(), groups.end(), next);
            if (group != groups.end()) {
                require(selections == 4, "reached group before four maps");
                const int expected = int(std::max_element(points.begin(), points.end()) - points.begin());
                require(next == groups[expected], "map path entered " + next + " instead of " + groups[expected]);
                require(vm.read_variable(3, 0) == expected, "map route result was not stored");
                for (uint16_t i = 0; i < 4; ++i)
                    require(vm.read_variable(5, i) == points[i], "map score changed before group entry");
                return;
            }
            name = next;
            require(vm.bind(scripts.get(name)), vm.error());
        } else if (vm.waiting_for_choice()) {
            require(selections < 4 && name == maps[selections], "unexpected map choice in " + name);
            require(vm.choices().size() == (unlocked ? 4u : 3u), "wrong locked/unlocked map options");
            const char* backgrounds[]={"bg\\map_gaku_0208c","bg\\map_sima_0209b",
                "bg\\map_sima_0211b","bg\\map_gaku_0212c"};
            require(map_background==std::string(backgrounds[selections])+(unlocked?"_asa":""),
                    "map artwork exposes the wrong heroine/location markers");
            require(!vm.choose(vm.choices().size()),"map accepted a nonexistent trigger");
            for (uint16_t i = 0; i < 4; ++i)
                require(vm.read_variable(5, i) == points[i], "previous map awarded the wrong score");
            if (saved) restore(vm, scripts.get(name));
            const int pick = picks[selections];
            // The first map's labels differ from both display order and score indices.
            const int first_labels[] = {3,1,2,4};
            const auto label = "label0" + std::to_string(selections ? pick + 1 : first_labels[pick]);
            const auto& choices = vm.choices();
            const auto choice = std::find_if(choices.begin(), choices.end(), [&](const dc4::MesChoice& c) {
                return c.label == label;
            });
            require(choice != choices.end(), "missing map target " + label);
            require(vm.choose(size_t(choice - choices.begin())), vm.error());
            ++points[pick];
            ++selections;
        } else {
            throw std::runtime_error("map path stopped in " + name + ": " + vm.error());
        }
    }
    throw std::runtime_error("map path exceeded execution budget");
}

void heroine_branches(Scripts& scripts) {
    const struct { const char* script; int left, right; const char* higher; const char* otherwise; } cases[] = {
        {"dc4_mia20190315e",4,0,"dc4_sor20190316a","dc4_nin20190316a"},
        {"dc4_mib20190315d",5,1,"dc4_miu20190315d","dc4_hiy20190315d1"},
        {"dc4_mic20190315d",6,2,"dc4_tiy20190316b","dc4_sii20190316a"},
        {"dc4_mid20190421a",7,3,"dc4_asu20190429b","dc4_mid20190428b"}
    };
    for (const auto& test : cases) {
        const auto& file = scripts.get(test.script);
        for (const auto& scores : {std::pair<int,int>{3,1}, {1,3}, {2,2}}) {
            dc4::MesVm vm(file);
            vm.write_variable(5, uint16_t(test.left), scores.first);
            vm.write_variable(5, uint16_t(test.right), scores.second);
            restore(vm, file);
            bool transferred = false;
            for (size_t steps = 0; steps < 100000; ++steps) {
                dc4::MesCommand command;
                if (!vm.next(&command)) break;
                if (!vm.transferring()) continue;
                const auto expected = scores.first > scores.second ? test.higher : test.otherwise;
                require(lower(command.str) == expected, std::string(test.script) + " entered " + command.str);
                transferred = true;
                break;
            }
            require(transferred && vm.error().empty(), std::string(test.script) + " did not branch: " + vm.error());
        }
    }
}

uint32_t flag_instruction(const dc4::MesFile& file, uint8_t opcode, uint16_t flag) {
    dc4::MesStream stream(file);
    dc4::MesCommand c;
    while (stream.next(&c))
        if (c.op == opcode && c.words[0] == 100 && c.words[1] == flag) return c.pos;
    throw std::runtime_error("missing progression instruction");
}

void run_to_transfer(dc4::MesVm& vm) {
    dc4::MesCommand c;
    for (size_t step = 0; step < 100000 && vm.next(&c); ++step)
        if (vm.transferring()) return;
    throw std::runtime_error("ending did not transfer: " + vm.error());
}

void progression_branches(Scripts& scripts) {
    const char* endings[] = {"dc4_nin20190503b", "dc4_sor20190504a", "dc4_hiy20190430b",
        "dc4_sii20190507c", "dc4_miu20190331c", "dc4_tiy20190405d",
        "dc4_asa20200406a", "dc4_asu20190506b"};
    if(scripts.get("main").version==0xABB6)endings[6]="dc4_asa20190513c";
    for (int last = 0; last < 8; ++last) {
        const auto& ending = scripts.get(endings[last]);
        dc4::MesVm vm(ending);
        vm.seek_to_pc(flag_instruction(ending, 0x82, uint16_t(101 + last)));
        run_to_transfer(vm);
        for (int i = 0; i < 8; ++i)
            require(vm.read_variable(100, uint16_t(101+i)) == int(i==last),
                    "ending awarded an unrelated completion");
        require(vm.read_variable(100,112)==0, "one ending unlocked Arisu's map");
    }
    for (int mask = 0; mask < 16; ++mask) for (int last = 0; last < 4; ++last) {
        const auto& ending = scripts.get(endings[last]);
        dc4::MesVm vm(ending);
        for (int i = 0; i < 4; ++i) vm.write_variable(100,uint16_t(101+i),(mask>>i)&1);
        vm.seek_to_pc(flag_instruction(ending,0x82,uint16_t(101+last)));
        restore(vm,ending);
        run_to_transfer(vm);
        require(vm.read_variable(100,112)==int((mask|(1<<last))==15),
                "Arisu map ignored one of the four prerequisite endings");
        require(vm.bind(scripts.get(maps[0])),vm.error());
        dc4::MesCommand c;while(vm.next(&c)) {}
        require(vm.waiting_for_choice() && vm.choices().size()==((mask|(1<<last))==15?4u:3u),
                "earned map unlock did not reach the actual choices");
    }
    for (const char* name : {"dc4_mid20190420e", "dc4_sor20190501a"}) {
        const auto& file=scripts.get(name);
        for (bool cleared : {false,true}) {
            dc4::MesVm vm(file);
            if(cleared)vm.write_variable(100,107,1);
            vm.seek_to_pc(flag_instruction(file,0x86,107));
            dc4::MesCommand c;
            for(int steps=0;steps<10000 && vm.next(&c);++steps)
                if(c.op==0x54)break;
            const bool sorane=std::string(name)=="dc4_sor20190501a";
            require(vm.waiting_for_choice()==(cleared||sorane),"Arisu completion gate bypassed");
            require(vm.choices().size()==(cleared?2u:sorane?1u:0u),"wrong gated heroine choices");
            if(cleared){
                restore(vm,file);
                const std::string label=sorane?"label02":"arisu";
                const auto found=std::find_if(vm.choices().begin(),vm.choices().end(),[&](const auto& choice){return choice.label==label;});
                require(found!=vm.choices().end(),"missing unlocked heroine choice");
                require(vm.choose(size_t(found-vm.choices().begin())),vm.error());
            }
        }
    }
    const auto& ed=scripts.get("ed");
    for (int missing=-1;missing<8;++missing) {
        dc4::MesVm vm(ed);
        for(int i=0;i<8;++i)vm.write_variable(100,uint16_t(101+i),int(i!=missing));
        dc4::MesCommand c;
        for(int steps=0;steps<10000 && vm.next(&c);++steps)if(c.op==0x13)break;
        require(vm.error().empty() && vm.read_variable(100,100)==int(missing==-1),
                "D.C. unlocked without all eight ending flags");
    }
    std::cout<<"8 ending awards, 64 Arisu map gates, Arisu/Sorane choices and 9 D.C. gates passed\n";
}

void game_regressions(const char* variable) {
    const char* data = std::getenv(variable);
    if (!data) { std::cout << variable << " unset: installed-data routes skipped\n"; return; }
    Scripts scripts{std::filesystem::path(data) / "MES", {}};
    size_t paths = 0;
    for (bool unlocked : {false,true}) {
        const int options = unlocked ? 4 : 3;
        for (int a = 0; a < options; ++a)
        for (int b = 0; b < options; ++b)
        for (int c = 0; c < options; ++c)
        for (int d = 0; d < options; ++d)
        for (bool saved : {false,true}) {
            map_path(scripts, {a,b,c,d}, unlocked, saved);
            ++paths;
        }
    }
    heroine_branches(scripts);
    progression_branches(scripts);
    std::cout << variable << ": " << paths << " map paths and 12 heroine comparisons passed\n";
}
}

int main() {
    int failures = 0;
    const auto run = [&](const char* name, auto test) {
        try { test(); }
        catch (const std::exception& e) { std::cerr << name << ": " << e.what() << '\n'; ++failures; }
    };
    run("Service", service_regressions);
    run("Fresh progress", fresh_progress);
    run("DC4", [] { game_regressions("DC4_DATA"); });
    run("DC4PH", [] { game_regressions("DC4PH_DATA"); });
    std::cout << (failures ? "DC4 route regressions FAILED\n" : "DC4 route regressions passed\n");
    return failures ? 1 : 0;
}
