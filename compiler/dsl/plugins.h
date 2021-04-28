#pragma once

#include "dsl.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace seq {

struct Plugin {
  std::unique_ptr<DSL> dsl;
  std::string path;
  void *handle;
};

class PluginManager {
private:
  ir::transform::PassManager *pm;
  std::vector<Plugin> plugins;

public:
  using LoadFunc = std::function<std::unique_ptr<DSL>()>;

  enum Error { NONE = 0, NOT_FOUND, NO_ENTRYPOINT, UNSUPPORTED_VERSION };

  explicit PluginManager(ir::transform::PassManager *pm) : pm(pm), plugins() {}
  ~PluginManager();
  Error load(const std::string &path);
};

} // namespace seq
