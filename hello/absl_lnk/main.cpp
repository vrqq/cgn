#include <iostream>
#include <string>
#include <vector>

#include <absl/strings/str_join.h>
#include <absl/random/random.h>
#include <absl/synchronization/mutex.h>

void rand1() {
    absl::BitGen bitgen;
  std::vector<int> objs = {10, 20, 30, 40, 50};
    size_t index = absl::Uniform(bitgen, 0u, objs.size());
    double fraction = absl::Uniform(bitgen, 0, 1.0);
    bool coin_flip = absl::Bernoulli(bitgen, 0.5);
}

int main() {
  std::vector<std::string> v = {"foo", "bar", "baz"};
  std::string s = absl::StrJoin(v, "-");

  std::cout << "Joined string: " << s << "\n";

  absl::Mutex mu;
  mu.Lock();
  mu.unlock();

  return 0;
}