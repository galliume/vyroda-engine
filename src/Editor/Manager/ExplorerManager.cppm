export module Editor.Managers.ExplorerManager;

import Engine.Core.TclTk;

import Engine.Managers.RenderManager;

namespace Vyroda {

  export class ExplorerManager
  {
    public:
      ExplorerManager() = default;
      ~ExplorerManager() = default;

      void registerCommand(
        Tcl_Interp * tcl_interp,
        RenderManager & render_manager);
  };
}
