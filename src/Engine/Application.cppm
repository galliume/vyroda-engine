/**
 * @file   Application.cppm
 * @author  <Guillaume PERCEPIED>
 * @date   Mon Mar  9 21:21:07 2026
 * 
 * @brief  main entrance
 * 
 * 
 */
module;
#include "VyrodaConfig.h"

export module Engine.Application;

import std;

import Engine.Core.GLFW;
import Engine.Core.MiniAudio;

//import Engine.Core.CommandQueueLocator;
import Engine.Managers.APIManager;
import Engine.Managers.ConfigManagerLocator;
import Engine.Managers.InputManagerLocator;
import Engine.Managers.NetworkManager;
import Engine.Managers.RenderManager;
import Engine.Managers.UIManager;

import Engine.GUI.Window;

namespace Vyroda
{
  export class Application
  {
  public:
    /** 
     * init the application
     *
     * @param bool editor_mode 
     */
    void init(bool const editor_mode);

    /** 
     * Run the game
     *
     */
    void run() const;

    /** 
     * Start the server with the specified port
     *
     * @param string port 
     */
    void startServer(std::string const& port);

    /** 
     * Get the RenderManager
     *
     * @return RenderManager&
     */
    RenderManager& getRenderManager() const { return *_render_manager; }

  private:
    std::unique_ptr<APIManager> _api_manager;
    std::unique_ptr<NetworkManager> _network_manager;
    std::unique_ptr<RenderManager> _render_manager;
    std::unique_ptr<UIManager> _ui_manager;
    std::chrono::time_point<std::chrono::steady_clock> _start_run;
  };
}
