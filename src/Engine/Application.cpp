/**
 * @file   Application.cpp
 * @author  <Guillaume PERCEPIED>
 * @date   Mon Mar  9 21:20:32 2026
 * 
 * @brief  main application entrance
 * 
 * 
 */
module Engine.Application;

import std;

import Engine.Core.GLFW;
import Engine.Core.GLM;
import Engine.Core.Logger;
import Engine.Core.TclTk;

import Engine.Managers.DbManager;
import Engine.Managers.FontManager;
import Engine.Managers.UIManager;

import Engine.Managers.ConfigManagerLocator;
import Engine.Managers.InputManagerLocator;

namespace Vyroda
{
  void Application::init(bool const editor_mode)
  {
    _start_run = std::chrono::steady_clock::now();
    
    //CommandQueueManagerLocator::init();
    //@todo ugly fix
    auto const root_path { std::filesystem::absolute(__FILE__).parent_path().parent_path().parent_path().string() };

    ConfigManagerLocator::init(root_path);
    auto const& appConfig { ConfigManagerLocator::get()->appConfig()["resolution"] };

    auto window { std::make_unique<Window>() };
    window->init("Vyroda",
      root_path + "/vampyropoda.jpg",
      appConfig["width"].get<std::uint16_t>(),
      appConfig["height"].get<std::uint16_t>(),
      editor_mode);

    _render_manager = std::make_unique<RenderManager>();
    _render_manager->init(std::move(window));

    _api_manager = std::make_unique<APIManager>();
    _network_manager = std::make_unique<NetworkManager>(_api_manager.get());

    //auto db_manager = std::make_unique<DbManager>();
    if (!editor_mode) {
      _render_manager->getWindow().show();
    }

    // TODO: needs more thoughts on tcl/tk menu
    //_ui_manager = std::make_unique<UIManager>(_render_manager->getWindow());
    //_ui_manager->init(root_path);
  }

