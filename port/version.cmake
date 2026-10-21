# writes renfs_version.h (RENFS_VERSION) from tools/version.sh; run at every build,
# and the file is only touched when the version has changed
if(DEFINED ENV{RENFS_VERSION_IS})
  set(V "$ENV{RENFS_VERSION_IS}")       # a build container without git: worked out outside it
else()
  execute_process(COMMAND sh ${SRC}/../tools/version.sh OUTPUT_VARIABLE V OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
if(NOT V)
  set(V "unknown")
endif()
set(TEXT "#define RENFS_VERSION \"${V}\"\n")
set(OLD "")
if(EXISTS ${OUT})
  file(READ ${OUT} OLD)
endif()
if(NOT OLD STREQUAL TEXT)
  file(WRITE ${OUT} "${TEXT}")
endif()
