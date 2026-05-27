#include "core/deferred_call.h"
#include "core/log.h"
#include "greeter/greeter.h"
#include "wayland/wayland_client.h"

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <grp.h>
#include <sstream>
#include <unistd.h>
#include <vector>

namespace {
  constexpr Logger kLog("main");

  std::atomic<bool> g_shutdownRequested{false};

  std::string formatGroupList() {
    int count = getgroups(0, nullptr);
    if (count <= 0) {
      return {};
    }
    std::vector<gid_t> groups(static_cast<size_t>(count));
    if (getgroups(count, groups.data()) < 0) {
      return {};
    }
    std::ostringstream oss;
    for (int i = 0; i < count; ++i) {
      if (i > 0) {
        oss << ',';
      }
      if (struct group* gr = getgrgid(groups[static_cast<size_t>(i)]); gr && gr->gr_name) {
        oss << gr->gr_name;
      } else {
        oss << groups[static_cast<size_t>(i)];
      }
    }
    return oss.str();
  }

  void logStartupEnvironment() {
    kLog.info("uid={} euid={} gid={} groups=[{}]", getuid(), geteuid(), getgid(), formatGroupList());
    kLog.info("WAYLAND_DISPLAY={} XDG_SESSION_TYPE={} XDG_RUNTIME_DIR={}",
              std::getenv("WAYLAND_DISPLAY") ? std::getenv("WAYLAND_DISPLAY") : "unset",
              std::getenv("XDG_SESSION_TYPE") ? std::getenv("XDG_SESSION_TYPE") : "unset",
              std::getenv("XDG_RUNTIME_DIR") ? std::getenv("XDG_RUNTIME_DIR") : "unset");
  }

  void preventGreetdRespawnLoop() {
    kLog.error("holding process so greetd does not respawn — fix config and restart greetd");
    while (!g_shutdownRequested.load()) {
      sleep(60);
    }
  }

  void signalHandler(int signum) {
    if (signum == SIGTERM || signum == SIGINT) {
      g_shutdownRequested = true;
    }
  }
}

int main(int argc, char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0) {
      std::printf("noctalia-greeter %s\n", NOCTALIA_GREETER_VERSION);
      return 0;
    }
    if (std::strcmp(argv[i], "--log-test") == 0) {
      emergencyLogBootstrap(argc, argv);
      initLogging();
      kLog.info("log-test ok; open files: {}", loggingPaths()[0] != '\0' ? loggingPaths() : "(none)");
      kLog.warn("log-test warn line");
      kLog.error("log-test error line");
      std::printf("Check: /var/log/noctalia-greeter.log /var/lib/noctalia-greeter/greeter.log "
                  "/tmp/noctalia-greeter.log\n");
      return loggingPaths()[0] != '\0' ? 0 : 1;
    }
    if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
      std::puts("Usage: noctalia-greeter [OPTIONS]\n"
                "\n"
                "Run as a Wayland client under a compositor (e.g. cage).\n"
                "\n"
                "Options:\n"
                "  -h, --help       Show this help message\n"
                "  -v, --version    Show version information\n"
                "  --log-test       Write test lines to all log paths and exit\n"
                "\n"
                "Environment:\n"
                "  GREETD_SOCK           Path to greetd Unix socket\n"
                "  WAYLAND_DISPLAY       Wayland display (set by compositor)\n"
                "  NOCTALIA_GREETER_LOG  Log file path (overrides defaults)\n"
                "\n"
                "Greetd example:\n"
                "  command = \"dbus-run-session cage -s -mlast -- /usr/local/bin/noctalia-greeter\"\n"
                "  user = \"greeter\"\n"
                "\n"
                "For more information, visit https://noctalia.dev");
      return 0;
    }
  }

  emergencyLogBootstrap(argc, argv);
  initLogging();

  std::signal(SIGTERM, signalHandler);
  std::signal(SIGINT, signalHandler);

  kLog.info("noctalia-greeter {}", NOCTALIA_GREETER_VERSION);

  const bool greetdLaunched = std::getenv("GREETD_SOCK") != nullptr;
  if (greetdLaunched) {
    const char* paths = loggingPaths();
    if (paths[0] != '\0') {
      kLog.info("greetd session: file logs at {}", paths);
    } else {
      kLog.warn("greetd session: no log file writable — run: just setup-log-dir");
    }
    logStartupEnvironment();
  }

  if (std::getenv("WAYLAND_DISPLAY") == nullptr) {
    kLog.error("WAYLAND_DISPLAY is not set — run under a Wayland compositor (e.g. cage)");
    if (greetdLaunched) {
      preventGreetdRespawnLoop();
    }
    return 1;
  }

  WaylandClient client;
  if (!client.connect()) {
    kLog.error("failed to connect to Wayland compositor");
    if (greetdLaunched) {
      preventGreetdRespawnLoop();
    }
    return 1;
  }

  Greeter greeter;
  if (!greeter.initialize(client)) {
    kLog.error("failed to initialize greeter");
    if (greetdLaunched) {
      preventGreetdRespawnLoop();
    }
    return 1;
  }

  int result = 0;
  try {
    result = greeter.run(client);
  } catch (const std::exception& e) {
    kLog.error("fatal error in event loop: {}", e.what());
    result = 1;
  }

  kLog.info("shutdown complete");
  return result;
}
