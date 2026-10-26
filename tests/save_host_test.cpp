// Include the desktop host without entering its event loop. These checks cover
// its parser, metadata cache and save-directory guard using owned fixtures.
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#define main dc4_desktop_entry_for_test
#include "../sdl_desktop/main.cpp"
#undef main

#include <iostream>

namespace {
int failures = 0;
void check(bool value, const char* expression, int line) {
    if (!value) { std::cerr << line << ": " << expression << '\n'; ++failures; }
}
#define CHECK(value) check(bool(value), #value, __LINE__)

struct TemporaryDirectory {
    fs::path parent, path;
    TemporaryDirectory() {
        parent = fs::canonical(fs::temp_directory_path());
        path = parent / ("dc4engine-save-host-" + std::to_string(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        if (!fs::create_directory(path)) throw std::runtime_error("cannot create unique test directory");
    }
    ~TemporaryDirectory() {
        std::error_code ec;
        const auto resolved = fs::weakly_canonical(path, ec);
        // Only remove the exact directory created by this fixture.
        if (!ec && resolved == path && resolved.parent_path() == parent &&
            resolved.filename().string().find("dc4engine-save-host-") == 0)
            fs::remove_all(resolved, ec);
    }
};

void write_fixture(const fs::path& path, const std::string& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), std::streamsize(bytes.size()));
    if (!out) throw std::runtime_error("cannot write test fixture");
}

bool parse_fixture(std::initializer_list<const char*> arguments, Options* options, std::string* error) {
    std::vector<std::string> storage{"dc4_desktop"};
    for (const char* argument : arguments) storage.emplace_back(argument);
    std::vector<char*> argv;
    for (auto& argument : storage) argv.push_back(argument.data());
    return parse_options(int(argv.size()), argv.data(), options, error);
}

void page_options() {
    Options valid;
    std::string error;
    CHECK(parse_fixture({"--shot", "capture.bmp", "--panel", "save", "--save-page", "10"}, &valid, &error));
    CHECK(valid.save_page == 10 && valid.panel == "save");
    valid = Options{};
    CHECK(parse_fixture({"--save-page", "1", "--panel", "load", "--shot", "capture.bmp"}, &valid, &error));
    CHECK(valid.save_page == 1 && valid.panel == "load");
    valid = Options{};
    CHECK(parse_fixture({"--panel", "save", "--shot", "capture.bmp"}, &valid, &error));
    CHECK(valid.save_page == 0);  // Unspecified means the screen's first page.

    for (const char* page : {"0", "11", "-1", "one", "1x"}) {
        Options invalid;
        error.clear();
        CHECK(!parse_fixture({"--panel", "save", "--shot", "capture.bmp", "--save-page", page}, &invalid, &error));
        CHECK(error.find("invalid number") != std::string::npos);
    }
    Options invalid;
    CHECK(!parse_fixture({"--save-page"}, &invalid, &error));
    CHECK(error.find("missing value") != std::string::npos);
    invalid = Options{};
    CHECK(!parse_fixture({"--save-page", "2"}, &invalid, &error));
    CHECK(error == "--save-page requires --panel save or --panel load");
    invalid = Options{};
    CHECK(!parse_fixture({"--shot", "capture.bmp", "--panel", "backlog", "--save-page", "2"}, &invalid, &error));
    CHECK(error == "--save-page requires --panel save or --panel load");
    invalid = Options{};
    CHECK(!parse_fixture({"--panel", "save", "--save-page", "2"}, &invalid, &error));
    CHECK(error.find("--panel requires --shot") != std::string::npos);
}

void protected_directories(const fs::path& root) {
    const fs::path game = root / "game", data = game / "AdvData";
    fs::create_directories(data);
    CHECK(is_original_save_directory(game / "savedata", data.string()));
    CHECK(is_original_save_directory(game / "savedata" / "nested", data.string()));
    CHECK(is_original_save_directory(data / "savedata", data.string()));
    CHECK(is_original_save_directory(data / "savedata" / "nested", data.string()));
    CHECK(is_original_save_directory(data / ".." / "savedata" / "nested", data.string()));
    CHECK(!is_original_save_directory(game / "savedata-engine", data.string()));
    CHECK(!is_original_save_directory(data / "savedata-engine" / "nested", data.string()));
    CHECK(!is_original_save_directory(root / "engine-saves", data.string()));
    CHECK(!engine_save_directory_allowed(data / "savedata", data.string()));
    CHECK(engine_save_directory_allowed(root / "engine-saves", data.string()));
#ifdef _WIN32
    CHECK(is_original_save_directory(game / "SaVeDaTa" / "nested", data.string()));
#endif
}

void dc2pc_save_directory(const fs::path& root) {
    const fs::path data = root / "dc2pc" / "AdvData";
    fs::create_directories(data / "MES");
    // No labels, version 0x9969, one scope marker and the end opcode.
    const char bytes[] = {0,0,0,0,0x69,char(0x99),2,0,0,0};
    write_fixture(data / "MES/main.mes", std::string(bytes, sizeof(bytes)));
    const char* override_dir = std::getenv("DC4_SAVE_DIR");
    if (override_dir && *override_dir) {
        CHECK(save_directory(data.string()) == fs::absolute(override_dir));
        return;
    }
    const auto saves = save_directory(data.string());
    CHECK(saves == data / "SaveData");
    CHECK(fs::is_directory(saves));
    CHECK(engine_save_directory_allowed(saves, data.string()));
    CHECK(!fs::exists(data / "arisa32-saves"));
    fs::rename(saves, data / "temporary-saves");
    fs::rename(data / "temporary-saves", data / "sAvEdAtA");
    const auto existing = save_directory(data.string());
    CHECK(existing == data / "sAvEdAtA");
    CHECK(engine_save_directory_allowed(existing, data.string()));
    write_fixture(existing / "DC2PC01.dat", "original PC save");
    write_fixture(existing / "slot1.dc4sav", "engine snapshot");
    CHECK(fs::file_size(existing / "DC2PC01.dat") == 16);
    fs::rename(existing, data.parent_path() / "Savedata");
    CHECK(save_directory(data.string()) == data.parent_path() / "Savedata");
    CHECK(!fs::exists(data / "arisa32-saves"));
}

void write_thumbnail(const fs::path& save, int width = 128, int height = 72) {
    Surface surface(SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32), SDL_FreeSurface);
    if (!surface || SDL_FillRect(surface.get(), nullptr, SDL_MapRGBA(surface->format, 31, 63, 127, 255)) < 0 ||
        SDL_SaveBMP(surface.get(), (save.string() + ".preview.bmp").c_str()) < 0)
        throw std::runtime_error("cannot write thumbnail fixture");
}

