include(FetchContent)

function(ccgen_fetchcontent_makeavailable name)
  FetchContent_GetProperties(${name})
  if(NOT ${name}_POPULATED)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)

    FetchContent_MakeAvailable(${name})

    FetchContent_GetProperties(${name})
    set(${name}_SOURCE_DIR
        ${${name}_SOURCE_DIR}
        PARENT_SCOPE)
    set(${name}_BINARY_DIR
        ${${name}_BINARY_DIR}
        PARENT_SCOPE)
    set(${name}_POPULATED
        ${${name}_POPULATED}
        PARENT_SCOPE)
  endif()
endfunction()
