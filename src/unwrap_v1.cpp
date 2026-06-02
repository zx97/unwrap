#include "unwrap_v1.h"
#include "unwrap_v1_state.h"

std::string unwrap_v1(const std::string& source) {
  V1State state;
  state.runnable_f = true;
  state.always_space_f = false;
  state.line_gap_limit2 = 1000000;
  state.line_soft_limit2 = 0;
  state.quote_limit = 0;
  state.line_endings = 0;
  return state.unwrap(source);
}