void slot_metadata(const fs::path& root) {
    const fs::path directory = root / "engine-saves";
    const auto baseline = fs::file_time_type::clock::now() - std::chrono::hours(1);
    for (int slot = 1; slot <= 100; ++slot) {
        const fs::path save = directory / ("slot" + std::to_string(slot) + ".dc4sav");
        write_fixture(save, "synthetic engine snapshot " + std::to_string(slot));
        fs::last_write_time(save, baseline);
    }
    fs::last_write_time(directory / "slot41.dc4sav", baseline + std::chrono::minutes(1));
    fs::last_write_time(directory / "slot100.dc4sav", baseline + std::chrono::minutes(2));
    const fs::path quick = directory / "quicksave.dc4sav";
    write_fixture(quick, "synthetic quick snapshot");
    fs::last_write_time(quick, baseline + std::chrono::minutes(3));

    const fs::path valid = directory / "slot1.dc4sav";
    write_fixture(valid.string() + ".preview.txt", "DC4PREVIEW1 " + save_fingerprint(valid) + "\nSpeaker: preview text");
    write_thumbnail(valid);
    write_fixture(valid.string()+".icon","DC4ICON1 "+save_fingerprint(valid)+"\n7");
    const fs::path stale = directory / "slot2.dc4sav";
    write_fixture(stale.string() + ".preview.txt", "DC4PREVIEW1 stale-fingerprint\nOutdated preview");
    write_thumbnail(stale);
    write_fixture(stale.string()+".icon","DC4ICON1 stale-fingerprint\n5");
    const fs::path bad_image = directory / "slot3.dc4sav";
    write_fixture(bad_image.string() + ".preview.txt", "DC4PREVIEW1 " + save_fingerprint(bad_image) + "\nText survives an invalid image");
    write_thumbnail(bad_image, 16, 9);
    const fs::path large_text = directory / "slot4.dc4sav";
    write_fixture(large_text.string() + ".preview.txt", std::string(4097, 'X'));
    write_thumbnail(large_text);
    const fs::path bad_utf8 = directory / "slot5.dc4sav";
    write_fixture(bad_utf8.string() + ".preview.txt", "DC4PREVIEW1 " + save_fingerprint(bad_utf8) + "\n\xC0\xAF");

    dc4ui::SaveScreen screen;
    SaveSlots slots;
    slots.refresh(directory, &screen,0xAAB6);
    CHECK(slots.entries.size() == 100);
    CHECK(std::all_of(slots.entries.begin(), slots.entries.end(), [](const dc4ui::SaveSlot& slot) { return slot.occupied; }));
    CHECK(slots.latest_slot == 99);  // A newer quicksave does not replace NEW.
    CHECK(slots.entries[0].character_icon==7);
    CHECK(slots.entries[1].character_icon==-1);
    write_fixture(valid.string()+".icon","DC4ICON1 "+save_fingerprint(valid)+"\n16");
    CHECK(SaveSlots::read(valid).character_icon==-1);
    CHECK(!slots.entries[99].timestamp.empty());
    CHECK(slots.entries[0].excerpt == u"Speaker: preview text");
    CHECK(slots.entries[0].thumbnail.w == 128 && slots.entries[0].thumbnail.h == 72);
    CHECK(slots.entries[0].thumbnail.rgba.size() == 128 * 72 * 4);
    if (slots.entries[0].thumbnail.rgba.size() >= 4) {
        const auto& pixels = slots.entries[0].thumbnail.rgba;
        CHECK(pixels[0] == 31 && pixels[1] == 63 && pixels[2] == 127 && pixels[3] == 255);
    }
    CHECK(slots.entries[1].excerpt.empty() && slots.entries[1].thumbnail.rgba.empty());
    CHECK(slots.entries[2].excerpt == u"Text survives an invalid image");
    CHECK(slots.entries[2].thumbnail.rgba.empty());
    CHECK(slots.entries[3].excerpt.empty() && slots.entries[3].thumbnail.rgba.empty());
    CHECK(slots.entries[4].excerpt.empty());

    screen.set_mode(dc4ui::SaveMode::Load);
    screen.set_page(9);
    CHECK(screen.hit_test(620, 570).type == dc4ui::SaveActionType::Slot);
    CHECK(screen.hit_test(620, 570).index == 99);
    CHECK(screen.hit_test(500, 40).type == dc4ui::SaveActionType::LatestLoad);
    CHECK(screen.hit_test(650, 40).type == dc4ui::SaveActionType::QuickLoad);
    CHECK(screen.hit_test(370, 40).type == dc4ui::SaveActionType::None);

    // Removed snapshots must clear both cached previews and clickable rows.
    fs::remove(valid);
    fs::remove(directory / "slot100.dc4sav");
    slots.refresh(directory, &screen,0xAAB6);
    CHECK(slots.latest_slot == 40);
    CHECK(!slots.entries[0].occupied && slots.entries[0].excerpt.empty() && slots.entries[0].thumbnail.rgba.empty());
    CHECK(!slots.entries[99].occupied);
    CHECK(screen.hit_test(620, 570).type == dc4ui::SaveActionType::None);

    slots.refresh(root / "empty-engine-saves", &screen,0xAAB6);
    CHECK(slots.latest_slot == -1);
    CHECK(std::none_of(slots.entries.begin(), slots.entries.end(), [](const dc4ui::SaveSlot& slot) { return slot.occupied; }));
    CHECK(screen.hit_test(500, 40).type == dc4ui::SaveActionType::None);
    CHECK(screen.hit_test(650, 40).type == dc4ui::SaveActionType::None);
}
void native_filenames(const fs::path& root) {
    const auto directory=root/"native-files";
    fs::create_directories(directory);
    CHECK(dc4::save_target(directory,0x9969,0).filename()=="DC2PC00.dat");
    CHECK(dc4::save_target(directory,0x9969,1).filename()=="DC2PC02.dat");
    CHECK(dc4::save_target(directory,0x9969,4).filename()=="DC2PC08.dat");
    CHECK(dc4::save_target(directory,0x9969,24).filename()=="DC2PC40.dat");
    CHECK(dc4::save_target(directory,0x9969,48).filename()=="DC2PC76.dat");
    CHECK(dc4::save_slot_number("DC2PC01.dat",0x9969)==-1);
    CHECK(dc4::save_slot_number("DC2PC05.dat",0x9969)==-1);
    for(int logical=0;logical<=100;++logical)
        CHECK(dc4::save_slot_number(dc4::save_target(directory,0x9969,logical).filename().string(),0x9969)==logical);
    CHECK(dc4::save_target(directory,0xAAB6,1).filename()=="DC403.dat");
    CHECK(dc4::save_target(directory,0xABB6,100).filename()=="DC4PHDL102.dat");
    CHECK(dc4::save_slot_number("DC400.dat",0xAAB6)==-1);
    CHECK(dc4::save_slot_number("dc402.DAT",0xAAB6)==0);
    CHECK(dc4::save_slot_number("DC4103.dat",0xAAB6)==-1);
    CHECK(dc4::save_slot_number("DC403.dat.arisa",0xAAB6)==-1);
    const auto pc=directory/"dc403.DAT", legacy=directory/"slot1.dc4sav";
    write_fixture(pc,"native");write_fixture(legacy,"legacy");
    const auto baseline=fs::file_time_type::clock::now();
    fs::last_write_time(pc,baseline);fs::last_write_time(legacy,baseline+std::chrono::hours(2));
    CHECK(dc4::save_target(directory,0xAAB6,1)==pc);
    CHECK(dc4::resolve_save(directory,0xAAB6,1)==pc);
    write_fixture(directory/"DC402.dat","quick");
    write_fixture(directory/"DC400.dat","reserved");
    CHECK(dc4::latest_save(directory,0xAAB6)==pc);
    write_fixture(directory/"slot2.dc4sav","second");
    fs::last_write_time(directory/"slot2.dc4sav",baseline+std::chrono::hours(1));
    CHECK(dc4::latest_save(directory,0xAAB6)==directory/"slot2.dc4sav");
    const auto classic=directory/"classic";fs::create_directories(classic);
    const auto copy=classic/"dc2pc01.DAT", numbered=classic/"DC2PC02.dat";
    write_fixture(copy,"latest copy");
    CHECK(dc4::latest_save(classic,0x9969)==copy);
    write_fixture(numbered,"regular");
    fs::last_write_time(copy,baseline);fs::last_write_time(numbered,baseline-std::chrono::hours(1));
    CHECK(dc4::latest_save(classic,0x9969)==copy);
    fs::last_write_time(numbered,baseline+std::chrono::hours(1));
    CHECK(dc4::latest_save(classic,0x9969)==numbered);
    write_fixture(classic/"DC2PC00.dat","quick");
    fs::last_write_time(classic/"DC2PC00.dat",baseline+std::chrono::hours(2));
    CHECK(dc4::latest_save(classic,0x9969)==numbered);
}
}  // namespace

int main() {
    try {
        TemporaryDirectory temporary;
        page_options();
        protected_directories(temporary.path);
        dc2pc_save_directory(temporary.path);
        slot_metadata(temporary.path);
        native_filenames(temporary.path);
    } catch (const std::exception& e) {
        std::cerr << "save_host_test: " << e.what() << '\n';
        return 1;
    }
    if (failures) std::cerr << failures << " save host checks failed\n";
    return failures ? 1 : 0;
}
