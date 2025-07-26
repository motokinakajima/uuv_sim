#include "constants.h"

class EventController {
public:
    EventController();
    void proceed_step();
    void run();
    void stop();
    double get_t();
    void update_state();
private:
    bool running = true;
    double cur_t = 0;
    const double delta_t = DELTA_T;
};