macro(add_poco)
    set(BUILD_SHARED_LIBS ON)
    set(ENABLE_FOUNDATION ON)
    set(ENABLE_FOUNDATION ON)
    set(ENABLE_ENCODINGS OFF)
    set(ENABLE_ENCODINGS_COMPILER OFF)
    set(ENABLE_XML OFF)
    set(ENABLE_JWT OFF)
    set(ENABLE_JSON OFF)
    set(ENABLE_MONGODB OFF)
    set(ENABLE_DATA OFF)
    set(ENABLE_DATA_SQLITE OFF)
    set(ENABLE_DATA_ODBC OFF)
    set(ENABLE_REDIS OFF)
    set(ENABLE_PDF OFF)
    set(ENABLE_UTIL OFF)
    set(ENABLE_NET OFF)
    set(ENABLE_CRYPTO OFF)
    set(ENABLE_NETSSL OFF)
    set(ENABLE_NETSSL_WIN OFF)
    set(ENABLE_SEVENZIP OFF)
    set(ENABLE_ZIP OFF)
    set(ENABLE_CPPPARSER OFF)
    set(ENABLE_POCODOC OFF)
    set(ENABLE_PAGECOMPILER OFF)
    set(ENABLE_PAGECOMPILER_FILE2PAGE OFF)
    include_directories(poco/Foundation/include)
    add_subdirectory(poco)
endmacro()