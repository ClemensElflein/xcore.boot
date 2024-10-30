add_library(SHA256 OBJECT
        sha256.c
)
target_include_directories(SHA256 PUBLIC .)