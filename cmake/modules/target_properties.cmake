function(set_vyroda_target_properties target)

  #set_target_properties(${target} PROPERTIES UNITY_BUILD ON)

  set_target_properties(${target} PROPERTIES
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON)

  target_compile_features(${target} PUBLIC cxx_std_26)

  message(STATUS "Properties applied for : ${target}")

endfunction()