include(FetchContent)

function(ccgen_fetchcontent_makeavailable name)
  string(TOLOWER "${name}" lower_name)
  FetchContent_GetProperties(${name})
  if(NOT ${lower_name}_POPULATED)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_TESTING OFF)

    FetchContent_MakeAvailable(${name})

    FetchContent_GetProperties(${name})
    set(${name}_SOURCE_DIR
        ${${lower_name}_SOURCE_DIR}
        PARENT_SCOPE)
    set(${lower_name}_SOURCE_DIR
        ${${lower_name}_SOURCE_DIR}
        PARENT_SCOPE)
    set(${name}_BINARY_DIR
        ${${lower_name}_BINARY_DIR}
        PARENT_SCOPE)
    set(${lower_name}_BINARY_DIR
        ${${lower_name}_BINARY_DIR}
        PARENT_SCOPE)
    set(${name}_POPULATED
        ${${lower_name}_POPULATED}
        PARENT_SCOPE)
    set(${lower_name}_POPULATED
        ${${lower_name}_POPULATED}
        PARENT_SCOPE)
  endif()
endfunction()
