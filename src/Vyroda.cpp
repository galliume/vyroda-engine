import std;

import Engine.Application;

import Engine.Core.Logger;
import Engine.Core.TclTk;

import Editor.Managers.EditorManager;

int main(int argc, char** argv)
{
  //@todo config broken ?
  auto const Vyroda_VERSION_MAJOR { 0 };
  auto const Vyroda_VERSION_MINOR { 2 };

  std::cout << argv[0] << " Version " << Vyroda_VERSION_MAJOR << "." << Vyroda_VERSION_MINOR << std::endl;
  
  bool server_mode{ false };
  std::string port{ "9371" };
  
  bool editor_mode { false };
  
  for (int i { 0 }; i < argc; ++i) {
    std::string argument = argv[i];
    if ("--server" == argument || "-S" == argument) {
      server_mode = true;
    } else if (argument.find("--port") != std::string::npos) {
      std::size_t pos = argument.find("=");
      if (pos == std::string::npos) {
        Vyroda::Logger::warn("bad argument {}", argument);
      } else {
        port = argument.substr(++pos, argument.size());
      }
    }
    if("--editor" == argument || "-E" == argument) {
      editor_mode = true;
    }
  }

  auto app { std::make_unique<Vyroda::Application>() };
  app->init(editor_mode);

  Vyroda::Logger::trace("server_mode {}", server_mode);

  if (server_mode) {
    Vyroda::Logger::trace("port {}", port);
    app->startServer(port);
  }

  Vyroda::Logger::trace("editor_mode : {}", editor_mode);

  if (editor_mode) {
    Tcl_FindExecutable(argv[0]);

    auto _editor_manager = std::make_unique<Vyroda::EditorManager>(app.get());
  } else {
    app->run();
  }

  return 0;
}
