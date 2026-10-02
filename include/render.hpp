#include <stop_token>

#include "config.hpp"
#include "snapshot.hpp"
#include "data.hpp"

void render(std::stop_token st, Snapshot<std::vector<DisplayData>> &snap, Snapshot<int> &msg_rate, const Config &cfg);