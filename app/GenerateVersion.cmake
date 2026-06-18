
# Regenerates version.h (git short SHA + build date) for the in-app version
# stamp. Run as a build step via a custom target so the stamp is refreshed on
# every build and can never go stale, in CI or locally. To avoid needless
# recompiles it only rewrites version.h when the contents actually change.
#
# Expects: IN_FILE, OUT_FILE, SOURCE_DIR.

find_package(Git QUIET)

if(Git_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE PYRELITE_GIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

if(NOT PYRELITE_GIT_SHA)
    set(PYRELITE_GIT_SHA "unknown")
endif()

string(TIMESTAMP PYRELITE_BUILD_DATE "%Y-%m-%d" UTC)

configure_file(${IN_FILE} ${OUT_FILE}.tmp @ONLY)

# Swap in the new header only if it differs, so unchanged builds don't force a
# recompile of everything that includes version.h.
if(EXISTS ${OUT_FILE})
    file(READ ${OUT_FILE} _existing)
    file(READ ${OUT_FILE}.tmp _generated)
    if(_existing STREQUAL _generated)
        file(REMOVE ${OUT_FILE}.tmp)
        return()
    endif()
endif()

file(RENAME ${OUT_FILE}.tmp ${OUT_FILE})
