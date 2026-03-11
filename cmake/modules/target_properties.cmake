function(set_vyroda_target_properties target)

  #set_target_properties(${target} PROPERTIES UNITY_BUILD ON)

  set_target_properties(${target} PROPERTIES
    CXX_SCAN_FOR_MODULES ON)

  message(STATUS "Properties applied for : ${target}")

endfunction()