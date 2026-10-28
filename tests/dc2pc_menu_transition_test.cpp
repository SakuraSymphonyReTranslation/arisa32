#include "arisa32/dc2pc_menu_transition.h"
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
    if (failures) std::cerr << failures << " DC2PC presentation checks failed\n";
    return failures ? 1 : 0;
}
