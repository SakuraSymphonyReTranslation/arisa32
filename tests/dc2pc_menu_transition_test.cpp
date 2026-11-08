#include "arisa32/dc2pc_menu_transition.h"
#include <algorithm>
#include <iostream>

int main() {
    int failures = 0;
    const auto check = [&](bool ok) { if (!ok) ++failures; };
    std::vector<uint8_t> frame;
    dc4ui::Dc2pcMenuTransition transition;
    check(!transition.active() && !transition.draw(&frame));
    for (int speed = 0; speed <= 3; ++speed) {
        const int duration = 640 / (1 << speed);
        transition.start({160,80,32,255}, speed, 1200);
        check(!transition.update(1200));
        transition.draw(&frame); check(frame[0] == 160);
        check(!transition.update(duration / 2));
        transition.draw(&frame); check(frame[0] == 80);
        // No update while the app is paused: repeated renders stay identical.
        const auto paused = frame;
        for (int i = 0; i < 10; ++i) { transition.draw(&frame); check(frame == paused); }
        check(transition.update(duration / 2));
        transition.draw(&frame); check(frame[0] == 0 && frame[3] == 255);
        check(!transition.update(1000)); // Replacement fires once; black persists during loading.
        transition.draw(&frame); check(frame[0] == 0);
        transition.reveal({32,160,80,255});
        transition.draw(&frame); check(frame[1] == 0);
        check(!transition.update(duration / 2));
        transition.draw(&frame); check(frame[1] == 80);
        check(!transition.update(duration / 2) && !transition.active());
    }
    transition.start({128,64,32,255}, 1);
    check(transition.update(2000)); // Slow frame must still present the black boundary.
    transition.draw(&frame); check(frame[0] == 0);
    transition.finish(); check(!transition.active());
    transition.start({0,0,0,255},1,600,false);
    check(!transition.update(599));
    check(transition.update(1)); // SC_END's authored wait does not add a second fade-out.
    transition.reveal({160,80,32,255});
    check(!transition.update(160)); transition.draw(&frame); check(frame[0] == 80);
    transition.finish();
    std::vector<uint8_t> scene(1280u*720u*4u,255);
    transition.start_dc2dm_title(scene,{});
    check(!transition.update(200)); transition.draw(&frame); check(frame==scene);
    check(!transition.update(40)); transition.draw(&frame);
    const auto visible = [](const std::vector<uint8_t>& pixels) {
        size_t count=0;
        for(size_t i=0;i<pixels.size();i+=4) if(pixels[i]) ++count;
        return count;
    };
    const auto first=visible(frame);
    check(first>0 && first<1280u*720u);
    const auto paused=frame;
    transition.draw(&frame); check(frame==paused);
    check(!transition.update(320)); transition.draw(&frame); check(visible(frame)<first);
    check(transition.update(640)); transition.draw(&frame); check(visible(frame)==0);
    check(!transition.update(10000));
    transition.reveal(scene); transition.draw(&frame); check(visible(frame)==0);
    check(!transition.update(320) && !transition.active());
    // Reusing the object for another profile must reset the particle mode.
    transition.start(scene,1); transition.update(160); transition.draw(&frame);
    check(frame[0]==127 && frame[4000]==127);
    transition.finish();
    if (failures) std::cerr << failures << " DC2PC presentation checks failed\n";
    return failures ? 1 : 0;
}