  void Application::run() const
  {
    using namespace std::chrono;

    auto const Vyroda_VERSION_MAJOR { 0 };
    auto const Vyroda_VERSION_MINOR { 2 };
    
    //duration<double> const title_rate{1.0};
    //duration<double> title_update{ title_rate};

    auto current_time{ steady_clock::now() };

    double accumulator{ 0.0 };
    //double total_time{ 0.0 };
    double const dt{ 0.01 };

    double ms_count{0.0};
    double fps_count{0.0};
    double frame_count{ 0.0 };
    auto last_time_debug_updated{ steady_clock::now() };

    FontManager::Text frame_counter {
      .id = 0,
      .name = "_VYR_frame_counter",
      .text = "Frame 00000000",
      .position = glm::vec3(10.0f, 20.0f, 0.0f),
      .color = glm::vec3(1.0f, 1.0f, 1.0f),
    };
    _render_manager->addText(frame_counter);
    FontManager::Text elapsed_time {
      .id = 0,
      .name = "_VYR_elapsed_time",
      .text = "Elapsed time 000000.00",
      .position = glm::vec3(200.0f, 20.0f, 0.0f),
      .color = glm::vec3(1.0f, 1.0f, 1.0f),
    };
    _render_manager->addText(elapsed_time);
    FontManager::Text ms_counter{
      .id = 0,
      .name = "_VYR_ms_counter",
      .text = "000.000 ms 000.000 fps",
      .position = glm::vec3(10.0f, 40.0f, 0.0f),
      .color = glm::vec3(1.0f, 1.0f, 1.0f),
    };
    _render_manager->addText(ms_counter);
    FontManager::Text camera_pos {
      .id = 0,
      .name = "_VYR_camera_pos",
      .text = "Position x: 000.000 y: 000.000 z: 000.000",
      .position = glm::vec3(10.0f, 60.0f, 0.0f),
      .color = glm::vec3(1.0f, 1.0f, 1.0f),
    };
    _render_manager->addText(camera_pos);

    bool release_build { true };

  #ifdef VYR_DEBUG_BUILD
    release_build = false;
  #endif

    auto* const config_manager { ConfigManagerLocator::get() };
    config_manager->setShowDebugHud(!release_build);

    bool last_show_debug_hud { config_manager->showDebugHud() };
    auto const apply_debug_hud_visibility = [&](bool const visible) {
      _render_manager->setTextVisible(frame_counter.id, visible);
      _render_manager->setTextVisible(elapsed_time.id, visible);
      _render_manager->setTextVisible(ms_counter.id, visible);
      _render_manager->setTextVisible(camera_pos.id, visible);
    };
    apply_debug_hud_visibility(last_show_debug_hud);

    std::string const title { std::format("Vyroda v{}.{} Vulkan version: {} {}",
      Vyroda_VERSION_MAJOR, Vyroda_VERSION_MINOR,
      _render_manager->getRenderer().getAPI().getAPIVersion(),
      (release_build ? "Release build" : "Debug build")) };

    glfwSetWindowTitle(_render_manager->getWindow().getGlfwWindow(), title.c_str());

    double const loaded_time { duration<double>(steady_clock::now() - _start_run).count() };
    Logger::trace("Started in {} seconds", loaded_time);

    constexpr double debug_hud_interval { 0.25 };
    glm::vec3 last_camera_pos { std::numeric_limits<float>::max() };
    bool hud_needs_refresh { last_show_debug_hud };
    std::uint64_t total_frames { 0 };

    while (!glfwWindowShouldClose(_render_manager->getWindow().getGlfwWindow())) {

      //while (Tcl_DoOneEvent(TCL_DONT_WAIT)) {}

      glfwPollEvents();

      auto const new_time{ steady_clock::now() };
      double frame_time{ duration<double>((new_time - current_time)).count() };

      if (frame_time > 0.25) {
        frame_time = 0.25;
      }
      accumulator += frame_time;

      //cf https://gafferongames.com/post/fix_your_timestep/
      while (accumulator >= dt) {
        _render_manager->getCamera().savePreviousState();
        _render_manager->getCamera().updateDeltaTime(dt);
        _render_manager->getCamera().move();

        accumulator -= dt;
        //total_time += dt;
      }

      double const alpha { accumulator / dt };
      _render_manager->getCamera().interpolate(alpha);

      bool const show_debug_hud { config_manager->showDebugHud() };
      if (show_debug_hud != last_show_debug_hud) {
        apply_debug_hud_visibility(show_debug_hud);
        last_show_debug_hud = show_debug_hud;
        if (show_debug_hud) {
          hud_needs_refresh = true;
        }
      }

      //Locator::getCommandQueue()->execPreRequest();
      _render_manager->renderScene(frame_time);
      //Locator::getCommandQueue()->execPostRequest();

      auto const _elapsed_time{ duration<double>(new_time - _start_run).count() };
      _render_manager->setElapsedTime(_elapsed_time);

      ++frame_count;
      ++total_frames;

      double const debug_dt { duration<double>(new_time - last_time_debug_updated).count() };
      bool fps_tick { false };
      if (debug_dt >= debug_hud_interval) {
        fps_count = frame_count / debug_dt;
        ms_count = (debug_dt * 1000.0) / std::max(1.0, frame_count);
        last_time_debug_updated = new_time;
        frame_count = 0.0;
        fps_tick = true;
      }

      if (show_debug_hud) {
        if (fps_tick || hud_needs_refresh) {
          _render_manager->updateText(ms_counter.id, std::format("{:<.2f}ms {:<.2f}fps", ms_count, fps_count));
          _render_manager->updateText(frame_counter.id, std::format("Frame {:<}", total_frames));
          _render_manager->updateText(elapsed_time.id, std::format("Elapsed time {:<.2f}", _elapsed_time));
        }

        auto const& current_camera_pos { _render_manager->getCamera().getPos() };
        bool const camera_moved {
          std::abs(current_camera_pos.x - last_camera_pos.x) > 0.005f
          || std::abs(current_camera_pos.y - last_camera_pos.y) > 0.005f
          || std::abs(current_camera_pos.z - last_camera_pos.z) > 0.005f
        };
        if (camera_moved || hud_needs_refresh) {
          _render_manager->updateText(
            camera_pos.id,
            std::format("Position x: {:<.2f} y: {:<.2f} z: {:<.2f}",
              current_camera_pos.x, current_camera_pos.y, current_camera_pos.z));
          last_camera_pos = current_camera_pos;
        }

        hud_needs_refresh = false;
      }

      current_time = new_time;

      //if (ConfigManagerLocator::get()->reload()) {
      //   _render_manager->cleanUp();
      //   _render_manager->init();
      //   ConfigManagerLocator::get()->setReload(false);
      // }
    }
    _render_manager->cleanUp();
  }

  void Application::startServer(std::string const& port)
  {
    _network_manager->startServer(port);
  }
}
