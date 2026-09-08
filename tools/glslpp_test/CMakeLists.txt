#
# glslpp_test — projet autonome, indépendant du build du moteur.
#
# Il ne dépend que de SDL2 (déjà installé via vcpkg pour DOOM 64 EX II).
# Se configure et se compile dans son propre dossier build, sans toucher
# à celui du moteur.
#
cmake_minimum_required(VERSION 3.16)
project(glslpp_test CXX)

find_package(SDL2 CONFIG REQUIRED)

add_executable(glslpp_test main.cc)

set_target_properties(glslpp_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)

if(TARGET SDL2::SDL2)
    target_link_libraries(glslpp_test PRIVATE SDL2::SDL2)
else()
    target_include_directories(glslpp_test PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(glslpp_test PRIVATE ${SDL2_LIBRARIES})
endif()

if(MSVC)
    # /utf-8 : le fichier source contient des accents dans les commentaires.
    target_compile_options(glslpp_test PRIVATE /utf-8)
endif()
