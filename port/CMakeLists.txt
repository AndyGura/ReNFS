cmake_minimum_required(VERSION 3.16)
project(renfs C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)    # for editors: port/build/compile_commands.json
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -O2)

# SDL2, linked dynamically (the MiSTer's hybrid framework is built around SDL2)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

add_executable(renfs
  src/main.c
  src/files.c
  src/unpack.c
  src/fsh.c
  src/video.c
  src/fe.c
  src/audio.c
  src/eacs.c
  src/tgv.c
  src/movie.c
  src/sound.c
  src/config.c
  src/font.c
  src/menu.c
  src/mainmenu.c
  src/options.c
  src/raster.c
  src/poly.c
  src/road.c src/roaddraw.c src/sky.c src/view.c src/car.c src/camera.c src/dash.c src/race.c
  src/model.c src/cardraw.c src/polyfill.c
  src/track.c
  src/exe.c
  src/trackinfo.c
  src/racepal.c
  src/trackfam.c
)
target_include_directories(renfs PRIVATE ${SDL2_INCLUDE_DIRS})
target_compile_options(renfs PRIVATE ${SDL2_CFLAGS_OTHER})
target_link_libraries(renfs PRIVATE ${SDL2_LIBRARIES})
