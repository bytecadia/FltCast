#include <stop_token>

#include "config.hpp"
#include "snapshot.hpp"
#include "data.hpp"

void render(std::stop_token st, Snapshot<std::vector<DisplayData>> &snap, const Config &cfg);