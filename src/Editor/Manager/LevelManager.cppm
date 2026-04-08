export module Editor.Managers.LevelManager;

import Engine.Core.TclTk;

import Engine.Managers.RenderManager;

namespace Vyroda {

  export class LevelManager
  {
    public:
      LevelManager() = default;
      ~LevelManager() = default;

      void registerCommand(
        Tcl_Interp * tcl_interp,
        RenderManager & render_manager);
  };
}
