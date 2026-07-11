namespace eval AppMenu {
  menu .menubar
  . configure -menu .menubar

  set m .menubar
  menu $m.file
  menu $m.edit
  $m add cascade -menu $m.file -label File
  $m add cascade -menu $m.edit -label Edit

  $m.file add command -label "Close" -command "AppMenu::close_VYR_edit"

  proc close_VYR_edit {} {
    exit 0
  }
}