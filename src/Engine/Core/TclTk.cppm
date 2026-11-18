module;

#include <cstdlib>
#include <string_view>
extern "C" {
  #include <tcl.h>
  #include <tclDecls.h>
  #include <tk.h>
}

export module Engine.Core.TclTk;

#undef TCL_OK
#undef TCL_ERROR
#undef TCL_RETURN
#undef TCL_BREAK
#undef TCL_CONTINUE
#undef TCL_STATIC
#undef TCL_VOLATILE
#undef TCL_DYNAMIC
#undef TCL_EVAL_DIRECT
#undef TCL_GLOBAL_ONLY
#undef TCL_DONT_WAIT
#undef Tcl_Eval

export
{
  using ::Tcl_Interp;
  using ::Tcl_Obj;
  using ::Tk_Window;
  using ::ClientData;
  using ::Tcl_FreeProc;
  using ::Tcl_FindExecutable;
  using ::Tcl_CreateInterp;
  using ::Tk_Init;
  using ::Tcl_CreateObjCommand;
  using ::Tcl_GetString;
  using ::Tcl_NewStringObj;
  using ::Tcl_SetObjResult;
  using ::Tcl_EvalEx;
  using ::Tk_MainWindow;
  using ::Tcl_GetIntFromObj;
  using ::Tcl_NewListObj;
  using ::Tk_MainLoop;
  using ::Tcl_DeleteInterp;
  using ::Tcl_DStringResult;
  using ::Tcl_Finalize;
  using ::Tcl_GetStringFromObj;
  using ::Tcl_GetObjResult;
  using ::Tcl_EvalFile;
  using ::Tcl_ListObjAppendElement;
  using ::Tcl_EvalObjEx;
  using ::Tcl_IncrRefCount;
  using ::Tcl_DecrRefCount;
  using ::Tcl_SetVar2;
  using ::Tcl_DoOneEvent;
}

export {

  inline constexpr int TCL_OK = 0;
  inline constexpr int TCL_ERROR = 1;
  inline constexpr int TCL_RETURN = 2;
  inline constexpr int TCL_BREAK = 3;
  inline constexpr int TCL_CONTINUE = 4;
  inline constexpr int TCL_EVAL_DIRECT = 0x040000;
  inline constexpr int TCL_GLOBAL_ONLY = 1;
  inline constexpr int TCL_DONT_WAIT = (1<<1);

  inline Tcl_FreeProc* const TCL_STATIC = nullptr;
  inline Tcl_FreeProc* const TCL_VOLATILE = reinterpret_cast<Tcl_FreeProc*>(-1);
  inline Tcl_FreeProc* const TCL_DYNAMIC = reinterpret_cast<Tcl_FreeProc*>(3);
}

namespace Vyroda {
  export inline int Tcl_Eval(Tcl_Interp *interp, const char *script) {
    return Tcl_EvalEx(interp, script, script ? static_cast<int>(std::string_view(script).size()) : 0, 0);
  }

  // TODO: ugly workaround, tcl/tk9 is not yet in msys pacman repos...
  export inline int Tcl_Init(Tcl_Interp* interp) {
#ifdef _WIN32
    _putenv("TCL_LIBRARY=C:/msys64/ucrt64/lib/tcl9.0.3/library");
#endif

    static const char* candidate_paths[] = {
      "C:/msys64/ucrt64/lib/tcl9.0.3/library",
      "C:/msys64/ucrt64/lib/tcl9/library",
      "C:/msys64/ucrt64/lib/tcl9.0/library",
      "C:/msys64/ucrt64/lib/tcl9.0",
      "C:/msys64/ucrt64/share/tcl9.0"
    };

    for (const char* path : candidate_paths) {
      ::Tcl_SetVar2(interp, "tcl_library", nullptr, path, TCL_GLOBAL_ONLY);
      if (::Tcl_Init(interp) == TCL_OK) {
        return TCL_OK;
      }
    }

    return ::Tcl_Init(interp);
  }
}
